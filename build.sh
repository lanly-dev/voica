#!/bin/sh
# Build the Voica host and the demo app into dist/.
# Requires: cmake, a C compiler, and clang (for the wasm32 app).
set -e
cd "$(dirname "$0")"

WAMR_TAG="WAMR-2.4.6"
if [ ! -d third_party/wasm-micro-runtime ]; then
  echo "fetching WAMR ${WAMR_TAG}..."
  mkdir -p third_party
  curl -sL "https://github.com/bytecodealliance/wasm-micro-runtime/archive/refs/tags/${WAMR_TAG}.tar.gz" \
    | tar xz -C third_party
  mv "third_party/wasm-micro-runtime-${WAMR_TAG}" third_party/wasm-micro-runtime
fi

cmake -S host -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

mkdir -p dist
cp build/voica dist/
clang --target=wasm32 -nostdlib -O2 -Wl,--no-entry -Wl,--export=run \
  -o dist/recorder.wasm app/recorder.c

ls -la dist/
echo "run it:  cd dist && ./voica recorder.wasm"
