@echo off
rem Build the Voica host and the demo app into dist\.
rem Requires: cmake, a C compiler (MSVC or gcc), and clang with lld
rem (clang compiles the wasm32 app; MSVC cannot target wasm32).
setlocal
cd /d "%~dp0"
set WAMR_TAG=WAMR-2.4.6

if not exist "third_party\wasm-micro-runtime" (
  echo fetching WAMR %WAMR_TAG%...
  if not exist third_party mkdir third_party
  curl -sL -o third_party\wamr.tar.gz "https://github.com/bytecodealliance/wasm-micro-runtime/archive/refs/tags/%WAMR_TAG%.tar.gz"
  if errorlevel 1 exit /b 1
  tar -xzf third_party\wamr.tar.gz -C third_party
  ren "third_party\wasm-micro-runtime-%WAMR_TAG%" wasm-micro-runtime
  del third_party\wamr.tar.gz
)

cmake -S host -B build -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 exit /b 1
cmake --build build --config Release
if errorlevel 1 exit /b 1

if not exist dist mkdir dist
if exist "build\Release\voica.exe" (
  copy /y "build\Release\voica.exe" dist\ >nul
) else (
  copy /y "build\voica.exe" dist\ >nul
)
clang --target=wasm32 -nostdlib -O2 -Wl,--no-entry -Wl,--export=run -o dist\recorder.wasm app\recorder.c
if errorlevel 1 exit /b 1

echo built dist\voica.exe and dist\recorder.wasm
echo run it:  cd dist ^&^& voica recorder.wasm
