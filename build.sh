#!/bin/bash
mkdir -p build
CPP_FILES=$(find src -name "*.cpp")

g++ -std=c++11 -I./include -o build/engine $CPP_FILES