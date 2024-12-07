#!/bin/bash

cd wasm

emcc \
  -std=c99 \
  -sUSE_SDL=2 \
  -sASSERTIONS=1 \
  -sMALLOC='dlmalloc' \
  -sFORCE_FILESYSTEM=1 \
  -sUSE_OFFSET_CONVERTER=1 \
  -sALLOW_MEMORY_GROWTH=1 \
  -sSTACK_SIZE=1mb \
  ../src/main.c \
  -o \
  softy.js

