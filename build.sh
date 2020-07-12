#!/bin/sh

mkdir -p build && cd build

cmake .. -D CMAKE_BUILD_TYPE=Release -D USE_AVX_INSTRUCTIONS=1
cmake --build . -j6
