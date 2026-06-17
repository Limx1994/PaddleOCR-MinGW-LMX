@echo off
setlocal

set MINGW_DIR=D:\tmp\tmp\toolchain\mingw
set PADDLE_LIB=D:\tmp\tmp\libs\paddle_inference_gcc
set OPENCV_DIR=D:\tmp\tmp\libs\opencv_install_gcc
set BUILD_DIR=%~dp0build_mingw
set SOURCE_DIR=%~dp0

echo ========================================
echo Building PaddleOCR C++ Inference Engine
echo Using MinGW GCC: %MINGW_DIR%
echo Paddle Inference: %PADDLE_LIB%
echo OpenCV: %OPENCV_DIR%
echo ========================================

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
cd /d "%BUILD_DIR%"

echo.
echo [Step 1] Running CMake configuration...
cmake "%SOURCE_DIR%" ^
    -G "Ninja" ^
    -DCMAKE_C_COMPILER="%MINGW_DIR%/bin/gcc.exe" ^
    -DCMAKE_CXX_COMPILER="%MINGW_DIR%/bin/g++.exe" ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_MAKE_PROGRAM="%MINGW_DIR%/bin/ninja.exe" ^
    -DPADDLE_LIB="%PADDLE_LIB%" ^
    -DOPENCV_DIR="%OPENCV_DIR%" ^
    -DWITH_MKL=OFF ^
    -DWITH_GPU=OFF ^
    -DWITH_STATIC_LIB=ON ^
    -DUSE_FREETYPE=OFF

if errorlevel 1 (
    echo CMake configuration failed!
    exit /b 1
)

echo.
echo [Step 2] Building...
"%MINGW_DIR%/bin/ninja.exe" -j16

if errorlevel 1 (
    echo Build failed!
    exit /b 1
)

echo.
echo ========================================
echo Build completed successfully!
echo Output: %BUILD_DIR%\ppocr.exe
echo ========================================

endlocal
