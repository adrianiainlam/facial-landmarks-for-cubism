#!/bin/bash -e

if [[ "$1" == "-h" || "$1" == "--help" ]]; then
    echo "Example usage:"
    echo "./upgrade.sh ./CubismSdkForNative-5-r.5"
    exit 0
fi

os=linux
if [ "$OSTYPE" = "msys" -o "$OSTYPE" = "cygwin" ]; then
    os=win
fi

git -C demo_dev checkout orig
cp -r "$1"/Samples/OpenGL/Demo/proj."$os".cmake/* ./demo_dev/
cd demo_dev
git add .
git commit -m "$(basename $1) $os"
git checkout master
git merge orig

echo "Now resolve conflicts, build, and test..."
