@echo off
setlocal
cd /d "%~dp0\.."

if not exist build (
    cmake -B build -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTING=ON
)

cmake --build build --config Release --parallel

