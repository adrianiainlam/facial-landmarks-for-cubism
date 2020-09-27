// -*- mode: c++ -*-

#ifndef __FACIAL_LANDMARK_DETECTOR_H__
#define __FACIAL_LANDMARK_DETECTOR_H__

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

#include <deque>
#include <string>
#include <opencv2/opencv.hpp>
#include <dlib/image_processing/frontal_face_detector.h>
#include <dlib/image_processing.h>
#include <dlib/gui_widgets.h>

class FacialLandmarkDetector
{
public:
    struct Params
    {
        double leftEyeOpenness;
        double rightEyeOpenness;
        double leftEyeSmile;
        double rightEyeSmile;
        double mouthOpenness;
        double mouthForm;
        double faceXAngle;
        double faceYAngle;
        double faceZAngle;
        bool autoBlink;
        bool autoBreath;
        bool randomMotion;
        // TODO eyebrows currently not supported...
        // I'd like to include them, but the dlib detection is very
        // noisy and inaccurate (at least for my face).
    };

    FacialLandmarkDetector(std::string cfgPath);

    Params getParams(void) const;

    void stop(void);

    void mainLoop(void);

private:
    enum LeftRight : bool
    {
        LEFT,
        RIGHT
    };

    cv::VideoCapture webcam;
    dlib::image_window win;
    dlib::frontal_face_detector detector;
    dlib::shape_predictor predictor;
    bool m_stop;

    double calcEyeAspectRatio(dlib::point& p1, dlib::point& p2,
                              dlib::point& p3, dlib::point& p4,
                              dlib::point& p5, dlib::point& p6) const;

    double calcRightEyeAspectRatio(dlib::full_object_detection& shape) const;
    double calcLeftEyeAspectRatio(dlib::full_object_detection& shape) const;

    double calcEyeOpenness(LeftRight eye,
                           dlib::full_object_detection& shape,
                           double faceYAngle) const;

    double calcMouthForm(dlib::full_object_detection& shape) const;
    double calcMouthOpenness(dlib::full_object_detection& shape, double mouthForm) const;

    double calcFaceXAngle(dlib::full_object_detection& shape) const;
    double calcFaceYAngle(dlib::full_object_detection& shape, double faceXAngle, double mouthForm) const;
    double calcFaceZAngle(dlib::full_object_detection& shape) const;

    void populateDefaultConfig(void);
    void parseConfig(std::string cfgPath);
    void throwConfigError(std::string paramName, std::string expectedType,
                          std::string line, unsigned int lineNum);


    std::deque<double> m_leftEyeOpenness;
    std::deque<double> m_rightEyeOpenness;

    std::deque<double> m_mouthOpenness;
    std::deque<double> m_mouthForm;

    std::deque<double> m_faceXAngle;
    std::deque<double> m_faceYAngle;
    std::deque<double> m_faceZAngle;

    struct Config
    {
        int cvVideoCaptureId;
        std::string predictorPath;
        double faceYAngleCorrection;
        double eyeSmileEyeOpenThreshold;
        double eyeSmileMouthFormThreshold;
        double eyeSmileMouthOpenThreshold;
        bool showWebcamVideo;
        bool renderLandmarksOnVideo;
        bool lateralInversion;
        std::size_t faceXAngleNumTaps;
        std::size_t faceYAngleNumTaps;
        std::size_t faceZAngleNumTaps;
        std::size_t mouthFormNumTaps;
        std::size_t mouthOpenNumTaps;
        std::size_t leftEyeOpenNumTaps;
        std::size_t rightEyeOpenNumTaps;
        int cvWaitKeyMs;
        double eyeClosedThreshold;
        double eyeOpenThreshold;
        double mouthNormalThreshold;
        double mouthSmileThreshold;
        double mouthClosedThreshold;
        double mouthOpenThreshold;
        double mouthOpenLaughCorrection;
        double faceYAngleXRotCorrection;
        double faceYAngleSmileCorrection;
        double faceYAngleZeroValue;
        double faceYAngleUpThreshold;
        double faceYAngleDownThreshold;
        bool autoBlink;
        bool autoBreath;
        bool randomMotion;
    } m_cfg;
};

#endif

