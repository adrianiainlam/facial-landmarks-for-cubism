#!/bin/sh

mkdir -p demo_clean

if [ "$OSTYPE" = "msys" -o "$OSTYPE" = "cygwin" ]; then
    cp -p -r CubismSdkForNative-4-r.6.2/Samples/OpenGL/Demo/proj.win.cmake/* ./demo_clean/
    diff -pruN --exclude build ./demo_clean ./demo_dev > ./demo_win.patch
else
    cp -p -r CubismSdkForNative-4-r.6.2/Samples/OpenGL/Demo/proj.linux.cmake/* ./demo_clean/
    diff -pruN --exclude build ./demo_clean ./demo_dev > ./demo.patch
fi
