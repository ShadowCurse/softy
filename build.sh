#!/bin/bash

mkdir -p build

clang -g -O0 -lm -lSDL2 src/main.c src/stb.c -o build/softy
