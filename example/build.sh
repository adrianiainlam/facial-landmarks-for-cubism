#!/bin/sh -e

mkdir -p demo_build

if [ "$OSTYPE" = "msys" -o "$OSTYPE" = "cygwin" ]; then
    cp -r CubismSdkForNative-4-r.6/Samples/OpenGL/Demo/proj.win.cmake/* ./demo_build/
    patch -d demo_build -p2 < demo_win.patch
    echo "Now go into ./demo_build/scripts and run the corresponding script for your MSVC version"
else
    cp -r CubismSdkForNative-4-r.6/Samples/OpenGL/Demo/proj.linux.cmake/* ./demo_build/
    patch -d demo_build -p2 < demo.patch
    ./demo_build/scripts/make_gcc
fi
