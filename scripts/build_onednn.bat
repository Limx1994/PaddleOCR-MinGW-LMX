@echo off
setlocal

:: oneDNN (MKLDNN) 独立构建脚本 - MinGW GCC + Ninja
:: 用法: cd scripts && build_onednn.bat

set SCRIPT_DIR=%~dp0
set MINGW_ROOT=%SCRIPT_DIR%..\toolchain\mingw
set ONEDNN_SRC=%SCRIPT_DIR%..\src\Paddle\third_party\onednn
set ONEDNN_BUILD=%SCRIPT_DIR%..\src\Paddle\third_party\onednn\build_mingw
set ONEDNN_INSTALL=%SCRIPT_DIR%..\libs\onednn_install_gcc

:: 检查 oneDNN 源码是否存在
if not exist "%ONEDNN_SRC%\CMakeLists.txt" (
    echo ERROR: oneDNN source not found at %ONEDNN_SRC%
    echo Please ensure the git submodule is checked out.
    exit /b 1
)

:: 确保 MinGW 在 PATH
set PATH=%MINGW_ROOT%\bin;%PATH%

:: 检查编译器
where gcc.exe >nul 2>&1
if errorlevel 1 (
    echo ERROR: gcc.exe not found in PATH. Please check MinGW installation.
    exit /b 1
)

:: 创建构建目录
if not exist "%ONEDNN_BUILD%" mkdir "%ONEDNN_BUILD%"
cd /d "%ONEDNN_BUILD%"

echo ============================================
echo Building oneDNN with MinGW GCC + Ninja
echo Source: %ONEDNN_SRC%
echo Build:  %ONEDNN_BUILD%
echo Install: %ONEDNN_INSTALL%
echo ============================================

:: 配置 oneDNN
cmake -G Ninja ^
    -DCMAKE_C_COMPILER=%MINGW_ROOT%\bin\gcc.exe ^
    -DCMAKE_CXX_COMPILER=%MINGW_ROOT%\bin\g++.exe ^
    -DCMAKE_MAKE_PROGRAM=%MINGW_ROOT%\bin\ninja.exe ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_INSTALL_PREFIX=%ONEDNN_INSTALL% ^
    -DCMAKE_C_FLAGS="-Wa,-mbig-obj -O2" ^
    -DCMAKE_CXX_FLAGS="-Wa,-mbig-obj -O2" ^
    -DDNNL_CPU_RUNTIME=OMP ^
    -DDNNL_BUILD_TESTS=OFF ^
    -DDNNL_BUILD_EXAMPLES=OFF ^
    -DDNNL_LIBRARY_TYPE=STATIC ^
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5 ^
    "%ONEDNN_SRC%"

if errorlevel 1 (
    echo ERROR: CMake configuration failed.
    exit /b 1
)

:: 构建
echo.
echo Building oneDNN...
ninja -j12

if errorlevel 1 (
    echo ERROR: Build failed.
    exit /b 1
)

:: 安装
echo.
echo Installing oneDNN...
ninja install

if errorlevel 1 (
    echo ERROR: Install failed.
    exit /b 1
)

echo.
echo ============================================
echo oneDNN build complete!
echo Install directory: %ONEDNN_INSTALL%
echo Static library: %ONEDNN_INSTALL%\lib\libdnnl.a
echo ============================================

:: 验证输出
if exist "%ONEDNN_INSTALL%\lib\libdnnl.a" (
    echo [OK] libdnnl.a found
) else (
    echo [WARN] libdnnl.a not found, checking lib64...
    if exist "%ONEDNN_INSTALL%\lib64\libdnnl.a" (
        echo [OK] libdnnl.a found in lib64
    ) else (
        echo [ERROR] libdnnl.a not found!
    )
)

if exist "%ONEDNN_INSTALL%\include\dnnl.h" (
    echo [OK] dnnl.h found
) else (
    echo [ERROR] dnnl.h not found!
)

endlocal
