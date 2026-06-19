@echo off
setlocal

:: Get project root directory
set SCRIPT_DIR=%~dp0
set ROOT_DIR=%SCRIPT_DIR%..\..\..
set MINGW_DIR=%ROOT_DIR%\toolchain\mingw
set PADDLE_LIB=%ROOT_DIR%\libs\paddle_inference_gcc
set OPENCV_DIR=%ROOT_DIR%\libs\opencv_install_gcc

:: Check if DLL mode
set USE_DLL=OFF
set USE_STATIC=ON
if "%1"=="--dll" (
    set USE_DLL=ON
    set USE_STATIC=OFF
    echo ========================================
    echo Building OCR Service [DLL MODE]
    echo ========================================
) else (
    echo ========================================
    echo Building OCR Service [STATIC MODE]
    echo Use --dll flag for DLL mode
    echo ========================================
)

:: Check toolchain
if not exist "%MINGW_DIR%\bin\gcc.exe" (
    echo ERROR: MinGW GCC not found at %MINGW_DIR%\bin\gcc.exe
    exit /b 1
)

:: Check Paddle library
if not exist "%PADDLE_LIB%" (
    echo ERROR: Paddle Inference library not found at %PADDLE_LIB%
    exit /b 1
)

:: Check OpenCV
if not exist "%OPENCV_DIR%" (
    echo ERROR: OpenCV not found at %OPENCV_DIR%
    exit /b 1
)

:: Create build directory
set BUILD_DIR=%SCRIPT_DIR%build
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
cd "%BUILD_DIR%"

:: Find cmake
set CMAKE_EXE=%ROOT_DIR%\toolchain\mingw\bin\cmake.exe
if not exist "%CMAKE_EXE%" (
    where cmake.exe >nul 2>&1
    if errorlevel 1 (
        echo ERROR: cmake.exe not found in toolchain or system PATH.
        echo Please install CMake and add it to PATH.
        exit /b 1
    )
    for /f "delims=" %%i in ('where cmake.exe') do set CMAKE_EXE=%%i
    echo [INFO] Using system cmake: %CMAKE_EXE%
) else (
    echo [INFO] Using toolchain cmake: %CMAKE_EXE%
)

echo.
echo MinGW GCC: %MINGW_DIR%
echo Paddle Inference: %PADDLE_LIB%
echo OpenCV: %OPENCV_DIR%
echo Build dir: %BUILD_DIR%
echo.

:: Run CMake
echo Running CMake...
%CMAKE_EXE% .. -G "MinGW Makefiles" ^
    -DCMAKE_C_COMPILER=%MINGW_DIR%\bin\gcc.exe ^
    -DCMAKE_CXX_COMPILER=%MINGW_DIR%\bin\g++.exe ^
    -DPADDLE_LIB=%PADDLE_LIB% ^
    -DOPENCV_DIR=%OPENCV_DIR% ^
    -DWITH_STATIC_LIB=%USE_STATIC% ^
    -DWITH_DLL_LIB=%USE_DLL%

if errorlevel 1 (
    echo ERROR: CMake configuration failed
    exit /b 1
)

:: Build
echo Building...
%MINGW_DIR%\bin\mingw32-make.exe -j10

if errorlevel 1 (
    echo ERROR: Build failed
    exit /b 1
)

echo.
echo ========================================
echo Build completed successfully!
echo ========================================
echo Service: %BUILD_DIR%\ppocr_service.exe
echo Client:  %BUILD_DIR%\ppocr_client.exe
echo.
echo Usage:
echo   ppocr_service.exe --model_dir ..\..\..\dist\ppocr\models --port 8080
echo   ppocr_client.exe ..\..\..\dist\ppocr\test.jpg
echo ========================================
