// -*- mode: c++ -*-

#ifndef FACIAL_LANDMARK_DETECTOR_H
#define FACIAL_LANDMARK_DETECTOR_H

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

#include <deque>
#include <string>

struct Point
{
    double x;
    double y;

    Point(double _x = 0, double _y = 0)
    {
        x = _x;
        y = _y;
    }
};

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
        // I'd like to include them, but the dlib / OSF detection is very
        // noisy and inaccurate (at least for my face).
    };

    FacialLandmarkDetector(std::string cfgPath);
    ~FacialLandmarkDetector();

    Params getParams(void) const;

    void stop(void);

    void mainLoop(void);

private:
    FacialLandmarkDetector(const FacialLandmarkDetector&) = delete;
    FacialLandmarkDetector& operator=(const FacialLandmarkDetector &) = delete;

    enum LeftRight : bool
    {
        LEFT,
        RIGHT
    };

    bool m_stop;

    int m_sock;
    static const int m_faceId = 0; // Only support one face for now

    double calcEyeAspectRatio(Point& p1, Point& p2,
                              Point& p3, Point& p4,
                              Point& p5, Point& p6) const;

    double calcRightEyeAspectRatio(Point landmarks[]) const;
    double calcLeftEyeAspectRatio(Point landmarks[]) const;

    double calcEyeOpenness(LeftRight eye,
                           Point landmarks[],
                           double faceYAngle) const;

    double calcMouthForm(Point landmarks[]) const;
    double calcMouthOpenness(Point landmarks[], double mouthForm) const;

    double calcFaceXAngle(Point landmarks[]) const;
    double calcFaceYAngle(Point landmarks[], double faceXAngle, double mouthForm) const;
    double calcFaceZAngle(Point landmarks[]) const;

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
        std::string osfIpAddress;
        int osfPort;
        double faceYAngleCorrection;
        double eyeSmileEyeOpenThreshold;
        double eyeSmileMouthFormThreshold;
        double eyeSmileMouthOpenThreshold;
        std::size_t faceXAngleNumTaps;
        std::size_t faceYAngleNumTaps;
        std::size_t faceZAngleNumTaps;
        std::size_t mouthFormNumTaps;
        std::size_t mouthOpenNumTaps;
        std::size_t leftEyeOpenNumTaps;
        std::size_t rightEyeOpenNumTaps;
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
        bool winkEnable;
        bool autoBlink;
        bool autoBreath;
        bool randomMotion;
    } m_cfg;
};

#endif

