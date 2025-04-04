#!/bin/bash
mkdir -p build

INCLUDES="-I./include -I/opt/homebrew/include"
LDFLAGS="-L/opt/homebrew/lib"
LIBS="-lSDL2"
CPP_FILES=$(find src -name "*.cpp")

OBJECTS=""
for file in $CPP_FILES; do
    obj="build/$(basename ${file%.cpp}).o"
    OBJECTS="$OBJECTS $obj"
    
    echo "Compiling $file -> $obj"
    g++ -std=c++20 $INCLUDES -c $file -o $obj
done

g++ $OBJECTS $LDFLAGS $LIBS -o build/engine