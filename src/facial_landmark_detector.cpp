/****
Copyright (c) 2020-2021 Adrian I. Lam

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

#include <cinttypes>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

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

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(m_cfg.osfPort);
    addr.sin_addr.s_addr = inet_addr(m_cfg.osfIpAddress.c_str());

    m_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_sock < 0)
    {
        throw std::runtime_error("Cannot create UDP socket");
    }

    int ret = bind(m_sock, (struct sockaddr *)&addr, sizeof addr);
    if (ret != 0)
    {
        throw std::runtime_error("Cannot bind socket");
    }
}

FacialLandmarkDetector::~FacialLandmarkDetector()
{
    close(m_sock);
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
    bool sync = !m_cfg.winkEnable;

    if (m_cfg.winkEnable)
    {
        if (rightEye < 0.1 && leftEye > 0.2)
        {
            leftEye = 1;
            rightEye = 0;
        }
        else if (leftEye < 0.1 && rightEye > 0.2)
        {
            leftEye = 0;
            rightEye = 1;
        }
        else
        {
            sync = true;
        }
    }

    if (sync)
    {
        // Combine the two to get better synchronized blinks
        double bothEyes = (leftEye + rightEye) / 2;
        leftEye = bothEyes;
        rightEye = bothEyes;
    }

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
        // Read UDP packet from OSF
        static const int nPoints = 68;
        static const int packetFrameSize = 8 + 4 + 2 * 4 + 2 * 4 + 1 + 4 + 3 * 4 + 3 * 4
                                         + 4 * 4 + 4 * 68 + 4 * 2 * 68 + 4 * 3 * 70 + 4 * 14;

        static const int landmarksOffset = 8 + 4 + 2 * 4 + 2 * 4 + 1 + 4 + 3 * 4 + 3 * 4
                                         + 4 * 4 + 4 * 68;

        uint8_t buf[packetFrameSize];
        ssize_t recvSize = recv(m_sock, buf, sizeof buf, 0);

        if (recvSize != packetFrameSize) continue;
        // Note: This is dependent on endianness, and we would assume that
        // the OSF instance is run on a machine with the same endianness
        // as our current machine.
        int recvFaceId = *(int *)(buf + 8);
        if (recvFaceId != m_faceId) continue; // We only support one face

        Point landmarks[nPoints];

        for (int i = 0; i < nPoints; i++)
        {
            float x = *(float *)(buf + landmarksOffset + i * 2 * sizeof(float));
            float y = *(float *)(buf + landmarksOffset + (i * 2 + 1) * sizeof(float));

            landmarks[i].x = x;
            landmarks[i].y = y;
        }

        /* The coordinates seem to be rather noisy in general.
         * We will push everything through some moving average filters
         * to reduce noise. The number of taps is determined empirically
         * until we get something good.
         * An alternative method would be to get some better dataset -
         * perhaps even to train on a custom data set just for the user.
         */

        // Face rotation: X direction (left-right)
        double faceXRot = calcFaceXAngle(landmarks);
        filterPush(m_faceXAngle, faceXRot, m_cfg.faceXAngleNumTaps);

        // Mouth form (smile / laugh) detection
        double mouthForm = calcMouthForm(landmarks);
        filterPush(m_mouthForm, mouthForm, m_cfg.mouthFormNumTaps);

        // Face rotation: Y direction (up-down)
        double faceYRot = calcFaceYAngle(landmarks, faceXRot, mouthForm);
        filterPush(m_faceYAngle, faceYRot, m_cfg.faceYAngleNumTaps);

        // Face rotation: Z direction (head tilt)
        double faceZRot = calcFaceZAngle(landmarks);
        filterPush(m_faceZAngle, faceZRot, m_cfg.faceZAngleNumTaps);

        // Mouth openness
        double mouthOpen = calcMouthOpenness(landmarks, mouthForm);
        filterPush(m_mouthOpenness, mouthOpen, m_cfg.mouthOpenNumTaps);

        // Eye openness
        double eyeLeftOpen = calcEyeOpenness(LEFT, landmarks, faceYRot);
        filterPush(m_leftEyeOpenness, eyeLeftOpen, m_cfg.leftEyeOpenNumTaps);
        double eyeRightOpen = calcEyeOpenness(RIGHT, landmarks, faceYRot);
        filterPush(m_rightEyeOpenness, eyeRightOpen, m_cfg.rightEyeOpenNumTaps);

        // Eyebrows: the landmark detection doesn't work very well for my face,
        // so I've not implemented them.
    }
}

double FacialLandmarkDetector::calcEyeAspectRatio(
    Point& p1, Point& p2,
    Point& p3, Point& p4,
    Point& p5, Point& p6) const
{
    double eyeWidth = dist(p1, p4);
    double eyeHeight1 = dist(p2, p6);
    double eyeHeight2 = dist(p3, p5);

    return (eyeHeight1 + eyeHeight2) / (2 * eyeWidth);
}

double FacialLandmarkDetector::calcEyeOpenness(
    LeftRight eye,
    Point landmarks[],
    double faceYAngle) const
{
    double eyeAspectRatio;
    if (eye == LEFT)
    {
        eyeAspectRatio = calcEyeAspectRatio(landmarks[42], landmarks[43], landmarks[44],
                                            landmarks[45], landmarks[46], landmarks[47]);
    }
    else
    {
        eyeAspectRatio = calcEyeAspectRatio(landmarks[36], landmarks[37], landmarks[38],
                                            landmarks[39], landmarks[40], landmarks[41]);
    }

    // Apply correction due to faceYAngle
    double corrEyeAspRat = eyeAspectRatio / std::cos(degToRad(faceYAngle));

    return linearScale01(corrEyeAspRat, m_cfg.eyeClosedThreshold, m_cfg.eyeOpenThreshold);
}



double FacialLandmarkDetector::calcMouthForm(Point landmarks[]) const
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

    auto eye1 = centroid(landmarks[36], landmarks[37], landmarks[38],
                         landmarks[39], landmarks[40], landmarks[41]);
    auto eye2 = centroid(landmarks[42], landmarks[43], landmarks[44],
                         landmarks[45], landmarks[46], landmarks[47]);
    double distEyes = dist(eye1, eye2);
    double distMouth = dist(landmarks[58], landmarks[62]);

    double form = linearScale01(distMouth / distEyes,
                                m_cfg.mouthNormalThreshold,
                                m_cfg.mouthSmileThreshold);

    return form;
}

double FacialLandmarkDetector::calcMouthOpenness(
    Point landmarks[],
    double mouthForm) const
{
    // Use points for the bottom of the upper lip, and top of the lower lip
    // We have 3 pairs of points available, which give the mouth height
    // on the left, in the middle, and on the right, resp.
    // First let's try to use an average of all three.
    double heightLeft   = dist(landmarks[61], landmarks[63]);
    double heightMiddle = dist(landmarks[60], landmarks[64]);
    double heightRight  = dist(landmarks[59], landmarks[65]);

    double avgHeight = (heightLeft + heightMiddle + heightRight) / 3;

    // Now, normalize it with the width of the mouth.
    double width = dist(landmarks[58], landmarks[62]);

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

double FacialLandmarkDetector::calcFaceXAngle(Point landmarks[]) const
{
    // This function will be easier to understand if you refer to the
    // diagram in faceXAngle.png

    // Construct the y-axis using (1) average of four points on the nose and
    // (2) average of five points on the upper lip.

    auto y0 = centroid(landmarks[27], landmarks[28], landmarks[29],
                       landmarks[30]);
    auto y1 = centroid(landmarks[48], landmarks[49], landmarks[50],
                       landmarks[51], landmarks[52]);

    // Now drop a perpedicular from the left and right edges of the face,
    // and calculate the ratio between the lengths of these perpendiculars

    auto left = centroid(landmarks[14], landmarks[15], landmarks[16]);
    auto right = centroid(landmarks[0], landmarks[1], landmarks[2]);

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

double FacialLandmarkDetector::calcFaceYAngle(Point landmarks[], double faceXAngle, double mouthForm) const
{
    // Use the nose
    // angle between the two left/right points and the tip
    double c = dist(landmarks[31], landmarks[35]);
    double a = dist(landmarks[30], landmarks[31]);
    double b = dist(landmarks[30], landmarks[35]);

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

double FacialLandmarkDetector::calcFaceZAngle(Point landmarks[]) const
{
    // Use average of eyes and nose

    auto eyeRight = centroid(landmarks[36], landmarks[37], landmarks[38],
                             landmarks[39], landmarks[40], landmarks[41]);
    auto eyeLeft  = centroid(landmarks[42], landmarks[43], landmarks[44],
                             landmarks[45], landmarks[46], landmarks[47]);

    auto noseLeft  = landmarks[35];
    auto noseRight = landmarks[31];

    double eyeYDiff = eyeRight.y - eyeLeft.y;
    double eyeXDiff = eyeRight.x - eyeLeft.x;

    double angle1 = std::atan(eyeYDiff / eyeXDiff);

    double noseYDiff = noseRight.y - noseLeft.y;
    double noseXDiff = noseRight.x - noseLeft.x;

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
                if (paramName == "osfIpAddress")
                {
                    if (!(ss >> m_cfg.osfIpAddress))
                    {
                        throwConfigError(paramName, "std::string",
                                         line, lineNum);
                    }
                }
                else if (paramName == "osfPort")
                {
                    if (!(ss >> m_cfg.osfPort))
                    {
                        throwConfigError(paramName, "int",
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
                else if (paramName == "winkEnable")
                {
                    if (!(ss >> m_cfg.winkEnable))
                    {
                        throwConfigError(paramName, "bool",
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

    m_cfg.faceYAngleCorrection = 10;
    m_cfg.eyeSmileEyeOpenThreshold = 0.6;
    m_cfg.eyeSmileMouthFormThreshold = 0.75;
    m_cfg.eyeSmileMouthOpenThreshold = 0.5;
    m_cfg.faceXAngleNumTaps = 7;
    m_cfg.faceYAngleNumTaps = 7;
    m_cfg.faceZAngleNumTaps = 7;
    m_cfg.mouthFormNumTaps = 3;
    m_cfg.mouthOpenNumTaps = 3;
    m_cfg.leftEyeOpenNumTaps = 3;
    m_cfg.rightEyeOpenNumTaps = 3;
    m_cfg.eyeClosedThreshold = 0.18;
    m_cfg.eyeOpenThreshold = 0.21;
    m_cfg.winkEnable = true;
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

