#!/bin/bash
cd ..
mkdir ext
cd ext
git clone https://github.com/emscripten-core/emsdk.git

cd emsdk
./emsdk install 4.0.17
./emsdk activate 4.0.17
