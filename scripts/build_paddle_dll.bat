@echo off
setlocal
set SCRIPT_DIR=%~dp0
set ROOT_DIR=%SCRIPT_DIR%..
set PATH=%ROOT_DIR%\toolchain\mingw\bin;%PATH%

echo ========================================
echo Building PaddlePaddle Inference DLL Mode (MinGW)
echo 4-DLL split: common / pir / phi_core / paddle_inference
echo Root dir: %ROOT_DIR%
echo ========================================

:: 检查源码目录
if not exist "%ROOT_DIR%\src\Paddle\CMakeLists.txt" (
    echo ERROR: Paddle source not found at %ROOT_DIR%\src\Paddle
    exit /b 1
)

:: 创建 DLL 构建目录（与静态模式分开）
set BUILD_DIR=%ROOT_DIR%\src\Paddle\build_gcc_dll
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
cd /d "%BUILD_DIR%"

echo.
echo [Step 1] Running CMake configuration (DLL mode)...
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
  -DWITH_SHARED_PHI=ON ^
  -DWITH_SHARED_IR=ON ^
  -DCMAKE_CXX_FLAGS="-Wa,-mbig-obj -g0 -O2" ^
  -DCMAKE_C_FLAGS="-Wa,-mbig-obj -g0 -O2"

if errorlevel 1 (
    echo CMake configure failed!
    exit /b 1
)

echo.
echo [Step 2] Building with Ninja...
"%ROOT_DIR%\toolchain\mingw\bin\ninja.exe" -j16

if errorlevel 1 (
    echo Build failed!
    exit /b 1
)

echo.
echo [Step 3] Copying DLLs and import libraries...
set DLL_DST=%ROOT_DIR%\libs\paddle_inference_gcc\dll
if not exist "%DLL_DST%" mkdir "%DLL_DST%"

:: 复制 4 个 Paddle DLL 和导入库
copy /Y "%BUILD_DIR%\paddle\common\libcommon.dll" "%DLL_DST%\" >nul
copy /Y "%BUILD_DIR%\paddle\common\libcommon.dll.a" "%DLL_DST%\" >nul
copy /Y "%BUILD_DIR%\paddle\pir\libpir.dll" "%DLL_DST%\" >nul
copy /Y "%BUILD_DIR%\paddle\pir\libpir.dll.a" "%DLL_DST%\" >nul
copy /Y "%BUILD_DIR%\paddle\phi\libphi_core.dll" "%DLL_DST%\" >nul
copy /Y "%BUILD_DIR%\paddle\phi\libphi_core.dll.a" "%DLL_DST%\" >nul
copy /Y "%BUILD_DIR%\paddle\fluid\inference\libpaddle_inference.dll" "%DLL_DST%\" >nul
copy /Y "%BUILD_DIR%\paddle\fluid\inference\libpaddle_inference.dll.a" "%DLL_DST%\" >nul

:: 复制第三方 DLL（如果存在）
if exist "%BUILD_DIR%\third_party\install\mkldnn\lib" (
    copy /Y "%BUILD_DIR%\third_party\install\mkldnn\lib\*.dll" "%DLL_DST%\" >nul 2>nul
)
if exist "%BUILD_DIR%\third_party\install\openblas\lib" (
    copy /Y "%BUILD_DIR%\third_party\install\openblas\lib\*.dll" "%DLL_DST%\" >nul 2>nul
)
if exist "%BUILD_DIR%\third_party\install\protobuf\lib" (
    copy /Y "%BUILD_DIR%\third_party\install\protobuf\lib\*.dll" "%DLL_DST%\" >nul 2>nul
)

:: 验证输出
echo.
echo ========================================
echo DLL build completed!
echo Output directory: %DLL_DST%
echo.
echo DLLs built:
if exist "%DLL_DST%\libcommon.dll" (echo   [OK] libcommon.dll) else (echo   [MISSING] libcommon.dll)
if exist "%DLL_DST%\libpir.dll" (echo   [OK] libpir.dll) else (echo   [MISSING] libpir.dll)
if exist "%DLL_DST%\libphi_core.dll" (echo   [OK] libphi_core.dll) else (echo   [MISSING] libphi_core.dll)
if exist "%DLL_DST%\libpaddle_inference.dll" (echo   [OK] libpaddle_inference.dll) else (echo   [MISSING] libpaddle_inference.dll)
echo ========================================

endlocal
