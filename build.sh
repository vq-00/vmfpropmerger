#!/bin/bash

# Build script for vmfpropmerger
# Usage:
#   ./build.sh
#   ./build.sh /path/to/cmake /path/to/gcc
#   CMAKE_PATH=/path/to/cmake GCC_PATH=/path/to/gcc ./build.sh

set -e

if [ -n "$1" ]; then
    CMAKE_PATH="$1"
fi
if [ -n "$2" ]; then
    GCC_PATH="$2"
fi

echo "Building Reactive Drop VMF Prop Merger..."
echo "==========================================="
echo ""

if [ -n "$CMAKE_PATH" ]; then
    if [ -x "$CMAKE_PATH/bin/cmake" ]; then
        CMAKE_CMD="$CMAKE_PATH/bin/cmake"
    elif [ -x "$CMAKE_PATH/cmake" ]; then
        CMAKE_CMD="$CMAKE_PATH/cmake"
    else
        echo "ERROR: CMake path is invalid: $CMAKE_PATH"
        echo "Provide a portable CMake location:"
        echo "  ./build.sh /path/to/cmake /path/to/gcc"
        exit 1
    fi
else
    if command -v cmake >/dev/null 2>&1; then
        CMAKE_CMD="cmake"
    else
        echo "ERROR: CMake was not found."
        echo "Provide a portable CMake location:"
        echo "  ./build.sh /path/to/cmake /path/to/gcc"
        echo "or install CMake and add it to PATH."
        exit 1
    fi
fi

if [ -n "$GCC_PATH" ]; then
    if [ -x "$GCC_PATH/bin/gcc" ] && [ -x "$GCC_PATH/bin/g++" ]; then
        GCC_BIN="$GCC_PATH/bin"
    elif [ -x "$GCC_PATH/gcc" ] && [ -x "$GCC_PATH/g++" ]; then
        GCC_BIN="$GCC_PATH"
    else
        echo "ERROR: GCC toolchain not found under $GCC_PATH"
        echo "Provide a valid GCC or MinGW directory containing gcc and g++ (root or bin path)."
        exit 1
    fi
else
    if command -v gcc >/dev/null 2>&1 && command -v g++ >/dev/null 2>&1; then
        GCC_BIN="$(dirname "$(command -v gcc)")"
    else
        echo "ERROR: No GCC toolchain was found."
        echo "Provide a portable GCC location:"
        echo "  ./build.sh /path/to/cmake /path/to/gcc"
        echo "or install GCC/MinGW and add it to PATH."
        exit 1
    fi
fi

mkdir -p build
cd build

echo "Configuring project..."
"$CMAKE_CMD" -G "MinGW Makefiles" -DCMAKE_C_COMPILER="$GCC_BIN/gcc" -DCMAKE_CXX_COMPILER="$GCC_BIN/g++" ..

if [ $? -ne 0 ]; then
    echo "ERROR: CMake configuration failed."
    cd ..
    exit 1
fi

echo "Compiling project..."
"$CMAKE_CMD" --build . --config Release --parallel 4

if [ $? -ne 0 ]; then
    echo "ERROR: Build failed."
    cd ..
    exit 1
fi

cd ..

echo ""
echo "Build complete!"
echo "Binary location: $(pwd)/build/vmfpropmerger"
echo ""
echo "To run: ./build/vmfpropmerger --help"