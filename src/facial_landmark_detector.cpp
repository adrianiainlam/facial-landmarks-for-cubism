/****
Copyright (c) 2020 Adrian I. Lam

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
****/

#include <stdexcept>
#include <fstream>
#include <string>
#include <sstream>
#include <cmath>

#include <opencv2/opencv.hpp>

#include <dlib/opencv.h>
#include <dlib/image_processing/frontal_face_detector.h>
#include <dlib/image_processing.h>
#include <dlib/image_processing/render_face_detections.h>

#include "facial_landmark_detector.h"
#include "math_utils.h"


static void filterPush(std::deque<double>& buf, double newval,
                       std::size_t numTaps)
{
    buf.push_back(newval);
    while (buf.size() > numTaps)
    {
        buf.pop_front();
    }
}

FacialLandmarkDetector::FacialLandmarkDetector(std::string cfgPath)
    : m_stop(false)
{
    parseConfig(cfgPath);

    if (!webcam.open(m_cfg.cvVideoCaptureId))
    {
        throw std::runtime_error("Unable to open webcam");
    }

    detector = dlib::get_frontal_face_detector();
    dlib::deserialize(m_cfg.predictorPath) >> predictor;
}

FacialLandmarkDetector::Params FacialLandmarkDetector::getParams(void) const
{
    Params params;

    params.faceXAngle = avg(m_faceXAngle);
    params.faceYAngle = avg(m_faceYAngle) + m_cfg.faceYAngleCorrection;
    // + 10 correct for angle between computer monitor and webcam
    params.faceZAngle = avg(m_faceZAngle);
    params.mouthOpenness = avg(m_mouthOpenness);
    params.mouthForm = avg(m_mouthForm);

    double leftEye = avg(m_leftEyeOpenness, 1);
    double rightEye = avg(m_rightEyeOpenness, 1);
    // Just combine the two to get better synchronized blinks
    // This effectively disables winks, so if we want to
    // support winks in the future (see below) we will need
    // a better way to handle this out-of-sync blinks.
    double bothEyes = (leftEye + rightEye) / 2;
    leftEye = bothEyes;
    rightEye = bothEyes;
    // Detect winks and make them look better
    // Commenting out - winks are difficult to be detected by the
    // dlib data set anyway... maybe in the future we can
    // add a runtime option to enable/disable...
    /*if (right == 0 && left > 0.2)
    {
        left = 1;
    }
    else if (left == 0 && right > 0.2)
    {
        right = 1;
    }
    */
    params.leftEyeOpenness = leftEye;
    params.rightEyeOpenness = rightEye;

    if (leftEye <= m_cfg.eyeSmileEyeOpenThreshold &&
        rightEye <= m_cfg.eyeSmileEyeOpenThreshold &&
        params.mouthForm > m_cfg.eyeSmileMouthFormThreshold &&
        params.mouthOpenness > m_cfg.eyeSmileMouthOpenThreshold)
    {
        params.leftEyeSmile = 1;
        params.rightEyeSmile = 1;
    }
    else
    {
        params.leftEyeSmile = 0;
        params.rightEyeSmile = 0;
    }

    params.autoBlink = m_cfg.autoBlink;
    params.autoBreath = m_cfg.autoBreath;
    params.randomMotion = m_cfg.randomMotion;

    return params;
}

void FacialLandmarkDetector::stop(void)
{
    m_stop = true;
}

void FacialLandmarkDetector::mainLoop(void)
{
    while (!m_stop)
    {
        cv::Mat frame;
        if (!webcam.read(frame))
        {
            throw std::runtime_error("Unable to read from webcam");
        }
        cv::Mat flipped;
        if (m_cfg.lateralInversion)
        {
            cv::flip(frame, flipped, 1);
        }
        else
        {
            flipped = frame;
        }
        dlib::cv_image<dlib::bgr_pixel> cimg(flipped);

        if (m_cfg.showWebcamVideo)
        {
            win.set_image(cimg);
        }

        std::vector<dlib::rectangle> faces = detector(cimg);

        if (faces.size() > 0)
        {
            dlib::rectangle face = faces[0];
            dlib::full_object_detection shape = predictor(cimg, face);

            /* The coordinates seem to be rather noisy in general.
             * We will push everything through some moving average filters
             * to reduce noise. The number of taps is determined empirically
             * until we get something good.
             * An alternative method would be to get some better dataset
             * for dlib - perhaps even to train on a custom data set just for the user.
             */

            // Face rotation: X direction (left-right)
            double faceXRot = calcFaceXAngle(shape);
            filterPush(m_faceXAngle, faceXRot, m_cfg.faceXAngleNumTaps);

            // Mouth form (smile / laugh) detection
            double mouthForm = calcMouthForm(shape);
            filterPush(m_mouthForm, mouthForm, m_cfg.mouthFormNumTaps);

            // Face rotation: Y direction (up-down)
            double faceYRot = calcFaceYAngle(shape, faceXRot, mouthForm);
            filterPush(m_faceYAngle, faceYRot, m_cfg.faceYAngleNumTaps);

            // Face rotation: Z direction (head tilt)
            double faceZRot = calcFaceZAngle(shape);
            filterPush(m_faceZAngle, faceZRot, m_cfg.faceZAngleNumTaps);

            // Mouth openness
            double mouthOpen = calcMouthOpenness(shape, mouthForm);
            filterPush(m_mouthOpenness, mouthOpen, m_cfg.mouthOpenNumTaps);

            // Eye openness
            double eyeLeftOpen = calcEyeOpenness(LEFT, shape, faceYRot);
            filterPush(m_leftEyeOpenness, eyeLeftOpen, m_cfg.leftEyeOpenNumTaps);
            double eyeRightOpen = calcEyeOpenness(RIGHT, shape, faceYRot);
            filterPush(m_rightEyeOpenness, eyeRightOpen, m_cfg.rightEyeOpenNumTaps);

            // TODO eyebrows?

            if (m_cfg.showWebcamVideo && m_cfg.renderLandmarksOnVideo)
            {
                win.clear_overlay();
                win.add_overlay(dlib::render_face_detections(shape));
            }
        }
        else
        {
            if (m_cfg.showWebcamVideo && m_cfg.renderLandmarksOnVideo)
            {
                win.clear_overlay();
            }
        }

        cv::waitKey(m_cfg.cvWaitKeyMs);
    }
}

double FacialLandmarkDetector::calcEyeAspectRatio(
    dlib::point& p1, dlib::point& p2,
    dlib::point& p3, dlib::point& p4,
    dlib::point& p5, dlib::point& p6) const
{
    double eyeWidth = dist(p1, p4);
    double eyeHeight1 = dist(p2, p6);
    double eyeHeight2 = dist(p3, p5);

    return (eyeHeight1 + eyeHeight2) / (2 * eyeWidth);
}

double FacialLandmarkDetector::calcEyeOpenness(
    LeftRight eye,
    dlib::full_object_detection& shape,
    double faceYAngle) const
{
    double eyeAspectRatio;
    if (eye == LEFT)
    {
        eyeAspectRatio = calcEyeAspectRatio(shape.part(42), shape.part(43), shape.part(44),
                                            shape.part(45), shape.part(46), shape.part(47));
    }
    else
    {
        eyeAspectRatio = calcEyeAspectRatio(shape.part(36), shape.part(37), shape.part(38),
                                            shape.part(39), shape.part(40), shape.part(41));
    }

    // Apply correction due to faceYAngle
    double corrEyeAspRat = eyeAspectRatio / std::cos(degToRad(faceYAngle));

    return linearScale01(corrEyeAspRat, m_cfg.eyeClosedThreshold, m_cfg.eyeOpenThreshold);
}



double FacialLandmarkDetector::calcMouthForm(dlib::full_object_detection& shape) const
{
    /* Mouth form parameter: 0 for normal mouth, 1 for fully smiling / laughing.
     * Compare distance between the two corners of the mouth
     * to the distance between the two eyes.
     */

    /* An alternative (my initial attempt) was to compare the corners of
     * the mouth to the top of the upper lip - they almost lie on a
     * straight line when smiling / laughing. But that is only true
     * when facing straight at the camera. When looking up / down,
     * the angle changes. So here we'll use the distance approach instead.
     */

    auto eye1 = centroid(shape.part(36), shape.part(37), shape.part(38),
                         shape.part(39), shape.part(40), shape.part(41));
    auto eye2 = centroid(shape.part(42), shape.part(43), shape.part(44),
                         shape.part(45), shape.part(46), shape.part(47));
    double distEyes = dist(eye1, eye2);
    double distMouth = dist(shape.part(48), shape.part(54));

    double form = linearScale01(distMouth / distEyes,
                                m_cfg.mouthNormalThreshold,
                                m_cfg.mouthSmileThreshold);

    return form;
}

double FacialLandmarkDetector::calcMouthOpenness(
    dlib::full_object_detection& shape,
    double mouthForm) const
{
    // Use points for the bottom of the upper lip, and top of the lower lip
    // We have 3 pairs of points available, which give the mouth height
    // on the left, in the middle, and on the right, resp.
    // First let's try to use an average of all three.
    double heightLeft = dist(shape.part(63), shape.part(65));
    double heightMiddle = dist(shape.part(62), shape.part(66));
    double heightRight = dist(shape.part(61), shape.part(67));

    double avgHeight = (heightLeft + heightMiddle + heightRight) / 3;

    // Now, normalize it with the width of the mouth.
    double width = dist(shape.part(60), shape.part(64));

    double normalized = avgHeight / width;

    double scaled = linearScale01(normalized,
                                  m_cfg.mouthClosedThreshold,
                                  m_cfg.mouthOpenThreshold,
                                  true, false);

    // Apply correction according to mouthForm
    // Notice that when you smile / laugh, width is increased
    scaled *= (1 + m_cfg.mouthOpenLaughCorrection * mouthForm);

    return scaled;
}

double FacialLandmarkDetector::calcFaceXAngle(dlib::full_object_detection& shape) const
{
    // This function will be easier to understand if you refer to the
    // diagram in faceXAngle.png

    // Construct the y-axis using (1) average of four points on the nose and
    // (2) average of four points on the upper lip.

    auto y0 = centroid(shape.part(27), shape.part(28), shape.part(29),
                       shape.part(30));
    auto y1 = centroid(shape.part(50), shape.part(51), shape.part(52),
                       shape.part(62));

    // Now drop a perpedicular from the left and right edges of the face,
    // and calculate the ratio between the lengths of these perpendiculars

    auto left = centroid(shape.part(14), shape.part(15), shape.part(16));
    auto right = centroid(shape.part(0), shape.part(1), shape.part(2));

    // Constructing a perpendicular:
    // Join the left/right point and the upper lip. The included angle
    // can now be determined using cosine rule.
    // Then sine of this angle is the perpendicular divided by the newly
    // created line.
    double opp = dist(right, y0);
    double adj1 = dist(y0, y1);
    double adj2 = dist(y1, right);
    double angle = solveCosineRuleAngle(opp, adj1, adj2);
    double perpRight = adj2 * std::sin(angle);

    opp = dist(left, y0);
    adj2 = dist(y1, left);
    angle = solveCosineRuleAngle(opp, adj1, adj2);
    double perpLeft = adj2 * std::sin(angle);

    // Model the head as a sphere and look from above.
    double theta = std::asin((perpRight - perpLeft) / (perpRight + perpLeft));

    theta = radToDeg(theta);
    if (theta < -30) theta = -30;
    if (theta > 30) theta = 30;
    return theta;
}

double FacialLandmarkDetector::calcFaceYAngle(dlib::full_object_detection& shape, double faceXAngle, double mouthForm) const
{
    // Use the nose
    // angle between the two left/right points and the tip
    double c = dist(shape.part(31), shape.part(35));
    double a = dist(shape.part(30), shape.part(31));
    double b = dist(shape.part(30), shape.part(35));

    double angle = solveCosineRuleAngle(c, a, b);

    // This probably varies a lot from person to person...

    // Best is probably to work out some trigonometry again,
    // but just linear interpolation seems to work ok...

    // Correct for X rotation
    double corrAngle = angle * (1 + (std::abs(faceXAngle) / 30
                                     * m_cfg.faceYAngleXRotCorrection));

    // Correct for smiles / laughs - this increases the angle
    corrAngle *= (1 - mouthForm * m_cfg.faceYAngleSmileCorrection);

    if (corrAngle >= m_cfg.faceYAngleZeroValue)
    {
        return -30 * linearScale01(corrAngle,
                                   m_cfg.faceYAngleZeroValue,
                                   m_cfg.faceYAngleDownThreshold,
                                   false, false);
    }
    else
    {
        return 30 * (1 - linearScale01(corrAngle,
                                       m_cfg.faceYAngleUpThreshold,
                                       m_cfg.faceYAngleZeroValue,
                                       false, false));
    }
}

double FacialLandmarkDetector::calcFaceZAngle(dlib::full_object_detection& shape) const
{
    // Use average of eyes and nose

    auto eyeRight = centroid(shape.part(36), shape.part(37), shape.part(38),
                             shape.part(39), shape.part(40), shape.part(41));
    auto eyeLeft = centroid(shape.part(42), shape.part(43), shape.part(44),
                            shape.part(45), shape.part(46), shape.part(47));

    auto noseLeft = shape.part(35);
    auto noseRight = shape.part(31);

    double eyeYDiff = eyeRight.y() - eyeLeft.y();
    double eyeXDiff = eyeRight.x() - eyeLeft.x();

    double angle1 = std::atan(eyeYDiff / eyeXDiff);

    double noseYDiff = noseRight.y() - noseLeft.y();
    double noseXDiff = noseRight.x() - noseLeft.x();

    double angle2 = std::atan(noseYDiff / noseXDiff);

    return radToDeg((angle1 + angle2) / 2);
}

void FacialLandmarkDetector::parseConfig(std::string cfgPath)
{
    populateDefaultConfig();
    if (cfgPath != "")
    {
        std::ifstream file(cfgPath);

        if (!file)
        {
            throw std::runtime_error("Failed to open config file");
        }

        std::string line;
        unsigned int lineNum = 0;

        while (std::getline(file, line))
        {
            lineNum++;

            if (line[0] == '#')
            {
                continue;
            }

            std::istringstream ss(line);
            std::string paramName;
            if (ss >> paramName)
            {
                if (paramName == "cvVideoCaptureId")
                {
                    if (!(ss >> m_cfg.cvVideoCaptureId))
                    {
                        throwConfigError(paramName, "int",
                                         line, lineNum);
                    }
                }
                else if (paramName == "predictorPath")
                {
                    if (!(ss >> m_cfg.predictorPath))
                    {
                        throwConfigError(paramName, "std::string",
                                         line, lineNum);
                    }
                }
                else if (paramName == "faceYAngleCorrection")
                {
                    if (!(ss >> m_cfg.faceYAngleCorrection))
                    {
                        throwConfigError(paramName, "double",
                                         line, lineNum);
                    }
                }
                else if (paramName == "eyeSmileEyeOpenThreshold")
                {
                    if (!(ss >> m_cfg.eyeSmileEyeOpenThreshold))
                    {
                        throwConfigError(paramName, "double",
                                         line, lineNum);
                    }
                }
                else if (paramName == "eyeSmileMouthFormThreshold")
                {
                    if (!(ss >> m_cfg.eyeSmileMouthFormThreshold))
                    {
                        throwConfigError(paramName, "double",
                                         line, lineNum);
                    }
                }
                else if (paramName == "eyeSmileMouthOpenThreshold")
                {
                    if (!(ss >> m_cfg.eyeSmileMouthOpenThreshold))
                    {
                        throwConfigError(paramName, "double",
                                         line, lineNum);
                    }
                }
                else if (paramName == "showWebcamVideo")
                {
                    if (!(ss >> m_cfg.showWebcamVideo))
                    {
                        throwConfigError(paramName, "bool",
                                         line, lineNum);
                    }
                }
                else if (paramName == "renderLandmarksOnVideo")
                {
                    if (!(ss >> m_cfg.renderLandmarksOnVideo))
                    {
                        throwConfigError(paramName, "bool",
                                         line, lineNum);
                    }
                }
                else if (paramName == "lateralInversion")
                {
                    if (!(ss >> m_cfg.lateralInversion))
                    {
                        throwConfigError(paramName, "bool",
                                         line, lineNum);
                    }
                }
                else if (paramName == "faceXAngleNumTaps")
                {
                    if (!(ss >> m_cfg.faceXAngleNumTaps))
                    {
                        throwConfigError(paramName, "std::size_t",
                                         line, lineNum);
                    }
                }
                else if (paramName == "faceYAngleNumTaps")
                {
                    if (!(ss >> m_cfg.faceYAngleNumTaps))
                    {
                        throwConfigError(paramName, "std::size_t",
                                         line, lineNum);
                    }
                }
                else if (paramName == "faceZAngleNumTaps")
                {
                    if (!(ss >> m_cfg.faceZAngleNumTaps))
                    {
                        throwConfigError(paramName, "std::size_t",
                                         line, lineNum);
                    }
                }
                else if (paramName == "mouthFormNumTaps")
                {
                    if (!(ss >> m_cfg.mouthFormNumTaps))
                    {
                        throwConfigError(paramName, "std::size_t",
                                         line, lineNum);
                    }
                }
                else if (paramName == "mouthOpenNumTaps")
                {
                    if (!(ss >> m_cfg.mouthOpenNumTaps))
                    {
                        throwConfigError(paramName, "std::size_t",
                                         line, lineNum);
                    }
                }
                else if (paramName == "leftEyeOpenNumTaps")
                {
                    if (!(ss >> m_cfg.leftEyeOpenNumTaps))
                    {
                        throwConfigError(paramName, "std::size_t",
                                         line, lineNum);
                    }
                }
                else if (paramName == "rightEyeOpenNumTaps")
                {
                    if (!(ss >> m_cfg.rightEyeOpenNumTaps))
                    {
                        throwConfigError(paramName, "std::size_t",
                                         line, lineNum);
                    }
                }
                else if (paramName == "cvWaitKeyMs")
                {
                    if (!(ss >> m_cfg.cvWaitKeyMs))
                    {
                        throwConfigError(paramName, "int",
                                         line, lineNum);
                    }
                }
                else if (paramName == "eyeClosedThreshold")
                {
                    if (!(ss >> m_cfg.eyeClosedThreshold))
                    {
                        throwConfigError(paramName, "double",
                                         line, lineNum);
                    }
                }
                else if (paramName == "eyeOpenThreshold")
                {
                    if (!(ss >> m_cfg.eyeOpenThreshold))
                    {
                        throwConfigError(paramName, "double",
                                         line, lineNum);
                    }
                }
                else if (paramName == "mouthNormalThreshold")
                {
                    if (!(ss >> m_cfg.mouthNormalThreshold))
                    {
                        throwConfigError(paramName, "double",
                                         line, lineNum);
                    }
                }
                else if (paramName == "mouthSmileThreshold")
                {
                    if (!(ss >> m_cfg.mouthSmileThreshold))
                    {
                        throwConfigError(paramName, "double",
                                         line, lineNum);
                    }
                }
                else if (paramName == "mouthClosedThreshold")
                {
                    if (!(ss >> m_cfg.mouthClosedThreshold))
                    {
                        throwConfigError(paramName, "double",
                                         line, lineNum);
                    }
                }
                else if (paramName == "mouthOpenThreshold")
                {
                    if (!(ss >> m_cfg.mouthOpenThreshold))
                    {
                        throwConfigError(paramName, "double",
                                         line, lineNum);
                    }
                }
                else if (paramName == "mouthOpenLaughCorrection")
                {
                    if (!(ss >> m_cfg.mouthOpenLaughCorrection))
                    {
                        throwConfigError(paramName, "double",
                                         line, lineNum);
                    }
                }
                else if (paramName == "faceYAngleXRotCorrection")
                {
                    if (!(ss >> m_cfg.faceYAngleXRotCorrection))
                    {
                        throwConfigError(paramName, "double",
                                         line, lineNum);
                    }
                }
                else if (paramName == "faceYAngleSmileCorrection")
                {
                    if (!(ss >> m_cfg.faceYAngleSmileCorrection))
                    {
                        throwConfigError(paramName, "double",
                                         line, lineNum);
                    }
                }
                else if (paramName == "faceYAngleZeroValue")
                {
                    if (!(ss >> m_cfg.faceYAngleZeroValue))
                    {
                        throwConfigError(paramName, "double",
                                         line, lineNum);
                    }
                }
                else if (paramName == "faceYAngleUpThreshold")
                {
                    if (!(ss >> m_cfg.faceYAngleUpThreshold))
                    {
                        throwConfigError(paramName, "double",
                                         line, lineNum);
                    }
                }
                else if (paramName == "faceYAngleDownThreshold")
                {
                    if (!(ss >> m_cfg.faceYAngleDownThreshold))
                    {
                        throwConfigError(paramName, "double",
                                         line, lineNum);
                    }
                }
                else if (paramName == "autoBlink")
                {
                    if (!(ss >> m_cfg.autoBlink))
                    {
                        throwConfigError(paramName, "bool",
                                         line, lineNum);
                    }
                }
                else if (paramName == "autoBreath")
                {
                    if (!(ss >> m_cfg.autoBreath))
                    {
                        throwConfigError(paramName, "bool",
                                         line, lineNum);
                    }
                }
                else if (paramName == "randomMotion")
                {
                    if (!(ss >> m_cfg.randomMotion))
                    {
                        throwConfigError(paramName, "bool",
                                         line, lineNum);
                    }
                }
                else
                {
                    std::ostringstream oss;
                    oss << "Unrecognized parameter name at line " << lineNum
                        << ": " << paramName;
                    throw std::runtime_error(oss.str());
                }
            }
        }
    }
}

void FacialLandmarkDetector::populateDefaultConfig(void)
{
    // These are values that I've personally tested to work OK for my face.
    // Your milage may vary - hence the config file.

    m_cfg.cvVideoCaptureId = 0;
    m_cfg.predictorPath = "shape_predictor_68_face_landmarks.dat";
    m_cfg.faceYAngleCorrection = 10;
    m_cfg.eyeSmileEyeOpenThreshold = 0.6;
    m_cfg.eyeSmileMouthFormThreshold = 0.75;
    m_cfg.eyeSmileMouthOpenThreshold = 0.5;
    m_cfg.showWebcamVideo = true;
    m_cfg.renderLandmarksOnVideo = true;
    m_cfg.lateralInversion = true;
    m_cfg.cvWaitKeyMs = 5;
    m_cfg.faceXAngleNumTaps = 11;
    m_cfg.faceYAngleNumTaps = 11;
    m_cfg.faceZAngleNumTaps = 11;
    m_cfg.mouthFormNumTaps = 3;
    m_cfg.mouthOpenNumTaps = 3;
    m_cfg.leftEyeOpenNumTaps = 3;
    m_cfg.rightEyeOpenNumTaps = 3;
    m_cfg.eyeClosedThreshold = 0.2;
    m_cfg.eyeOpenThreshold = 0.25;
    m_cfg.mouthNormalThreshold = 0.75;
    m_cfg.mouthSmileThreshold = 1.0;
    m_cfg.mouthClosedThreshold = 0.1;
    m_cfg.mouthOpenThreshold = 0.4;
    m_cfg.mouthOpenLaughCorrection = 0.2;
    m_cfg.faceYAngleXRotCorrection = 0.15;
    m_cfg.faceYAngleSmileCorrection = 0.075;
    m_cfg.faceYAngleZeroValue = 1.8;
    m_cfg.faceYAngleDownThreshold = 2.3;
    m_cfg.faceYAngleUpThreshold = 1.3;
    m_cfg.autoBlink = false;
    m_cfg.autoBreath = false;
    m_cfg.randomMotion = false;
}

void FacialLandmarkDetector::throwConfigError(std::string paramName,
                                              std::string expectedType,
                                              std::string line,
                                              unsigned int lineNum)
{
    std::ostringstream ss;
    ss << "Error parsing config file for parameter " << paramName
       << "\nAt line " << lineNum << ": " << line
       << "\nExpecting value of type " << expectedType;

    throw std::runtime_error(ss.str());
}

