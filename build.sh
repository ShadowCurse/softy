#!/bin/bash

mkdir -p build

clang -lSDL2 src/main.c -o build/softy
