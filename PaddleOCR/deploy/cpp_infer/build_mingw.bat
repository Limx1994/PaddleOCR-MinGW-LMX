@echo off
setlocal

:: 获取项目根目录（相对于此脚本位置）
set SCRIPT_DIR=%~dp0
set ROOT_DIR=%SCRIPT_DIR%..\..\..
set MINGW_DIR=%ROOT_DIR%\toolchain\mingw
set PADDLE_LIB=%ROOT_DIR%\libs\paddle_inference_gcc
set OPENCV_DIR=%ROOT_DIR%\libs\opencv_install_gcc
set SOURCE_DIR=%SCRIPT_DIR%

:: 检查是否使用 DLL 模式
set USE_DLL=OFF
set USE_STATIC=ON
set BUILD_SUFFIX=
if "%1"=="--dll" (
    set USE_DLL=ON
    set USE_STATIC=OFF
    set BUILD_SUFFIX=_dll
    echo ========================================
    echo Building PaddleOCR C++ Inference Engine [DLL MODE]
    echo ========================================
) else if "%1"=="-dll" (
    set USE_DLL=ON
    set USE_STATIC=OFF
    set BUILD_SUFFIX=_dll
    echo ========================================
    echo Building PaddleOCR C++ Inference Engine [DLL MODE]
    echo ========================================
) else (
    echo ========================================
    echo Building PaddleOCR C++ Inference Engine [STATIC MODE]
    echo Use --dll flag for DLL mode
    echo ========================================
)

set BUILD_DIR=%SCRIPT_DIR%build_mingw%BUILD_SUFFIX%

echo.
echo MinGW GCC: %MINGW_DIR%
echo Paddle Inference: %PADDLE_LIB%
echo OpenCV: %OPENCV_DIR%
echo Build dir: %BUILD_DIR%
echo DLL mode: %USE_DLL%
echo.

:: 检查工具链
if not exist "%MINGW_DIR%\bin\gcc.exe" (
    echo ERROR: MinGW GCC not found at %MINGW_DIR%\bin\gcc.exe
    exit /b 1
)

:: 检查 Paddle 库
if not exist "%PADDLE_LIB%" (
    echo ERROR: Paddle Inference library not found at %PADDLE_LIB%
    echo Please build Paddle first: scripts\build_paddle.bat
    exit /b 1
)

:: 检查 OpenCV
if not exist "%OPENCV_DIR%" (
    echo ERROR: OpenCV not found at %OPENCV_DIR%
    echo Please build OpenCV first or install to libs\opencv_install_gcc
    exit /b 1
)

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
    -DWITH_STATIC_LIB=%USE_STATIC% ^
    -DWITH_DLL_LIB=%USE_DLL% ^
    -DUSE_FREETYPE=OFF

if errorlevel 1 (
    echo CMake configuration failed!
    exit /b 1
)

echo.
echo [Step 2] Building...
"%MINGW_DIR%\bin\ninja.exe" -j16

if errorlevel 1 (
    echo Build failed!
    exit /b 1
)

echo.
echo ========================================
echo Build completed successfully!
echo Output: %BUILD_DIR%\ppocr.exe
if "%USE_DLL%"=="ON" (
    echo.
    echo NOTE: DLL mode requires DLLs in the runtime directory.
    echo Run: scripts\distribute_dll.bat
)
echo ========================================

endlocal
