// -*- mode: c++ -*-

#ifndef __FACE_DETECTOR_MATH_UTILS_H__
#define __FACE_DETECTOR_MATH_UTILS_H__

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

#include <cmath>
#include <initializer_list>
#include <dlib/image_processing.h>

static const double PI = 3.14159265358979;

template<class T>
static double avg(T container, double defaultValue = 0)
{
    if (container.size() == 0)
    {
        return defaultValue;
    }

    double sum = 0;
    for (auto it = container.begin(); it != container.end(); ++it)
    {
        sum += *it;
    }
    return sum / container.size();
}

template<class... Args>
static dlib::point centroid(Args&... args)
{
    std::size_t numArgs = sizeof...(args);
    if (numArgs == 0) return dlib::point(0, 0);

    double sumX = 0, sumY = 0;
    for (auto point : {args...})
    {
        sumX += point.x();
        sumY += point.y();
    }

    return dlib::point(sumX / numArgs, sumY / numArgs);
}

static inline double sq(double x)
{
    return x * x;
}

static double solveCosineRuleAngle(double opposite,
                                   double adjacent1,
                                   double adjacent2)
{
    // c^2 = a^2 + b^2 - 2 a b cos(C)
    double cosC = (sq(opposite) - sq(adjacent1) - sq(adjacent2)) /
                  (-2 * adjacent1 * adjacent2);
    return std::acos(cosC);
}

static inline double radToDeg(double rad)
{
    return rad * 180 / PI;
}

static inline double degToRad(double deg)
{
    return deg * PI / 180;
}

double dist(dlib::point& p1, dlib::point& p2)
{
    double xDist = p1.x() - p2.x();
    double yDist = p1.y() - p2.y();

    return std::hypot(xDist, yDist);
}

/*! Scale linearly from 0 to 1 (both end-points inclusive) */
double linearScale01(double num, double min, double max,
                     bool clipMin = true, bool clipMax = true)
{
    if (num < min && clipMin) return 0.0;
    if (num > max && clipMax) return 1.0;
    return (num - min) / (max - min);
}

#endif
