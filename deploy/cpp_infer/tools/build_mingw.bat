@echo off
setlocal enabledelayedexpansion

REM PaddleOCR MinGW Build Script
REM Usage: build_mingw.bat [PADDLE_LIB_DIR] [OPENCV_DIR]

set PADDLE_LIB=%1
set OPENCV_DIR=%2
set MINGW_PATH=D:\tmp\tmp\toolchain\mingw

if "%PADDLE_LIB%"=="" (
    echo Usage: build_mingw.bat [PADDLE_LIB_DIR] [OPENCV_DIR]
    echo Example: build_mingw.bat D:\tmp\tmp\libs\paddle_inference_gcc D:\tmp\tmp\libs\opencv_install_gcc
    exit /b 1
)

if "%OPENCV_DIR%"=="" (
    echo Usage: build_mingw.bat [PADDLE_LIB_DIR] [OPENCV_DIR]
    echo Example: build_mingw.bat D:\tmp\tmp\libs\paddle_inference_gcc D:\tmp\tmp\libs\opencv_install_gcc
    exit /b 1
)

echo ========================================
echo PaddleOCR MinGW Build
echo ========================================
echo Paddle Inference: %PADDLE_LIB%
echo OpenCV: %OPENCV_DIR%
echo MinGW: %MINGW_PATH%
echo ========================================

REM Set PATH to include MinGW and CMake
set PATH=%MINGW_PATH%\bin;D:\APPS\Qt\Tools\CMake_64\bin;%PATH%

REM Create build directory
if exist build_mingw (
    echo Cleaning existing build directory...
    rmdir /s /q build_mingw
)
mkdir build_mingw
cd build_mingw

REM Run CMake
echo Running CMake...
cmake .. -G "MinGW Makefiles" ^
    -DCMAKE_C_COMPILER="%MINGW_PATH%/bin/gcc.exe" ^
    -DCMAKE_CXX_COMPILER="%MINGW_PATH%/bin/g++.exe" ^
    -DCMAKE_MAKE_PROGRAM="%MINGW_PATH%/bin/mingw32-make.exe" ^
    -DPADDLE_LIB="%PADDLE_LIB%" ^
    -DOPENCV_DIR="%OPENCV_DIR%" ^
    -DWITH_MKL=OFF ^
    -DWITH_GPU=OFF ^
    -DWITH_STATIC_LIB=ON

if errorlevel 1 (
    echo CMake configuration failed!
    exit /b 1
)

REM Build
echo Building...
mingw32-make -j%NUMBER_OF_PROCESSORS%

if errorlevel 1 (
    echo Build failed!
    exit /b 1
)

echo ========================================
echo Build completed successfully!
echo Executable: build_mingw\ppocr.exe
echo ========================================

cd ..
