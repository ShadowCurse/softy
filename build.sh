#!/bin/bash

mkdir -p build

clang -lm -lSDL2 src/main.c src/stb_image.c -o build/softy
