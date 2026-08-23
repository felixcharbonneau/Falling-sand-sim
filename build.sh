#!/bin/sh
set -e

mkdir -p build/shader

for f in src/shader/*.comp; do
    [ -e "$f" ] || continue
    out="build/shader/$(basename "$f").spv"
    echo "glslc $f -> $out"
    glslc --target-env=vulkan1.3 -I src/shader "$f" -o "$out"
done

set -e
SRC=$(find src -name '*.c')
gcc -std=c11 -g $SRC -I./src -o build/falling_sand \
    $(pkg-config --cflags --libs sdl3 vulkan) -lm -lpthread -ldl