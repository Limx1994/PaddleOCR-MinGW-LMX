@echo off
setlocal
set SCRIPT_DIR=%~dp0
set ROOT_DIR=%SCRIPT_DIR%..
set PATH=%ROOT_DIR%\toolchain\mingw\bin;%PATH%

echo ========================================
echo Building PaddlePaddle Inference (MinGW)
echo Root dir: %ROOT_DIR%
echo ========================================

if not exist "%ROOT_DIR%\src\Paddle\build_gcc" mkdir "%ROOT_DIR%\src\Paddle\build_gcc"
cd /d "%ROOT_DIR%\src\Paddle\build_gcc"

echo.
echo [Step 1] Running CMake configuration...
"%ROOT_DIR%\toolchain\mingw\bin\cmake.exe" .. -G Ninja ^
  -DCMAKE_C_COMPILER="%ROOT_DIR%/toolchain/mingw/bin/gcc.exe" ^
  -DCMAKE_CXX_COMPILER="%ROOT_DIR%/toolchain/mingw/bin/g++.exe" ^
  -DCMAKE_MAKE_PROGRAM="%ROOT_DIR%/toolchain/mingw/bin/ninja.exe" ^
  -DCMAKE_INSTALL_PREFIX="%ROOT_DIR%/libs/paddle_inference_gcc" ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DWITH_GPU=OFF ^
  -DWITH_MKL=OFF ^
  -DWITH_ONEDNN=ON ^
  -DWITH_AVX=ON ^
  -DWITH_PYTHON=OFF ^
  -DON_INFER=ON ^
  -DWITH_TESTING=OFF ^
  -DWITH_INFERENCE_API_TEST=OFF ^
  -DWITH_ONNXRUNTIME=OFF ^
  -DWITH_PSCORE=OFF ^
  -DWITH_HETERPS=OFF ^
  -DWITH_ARM=OFF ^
  -DWITH_CRYPTO=OFF ^
  -DCMAKE_CXX_FLAGS="-Wa,-mbig-obj -g0 -O2" ^
  -DCMAKE_C_FLAGS="-Wa,-mbig-obj -g0 -O2"

if errorlevel 1 (
    echo CMake configure failed!
    exit /b 1
)

echo.
echo [Step 2] Building with Ninja...
"%ROOT_DIR%\toolchain\mingw\bin\ninja.exe" -j12

if errorlevel 1 (
    echo Build failed!
    exit /b 1
)

echo.
echo ========================================
echo Build completed successfully!
echo ========================================

endlocal
