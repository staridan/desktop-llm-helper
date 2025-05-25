@echo off
setlocal

:: Run from a plain Command Prompt; uses Qt-installed MinGW 13.1.0 toolchain

:: Project root directory (trim trailing slash)
set "PROJECT_ROOT=%~dp0"
if "%PROJECT_ROOT:~-1%"=="\" set "PROJECT_ROOT=%PROJECT_ROOT:~0,-1%"

set "BUILD_DIR=%PROJECT_ROOT%\build"
set "DEPLOY_DIR=%BUILD_DIR%\deploy"
set "INSTALLER_DIR=%PROJECT_ROOT%\installer-output"

:: Paths to Qt (MinGW build) and Qt Installer Framework
set "QT_DIR=C:\Qt\6.8.1\mingw_64"
set "IFW_DIR=C:\Qt\Tools\QtInstallerFramework\4.9"

:: MinGW-w64 13.1.0 toolchain directory
set "MINGW_COMPILER_DIR=C:\Qt\Tools\mingw1310_64\bin"
if not exist "%MINGW_COMPILER_DIR%\g++.exe" (
    echo [Error] g++.exe not found in %MINGW_COMPILER_DIR%
    pause
    exit /b 1
)

:: Add MinGW to PATH and set make tool
set "PATH=%MINGW_COMPILER_DIR%;%PATH%"
set "MAKE_TOOL=%MINGW_COMPILER_DIR%\mingw32-make.exe"

:: Clean previous build
if exist "%BUILD_DIR%" rd /s /q "%BUILD_DIR%"
mkdir "%BUILD_DIR%"

:: Configure and build
pushd "%BUILD_DIR%"
cmake -G "MinGW Makefiles" ^
      -DCMAKE_BUILD_TYPE=Release ^
      -DCMAKE_C_COMPILER="%MINGW_COMPILER_DIR%\gcc.exe" ^
      -DCMAKE_CXX_COMPILER="%MINGW_COMPILER_DIR%\g++.exe" ^
      -DCMAKE_MAKE_PROGRAM="%MAKE_TOOL%" ^
      "%PROJECT_ROOT%"
if errorlevel 1 (
    echo [Error] CMake configuration failed.
    pause
    exit /b 1
)
"%MAKE_TOOL%"
if errorlevel 1 (
    echo [Error] Build failed.
    pause
    exit /b 1
)
popd

:: Deploy Qt dependencies
if exist "%DEPLOY_DIR%" rd /s /q "%DEPLOY_DIR%"
mkdir "%DEPLOY_DIR%"
"%QT_DIR%\bin\windeployqt.exe" --release "%BUILD_DIR%\DesktopLLMHelper.exe" --dir "%DEPLOY_DIR%"
if errorlevel 1 (
    echo [Error] windeployqt failed. Check QT_DIR path.
    pause
    exit /b 1
)

:: Verify installer configuration
if not exist "%PROJECT_ROOT%\installer\config\config.xml" (
    echo [Error] Installer config not found: %PROJECT_ROOT%\installer\config\config.xml
    pause
    exit /b 1
)
if not exist "%PROJECT_ROOT%\installer\packages" (
    echo [Error] Installer packages directory not found: %PROJECT_ROOT%\installer\packages
    pause
    exit /b 1
)

:: Create installer
if exist "%INSTALLER_DIR%" rd /s /q "%INSTALLER_DIR%"
mkdir "%INSTALLER_DIR%"
"%IFW_DIR%\bin\binarycreator.exe" ^
    --config "%PROJECT_ROOT%\installer\config\config.xml" ^
    --packages "%PROJECT_ROOT%\installer\packages" ^
    "%INSTALLER_DIR%\DesktopLLMHelperInstaller.exe"
if errorlevel 1 (
    echo [Error] Installer creation failed.
    pause
    exit /b 1
)

echo.
echo Build, deployment, and installer creation completed successfully!
pause