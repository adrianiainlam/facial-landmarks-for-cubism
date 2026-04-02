#!/bin/sh -e

os=linux
patch=demo.patch
if [ "$OSTYPE" = "msys" -o "$OSTYPE" = "cygwin" ]; then
    os=win
    patch=demo_win.patch
fi

mkdir -p demo_build

cp -r CubismSdkForNative-5-r.5/Samples/OpenGL/Demo/proj."$os".cmake/* ./demo_build/
cd demo_build
git init
git add .
git commit -m "Original example from CubismSdkForNative"
git apply ../"$patch"

if [ "$OSTYPE" = "msys" -o "$OSTYPE" = "cygwin" ]; then
    echo "Now go into ./demo_build/scripts and run the corresponding script for your MSVC version"
else
    ./scripts/make_gcc
fi
