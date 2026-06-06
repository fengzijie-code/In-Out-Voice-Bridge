@echo off
setlocal

echo ============================================
echo  In Out Voice Bridge - Installer Build
echo ============================================
echo.

:: Find MSBuild
set "MSBUILD="
if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" (
    set "MSBUILD=C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
)
if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe" (
    set "MSBUILD=C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe"
)
if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe" (
    set "MSBUILD=C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe"
)

if "%MSBUILD%"=="" (
    echo ERROR: Could not find MSBuild. Make sure Visual Studio 2022 is installed.
    exit /b 1
)

:: Find Inno Setup
set "ISCC="
if exist "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" (
    set "ISCC=C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
)
if exist "C:\Program Files\Inno Setup 6\ISCC.exe" (
    set "ISCC=C:\Program Files\Inno Setup 6\ISCC.exe"
)

if "%ISCC%"=="" (
    echo ERROR: Could not find Inno Setup 6. Download from https://jrsoftware.org/isinfo.php
    exit /b 1
)

cd /d "%~dp0\.."

:: Step 1: Build C++ Audio Engine DLL (Release)
echo [1/4] Building C++ Audio Engine (Release x64)...
"%MSBUILD%" "src\InOutVoiceBridge.AudioEngine\InOutVoiceBridge.AudioEngine.vcxproj" /p:Configuration=Release /p:Platform=x64 /v:minimal
if errorlevel 1 (
    echo ERROR: C++ build failed.
    exit /b 1
)
echo      OK
echo.

:: Step 2: Publish .NET app (self-contained)
echo [2/4] Publishing .NET app (self-contained, win-x64)...
dotnet publish "src\InOutVoiceBridge.App\InOutVoiceBridge.App.csproj" -c Release -r win-x64 --self-contained true -o publish /p:PublishSingleFile=false
if errorlevel 1 (
    echo ERROR: dotnet publish failed.
    exit /b 1
)
echo      OK
echo.

:: Step 3: Copy C++ DLL to publish folder
echo [3/4] Copying AudioEngine DLL to publish folder...
copy /Y "src\InOutVoiceBridge.AudioEngine\x64\Release\InOutVoiceBridge.AudioEngine.dll" "publish\" >nul
if errorlevel 1 (
    echo ERROR: Failed to copy DLL.
    exit /b 1
)
echo      OK
echo.

:: Step 4: Build installer
echo [4/4] Building installer with Inno Setup...
"%ISCC%" "installer\setup.iss"
if errorlevel 1 (
    echo ERROR: Inno Setup build failed.
    exit /b 1
)
echo.
echo ============================================
echo  SUCCESS! Installer created at:
echo  installer\Output\InOutVoiceBridgeSetup.exe
echo ============================================
pause
