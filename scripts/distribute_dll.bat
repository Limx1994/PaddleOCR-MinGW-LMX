@echo off
setlocal
set SCRIPT_DIR=%~dp0
set ROOT_DIR=%SCRIPT_DIR%..
set DIST_DIR=%ROOT_DIR%\dist\ppocr
set DLL_SRC=%ROOT_DIR%\libs\paddle_inference_gcc\dll

echo ========================================
echo Distributing DLLs to dist\ppocr\
echo ========================================

:: 检查目标目录
if not exist "%DIST_DIR%" (
    echo Creating dist directory: %DIST_DIR%
    mkdir "%DIST_DIR%"
)

:: 复制 4 个 Paddle DLL
echo.
echo [1/4] Copying Paddle DLLs...
set PADDLE_DLL_OK=1
for %%d in (libcommon.dll libpir.dll libphi_core.dll libpaddle_inference.dll) do (
    if exist "%DLL_SRC%\%%d" (
        copy /Y "%DLL_SRC%\%%d" "%DIST_DIR%\" >nul
        echo   [OK] %%d
    ) else (
        echo   [MISSING] %%d - run build_paddle_dll.bat first!
        set PADDLE_DLL_OK=0
    )
)

if %PADDLE_DLL_OK%==0 (
    echo.
    echo ERROR: Some Paddle DLLs are missing. Run build_paddle_dll.bat first.
    exit /b 1
)

:: 复制 MinGW 运行时 DLL
echo.
echo [2/4] Copying MinGW runtime DLLs...
set MINGW_BIN=%ROOT_DIR%\toolchain\mingw\bin
for %%d in (libgcc_s_seh-1.dll libgomp-1.dll libstdc++-6.dll libwinpthread-1.dll) do (
    if exist "%MINGW_BIN%\%%d" (
        copy /Y "%MINGW_BIN%\%%d" "%DIST_DIR%\" >nul
        echo   [OK] %%d
    ) else (
        echo   [SKIP] %%d (not found in toolchain)
    )
)

:: 复制 OpenCV DLL
echo.
echo [3/4] Copying OpenCV DLL...
set OPENCV_DLL=%ROOT_DIR%\libs\opencv_install_gcc\x64\mingw\bin\libopencv_world470.dll
if exist "%OPENCV_DLL%" (
    copy /Y "%OPENCV_DLL%" "%DIST_DIR%\" >nul
    echo   [OK] libopencv_world470.dll
) else (
    echo   [MISSING] libopencv_world470.dll
)

:: 复制第三方库 DLL（从构建输出或安装目录）
echo.
echo [4/4] Copying third-party DLLs...
set THIRD_PARTY_DLL_OK=1

:: 从 DLL 构建目录复制
set DLL_BUILD=%ROOT_DIR%\src\Paddle\build_gcc_dll
if exist "%DLL_BUILD%\third_party\install" (
    for /r "%DLL_BUILD%\third_party\install" %%f in (*.dll) do (
        copy /Y "%%f" "%DIST_DIR%\" >nul 2>nul
        echo   [OK] %%~nxf
    )
)

:: 从静态构建目录复制（如果 DLL 构建目录没有）
set STATIC_BUILD=%ROOT_DIR%\src\Paddle\build_gcc
if exist "%STATIC_BUILD%\third_party\install" (
    for /r "%STATIC_BUILD%\third_party\install" %%f in (*.dll) do (
        if not exist "%DIST_DIR%\%%~nxf" (
            copy /Y "%%f" "%DIST_DIR%\" >nul 2>nul
            echo   [OK] %%~nxf (from static build)
        )
    )
)

:: 验证输出
echo.
echo ========================================
echo Distribution completed!
echo Target: %DIST_DIR%
echo.
echo Files in dist:
dir /b "%DIST_DIR%\*.dll" 2>nul | find /c /v "" >nul
for /f %%n in ('dir /b "%DIST_DIR%\*.dll" 2^>nul ^| find /c /v ""') do echo   DLL count: %%n
dir /b "%DIST_DIR%\*.exe" 2>nul | find /c /v "" >nul
for /f %%n in ('dir /b "%DIST_DIR%\*.exe" 2^>nul ^| find /c /v ""') do echo   EXE count: %%n
echo ========================================

endlocal
