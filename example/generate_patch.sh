#!/bin/sh

if [ "$OSTYPE" = "msys" -o "$OSTYPE" = "cygwin" ]; then
    git -C demo_dev diff orig > ./demo_win.patch
else
    git -C demo_dev diff orig > ./demo.patch
fi
