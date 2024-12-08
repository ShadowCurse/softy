#!/bin/bash

mkdir -p build

clang -lm -lSDL2 src/main.c src/stb.c -o build/softy
