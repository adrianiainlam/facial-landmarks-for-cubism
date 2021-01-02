# Facial Landmarks for Cubism

A library that extracts facial landmarks from a webcam feed and converts them
into Live2D® Cubism SDK parameters.

*Disclaimer: This library is designed for use with the Live2D® Cubism SDK.
It is not part of the SDK itself, and is not affiliated in any way with Live2D
Inc. The Live2D® Cubism SDK belongs solely to Live2D Inc. You will need to
agree to Live2D Inc.'s license agreements to use the Live2D® Cubism SDK.*

This block diagram shows the intended usage of this library:

![Block diagram showing interaction of this library with other components](block_diagram.png)

Video showing me using the example program:
<https://youtu.be/SZPEKwEqbdI>

## Spin-off: Mouse Tracking for Cubism

An alternative version using mouse cursor tracking and audio based lip
syncing instead of face tracking is available at
<https://github.com/adrianiainlam/mouse-tracker-for-cubism>.

The main advantage is a much lower CPU load.

## Supporting environments

This library was developed and tested only on Ubuntu 18.04 using GCC 7.5.0.
However I don't think I've used anything that prevents it from being
cross-platform compatible -- it should still work as long as you have a
recent C/C++ compiler. The library should only require C++11. The Cubism
SDK requires C++14. I have made use of one C++17 library (`<filesystem>`)
in the example program, but it should be straightforward to change this
if you don't have C++17 support.

I have provided some shell scripts for convenience when building. In an
environment without a `/bin/sh` shell you may have to run the commands
manually. Hereafter, all build instructions will assume a Linux environment
where a shell is available.

If your CPU does not support AVX instructions you may want to edit "build.sh"
and "example/demo.patch" to remove the `-D USE_AVX_INSTRUCTIONS=1` variable
(or change AVX to SSE4 or SSE2). However there could be a penalty in
performance.

## Build instructions

1. Install dependencies.

   You will require a recent C/C++ compiler, `make`, `patch`, CMake >= 3.16,
   and the OpenCV library (I'm using version 4.3.0). To compile the example
   program you will also require the OpenGL library (and its dev headers)
   among other libraries required for the example program. The libraries I
   had to install (this list may not be exhaustive) are:

       libgl1-mesa-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libglu1-mesa-dev

2. Clone this repository including its submodule (dlib)

       git clone --recurse-submodules https://github.com/adrianiainlam/facial-landmarks-for-cubism.git

3. To build the library only: (Skip this step if you want to build the example
   program. It will be done automatically.)

       cd <path of the git repo>
       ./build.sh

4. You will require a facial landmark dataset to use with dlib. I have
   downloaded mine from
   <http://dlib.net/files/shape_predictor_68_face_landmarks.dat.bz2>.
   Extract the file and edit the "config.txt" file to point to the
   path to this file.

   Note: The license for this dataset excludes commercial use. If you want
   to use this library in a commercial product you will need to obtain a
   dataset in some other way.

To build the example program:

5. Copy the extracted dlib dataset from step 4 to the "example" folder
   of this repo.

6. Download "Cubism 4 SDK for Native R1" from the Live2D website:
   <https://www.live2d.com/en/download/cubism-sdk/download-native/>.

   Extract the archive -- put the "CubismSdkForNative-4-r.1" folder under
   the "example" folder of this repo.

   Note: The Cubism SDK is the property of Live2D and is not part of this
   project. You must agree to Live2D's license agreements to use it.

7. Go into the
   "example/CubismSdkForNative-4-r.1/Samples/OpenGL/thirdParty/scripts"
   directory and run

       ./setup_glew_glfw

8. Go back to the "example" directory and run

       ./build.sh

9. Now try running the example program. From the "example" directory:

       cd ./demo_build/build/make_gcc/bin/Demo/
       ./Demo


## Command-line arguments for the example program

Most command-line arguments are to control the Cubism side of the program.
Only one argument (`--config`) is used to specify the configuration file
for the Facial Landmarks for Cubism library.

 * `--window-width`, `-W`: Specify the window width
 * `--window-height`, `-H`: Specify the window height
 * `--window-title`, `-t`: Specify the window title
 * `--root-dir`, `-d`: The directory at which the "Resources" folder will
   be found. This is where the model data will be located.
 * `--scale-factor`, `-f`: How the model should be scaled
 * `--translate-x`, `-x`: Horizontal translation of the model within the
   window
 * `--translate-y`, `-y`: Vertical translation of the model within the window
 * `--model`, `-m`: Name of the model to be used. This must be located inside
   the "Resources" folder.
 * `--old-param-id`, `-o`: If set to 1, translate new (Cubism 3+) parameter
   IDs to old (Cubism 2.1) IDs. This is necessary, for example, for
   [the Chitose model available from Live2D](https://www.live2d.com/en/download/sample-data/).
 * `--config`, `-c`: Path to the configuration file for the Facial Landmarks
   for Cubism library. See below for more details.


## Configuration file

Due to the differences in hardware and differences in each person's face,
I have decided to make pretty much every parameter tweakable. The file
"config.txt" lists and documents all parameters and their default values.
You can change the values there and pass it to the example program using
the `-c` argument. If using the library directly, the path to this file
should be passed to the constructor (or pass an empty string to use
default values).

## Troubleshooting

1. Example program crashes with SIGILL (Illegal instruction).

   Your CPU probably doesn't support AVX instructions which is used by dlib.
   You can confirm this by running

       grep avx /proc/cpuinfo

   If this is the case, try to find out if your CPU supports SSE4 or SSE2,
   then edit "build.sh" and "example/demo.patch" to change
   `USE_AVX_INSTRUCTIONS=1` to `USE_SSE4_INSTRUCTIONS=1` or
   `USE_SSE2_INSTRUCTIONS=1`.

## License

The library itself is provided under the MIT license. By "the library itself"
I refer to the following files that I have provided under this repo:

 * src/facial_landmark_detector.cpp
 * src/math_utils.h
 * include/facial_landmark_detector.h
 * and if you decide to build the binary for the library, the resulting
   binary file (typically build/libFacialLandmarksForCubism.a)

The license text can be found in LICENSE-MIT.txt, and also at the top of
the .cpp and .h files.

The library makes use of the dlib library, provided here as a Git
submodule, which is used under the Boost Software License, version 1.0.
The full license text can be found under lib/dlib/dlib/LICENSE.txt.

The example program is a patched version of the sample program provided
by Live2D (because there's really no point in reinventing the wheel),
and as such, as per the licensing restrictions by Live2D, is still the
property of Live2D.

The patch file (example/demo.patch) contains lines showing additions by
me, as well as deleted lines and unchanged lines for context. The deleted
and unchanged lines are obviously still owned by Live2D. For my additions,
where substantial enough for me to claim ownership, I release them under
the Do What the Fuck You Want to Public License, version 2. The full license
text can be found in LICENSE-WTFPL.txt.

All other files not mentioned above that I have provided in this repo
(i.e. not downloaded and placed here by you), *excluding* the two license
documents and files generated by Git, are also released under the Do What
the Fuck You Want to Public License, version 2, whose full license text
can be found in LICENSE-WTFPL.txt.

In order to use example program, or in any other way use this library
with the Live2D® Cubism SDK, you must agree to the license by Live2D Inc.
Their licenses can be found here:
<https://www.live2d.com/en/download/cubism-sdk/download-native/>.

The library requires a facial landmark dataset, and the one provided by
dlib (which is derived from a dataset owned by Imperial College London)
has been used in development. The license for this dataset excludes
commercial use. You must obtain an alternative dataset if you wish to
use this library commercially.

This is not a license requirement, but if you find my library useful,
I'd love to hear from you! Send me an email at spam(at)adrianiainlam.tk --
replacing "spam" with the name of this repo :).

## Contributions

Contributions welcome! This is only a hobby weekend project so I don't
really have many environments / faces to test it on. Feel free to submit
issues or pull requests on GitHub, or send questions or patches to me
(see my email address above) if you prefer email. Thanks :)

