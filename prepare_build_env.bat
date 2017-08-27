@echo off

set curDir=%~dp0

@call :GetWindowsSdkDir
@call :GetWindowsSdkExecutablePath
@call :GetExtensionSdkDir
@call :GetVCInstallDir
@call :GetUniversalCRTSdkDir

if not "%UniversalCRTSdkDir%" == "" @set UCRTContentRoot=%UniversalCRTSdkDir%

@call :amd64

@REM -----------------------------------------------------------------------
:GetWindowsSdkDir
@set WindowsSdkDir=
@set WindowsLibPath=
@set WindowsSDKVersion=
@set WindowsSDKLibVersion=winv6.3\
@call :GetWindowsSdkDirHelper32 HKLM > nul 2>&1
@if errorlevel 1 call :GetWindowsSdkDirHelper32 HKCU > nul 2>&1
@if errorlevel 1 call :GetWindowsSdkDirHelper64 HKLM > nul 2>&1
@if errorlevel 1 call :GetWindowsSdkDirHelper64 HKCU > nul 2>&1
@exit /B 0

:GetWindowsSdkDirHelper32
@REM Get Windows 10 SDK installed folder
@for /F "tokens=1,2*" %%i in ('reg query "%1\SOFTWARE\Microsoft\Microsoft SDKs\Windows\v10.0" /v "InstallationFolder"') DO (
	@if "%%i"=="InstallationFolder" (
		@set WindowsSdkDir=%%k
	)
)
@REM Get windows 10 sdk version number
@if not "%WindowsSdkDir%"=="" @FOR /F "delims=" %%i IN ('dir "%WindowsSdkDir%include\" /b /ad-h /on') DO @set WindowsSDKVersion=%%i\
@if not "%WindowsSDKVersion%"=="" @SET WindowsSDKLibVersion=%WindowsSDKVersion%
@if not "%WindowsSdkDir%"=="" @set WindowsLibPath=%WindowsSdkDir%UnionMetadata;%WindowsSdkDir%References;%WindowsSdkDir%References\Windows.Foundation.UniversalApiContract\1.0.0.0;%WindowsSdkDir%References\Windows.Foundation.FoundationContract\1.0.0.0;%WindowsSdkDir%References\indows.Networking.Connectivity.WwanContract\1.0.0.0

@REM Get Windows 8.1 SDK installed folder, if Windows 10 SDK is not installed
@if "%WindowsSdkDir%"=="" @for /F "tokens=1,2*" %%i in ('reg query "%1\SOFTWARE\Microsoft\Microsoft SDKs\Windows\v8.1" /v "InstallationFolder"') DO (
	@if "%%i"=="InstallationFolder" (
		@set WindowsSdkDir=%%k
	)
)
@if "%WindowsLibPath%"==""  @set WindowsLibPath=%WindowsSdkDir%References\CommonConfiguration\Neutral
@if "%WindowsSdkDir%"=="" exit /B 1
@exit /B 0

:GetWindowsSdkDirHelper64
@REM Get Windows 10 SDK installed folder
@for /F "tokens=1,2*" %%i in ('reg query "%1\SOFTWARE\Wow6432Node\Microsoft\Microsoft SDKs\Windows\v10.0" /v "InstallationFolder"') DO (
	@if "%%i"=="InstallationFolder" (
		@SET WindowsSdkDir=%%k
	)
)
@REM get windows 10 sdk version number
@if not "%WindowsSdkDir%"=="" @FOR /F "delims=" %%i IN ('dir "%WindowsSdkDir%include\" /b /ad-h /on') DO @SET WindowsSDKVersion=%%i\
@if not "%WindowsSDKVersion%"=="" @SET WindowsSDKLibVersion=%WindowsSDKVersion%
@if not "%WindowsSdkDir%"=="" @set WindowsLibPath=%WindowsSdkDir%UnionMetadata;%WindowsSdkDir%References;%WindowsSdkDir%References\Windows.Foundation.UniversalApiContract\1.0.0.0;%WindowsSdkDir%References\Windows.Foundation.FoundationContract\1.0.0.0;%WindowsSdkDir%References\indows.Networking.Connectivity.WwanContract\1.0.0.0

@REM Get Windows 8.1 SDK installed folder, if Windows 10 SDK is not installed
@if "%WindowsSdkDir%"=="" @for /F "tokens=1,2*" %%i in ('reg query "%1\SOFTWARE\Wow6432Node\Microsoft\Microsoft SDKs\Windows\v8.1" /v "InstallationFolder"') DO (
	@if "%%i"=="InstallationFolder" (
		@SET WindowsSdkDir=%%k
	)
)
@if "%WindowsLibPath%"==""  @set WindowsLibPath=%WindowsSdkDir%References\CommonConfiguration\Neutral
@if "%WindowsSdkDir%"=="" exit /B 1
@exit /B 0

@REM ---------------------------------------------------------------------------------------------------------------------------
:GetWindowsSdkExecutablePath
@set WindowsSDK_ExecutablePath_x86=
@set WindowsSDK_ExecutablePath_x64=
@set NETFXSDKDir=
@call :GetWindowsSdkExePathHelper HKLM\SOFTWARE > nul 2>&1
@if errorlevel 1 call :GetWindowsSdkExePathHelper HKCU\SOFTWARE > nul 2>&1
@if errorlevel 1 call :GetWindowsSdkExePathHelper HKLM\SOFTWARE\Wow6432Node > nul 2>&1
@if errorlevel 1 call :GetWindowsSdkExePathHelper HKCU\SOFTWARE\Wow6432Node > nul 2>&1
@exit /B 0

:GetWindowsSdkExePathHelper
@REM Get .NET 4.6.1 SDK tools and libs include path
@for /F "tokens=1,2*" %%i in ('reg query "%1\Microsoft\Microsoft SDKs\NETFXSDK\4.6.1\WinSDK-NetFx40Tools-x86" /v "InstallationFolder"') DO (
	@if "%%i"=="InstallationFolder" (
		@SET WindowsSDK_ExecutablePath_x86=%%k
	)
)

@for /F "tokens=1,2*" %%i in ('reg query "%1\Microsoft\Microsoft SDKs\NETFXSDK\4.6.1\WinSDK-NetFx40Tools-x64" /v "InstallationFolder"') DO (
	@if "%%i"=="InstallationFolder" (
		@SET WindowsSDK_ExecutablePath_x64=%%k
	)
)

@for /F "tokens=1,2*" %%i in ('reg query "%1\Microsoft\Microsoft SDKs\NETFXSDK\4.6.1" /v "KitsInstallationFolder"') DO (
	@if "%%i"=="KitsInstallationFolder" (
		@SET NETFXSDKDir=%%k
	)
)

@REM Falls back to get .NET 4.6 SDK tools and libs include path
@if "%NETFXSDKDir%"=="" @for /F "tokens=1,2*" %%i in ('reg query "%1\Microsoft\Microsoft SDKs\NETFXSDK\4.6\WinSDK-NetFx40Tools-x86" /v "InstallationFolder"') DO (
	@if "%%i"=="InstallationFolder" (
		@SET WindowsSDK_ExecutablePath_x86=%%k
	)
)

@if "%NETFXSDKDir%"=="" @for /F "tokens=1,2*" %%i in ('reg query "%1\Microsoft\Microsoft SDKs\NETFXSDK\4.6\WinSDK-NetFx40Tools-x64" /v "InstallationFolder"') DO (
	@if "%%i"=="InstallationFolder" (
		@SET WindowsSDK_ExecutablePath_x64=%%k
	)
)

@if "%NETFXSDKDir%"=="" @for /F "tokens=1,2*" %%i in ('reg query "%1\Microsoft\Microsoft SDKs\NETFXSDK\4.6" /v "KitsInstallationFolder"') DO (
	@if "%%i"=="KitsInstallationFolder" (
		@SET NETFXSDKDir=%%k
	)
)

@REM Falls back to use .NET 4.5.1 SDK
@if "%WindowsSDK_ExecutablePath_x86%"=="" @for /F "tokens=1,2*" %%i in ('reg query "%1\Microsoft\Microsoft SDKs\Windows\v8.1A\WinSDK-NetFx40Tools-x86" /v "InstallationFolder"') DO (
	@if "%%i"=="InstallationFolder" (
		@SET WindowsSDK_ExecutablePath_x86=%%k
	)
)

@if "%WindowsSDK_ExecutablePath_x64%"=="" @for /F "tokens=1,2*" %%i in ('reg query "%1\Microsoft\Microsoft SDKs\Windows\v8.1A\WinSDK-NetFx40Tools-x64" /v "InstallationFolder"') DO (
	@if "%%i"=="InstallationFolder" (
		@SET WindowsSDK_ExecutablePath_x64=%%k
	)
)

@if "%WindowsSDK_ExecutablePath_x86%"=="" @if "%WindowsSDK_ExecutablePath_x64%"=="" exit /B 1
@exit /B 0

@REM -----------------------------------------------------------------------TODO(ogaro): Delete.
:GetExtensionSdkDir
@set ExtensionSdkDir=

@REM Windows 8.1 Extension SDK
@if exist "%ProgramFiles%\Microsoft SDKs\Windows\v8.1\ExtensionSDKs\Microsoft.VCLibs\14.0\SDKManifest.xml" set ExtensionSdkDir=%ProgramFiles%\Microsoft SDKs\Windows\v8.1\ExtensionSDKs
@if exist "%ProgramFiles(x86)%\Microsoft SDKs\Windows\v8.1\ExtensionSDKs\Microsoft.VCLibs\14.0\SDKManifest.xml" set ExtensionSdkDir=%ProgramFiles(x86)%\Microsoft SDKs\Windows\v8.1\ExtensionSDKs

@REM Windows 10 Extension SDK, this will replace the Windows 8.1 "ExtensionSdkDir" if Windows 10 SDK is installed
@if exist "%ProgramFiles%\Microsoft SDKs\Windows Kits\10\ExtensionSDKs\Microsoft.VCLibs\14.0\SDKManifest.xml" set ExtensionSdkDir=%ProgramFiles%\Microsoft SDKs\Windows Kits\10\ExtensionSDKs
@if exist "%ProgramFiles(x86)%\Microsoft SDKs\Windows Kits\10\ExtensionSDKs\Microsoft.VCLibs\14.0\SDKManifest.xml" set ExtensionSdkDir=%ProgramFiles(x86)%\Microsoft SDKs\Windows Kits\10\ExtensionSDKs

@if "%ExtensionSdkDir%"=="" exit /B 1
@exit /B 0

@REM -----------------------------------------------------------------------
:GetVCInstallDir
@set VCINSTALLDIR=
@call :GetVCInstallDirHelper32 HKLM > nul 2>&1
@if errorlevel 1 call :GetVCInstallDirHelper32 HKCU > nul 2>&1
@if errorlevel 1 call :GetVCInstallDirHelper64  HKLM > nul 2>&1
@if errorlevel 1 call :GetVCInstallDirHelper64  HKCU > nul 2>&1
@exit /B 0

:GetVCInstallDirHelper32
@for /F "tokens=1,2*" %%i in ('reg query "%1\SOFTWARE\Microsoft\VisualStudio\SxS\VC7" /v "14.0"') DO (
	@if "%%i"=="14.0" (
		@SET VCINSTALLDIR=%%k
	)
)
@if "%VCINSTALLDIR%"=="" exit /B 1
@exit /B 0

:GetVCInstallDirHelper64
@for /F "tokens=1,2*" %%i in ('reg query "%1\SOFTWARE\Wow6432Node\Microsoft\VisualStudio\SxS\VC7" /v "14.0"') DO (
	@if "%%i"=="14.0" (
		@SET VCINSTALLDIR=%%k
	)
)
@if "%VCINSTALLDIR%"=="" exit /B 1
@exit /B 0

@REM -----------------------------------------------------------------------
:GetUniversalCRTSdkDir
@set UniversalCRTSdkDir=
@call :GetUniversalCRTSdkDirHelper64 HKLM > nul 2>&1
@if errorlevel 1 call :GetUniversalCRTSdkDirHelper64 HKCU > nul 2>&1
@if errorlevel 1 call :GetUniversalCRTSdkDirHelper32 HKLM > nul 2>&1
@if errorlevel 1 call :GetUniversalCRTSdkDirHelper32 HKCU > nul 2>&1
@exit /B 0

:GetUniversalCRTSdkDirHelper32
@for /F "tokens=1,2*" %%i in ('reg query "%1\SOFTWARE\Microsoft\Windows Kits\Installed Roots" /v "KitsRoot10"') DO (
	@if "%%i"=="KitsRoot10" (
		@SET UniversalCRTSdkDir=%%k
	)
)
@if "%UniversalCRTSdkDir%"=="" exit /B 1
@FOR /F "delims=" %%i IN ('dir "%UniversalCRTSdkDir%include\" /b /ad-h /on') DO @SET UCRTVersion=%%i
@exit /B 0

:GetUniversalCRTSdkDirHelper64
@for /F "tokens=1,2*" %%i in ('reg query "%1\SOFTWARE\Wow6432Node\Microsoft\Windows Kits\Installed Roots" /v "KitsRoot10"') DO (
	@if "%%i"=="KitsRoot10" (
		@SET UniversalCRTSdkDir=%%k
	)
)
@if "%UniversalCRTSdkDir%"=="" exit /B 1
@FOR /F "delims=" %%i IN ('dir "%UniversalCRTSdkDir%include\" /b /ad-h /on') DO @SET UCRTVersion=%%i
@exit /B 0

@REM -----------------------------------------------------------------------
:GetFrameworkDir64
@set FrameworkDir64=
@call :GetFrameworkDir64Helper32 HKLM > nul 2>&1
@if errorlevel 1 call :GetFrameworkDir64Helper32 HKCU > nul 2>&1
@if errorlevel 1 call :GetFrameworkDir64Helper64  HKLM > nul 2>&1
@if errorlevel 1 call :GetFrameworkDir64Helper64  HKCU > nul 2>&1
@exit /B 0

:GetFrameworkDir64Helper32
@for /F "tokens=1,2*" %%i in ('reg query "%1\SOFTWARE\Microsoft\VisualStudio\SxS\VC7" /v "FrameworkDir64"') DO (
	@if "%%i"=="FrameworkDir64" (
		@SET FrameworkDIR64=%%k
	)
)
@if "%FrameworkDIR64%"=="" exit /B 1
@exit /B 0

:GetFrameworkDir64Helper64
@for /F "tokens=1,2*" %%i in ('reg query "%1\SOFTWARE\Wow6432Node\Microsoft\VisualStudio\SxS\VC7" /v "FrameworkDir64"') DO (
	@if "%%i"=="FrameworkDir64" (
		@SET FrameworkDIR64=%%k
	)
)
@if "%FrameworkDIR64%"=="" exit /B 1
@exit /B 0

@REM -----------------------------------------------------------------------
:GetFrameworkVer64
@set FrameworkVer64=
@call :GetFrameworkVer64Helper32 HKLM > nul 2>&1
@if errorlevel 1 call :GetFrameworkVer64Helper32 HKCU > nul 2>&1
@if errorlevel 1 call :GetFrameworkVer64Helper64  HKLM > nul 2>&1
@if errorlevel 1 call :GetFrameworkVer64Helper64  HKCU > nul 2>&1
@exit /B 0

:GetFrameworkVer64Helper32
@for /F "tokens=1,2*" %%i in ('reg query "%1\SOFTWARE\Microsoft\VisualStudio\SxS\VC7" /v "FrameworkVer64"') DO (
	@if "%%i"=="FrameworkVer64" (
		@SET FrameworkVersion64=%%k
	)
)
@if "%FrameworkVersion64%"=="" exit /B 1
@exit /B 0

:GetFrameworkVer64Helper64
@for /F "tokens=1,2*" %%i in ('reg query "%1\SOFTWARE\Wow6432Node\Microsoft\VisualStudio\SxS\VC7" /v "FrameworkVer64"') DO (
	@if "%%i"=="FrameworkVer64" (
		@SET FrameworkVersion64=%%k
	)
)
@if "%FrameworkVersion64%"=="" exit /B 1
@exit /B 0

@rem ---------------------------------------------------------------------------------
:amd64

@call :GetFrameworkDir64
@call :GetFrameworkVer64

@set FrameworkDir=%FrameworkDir64%
@set FrameworkVersion=%FrameworkVersion64%

if not "%WindowsSDK_ExecutablePath_x64%" == "" @set PATH=%WindowsSDK_ExecutablePath_x64%;%PATH%

@rem
@rem Set Windows SDK include/lib path
@rem
if not "%WindowsSdkDir%" == "" @set PATH=%WindowsSdkDir%bin\x64;%WindowsSdkDir%bin\x86;%PATH%
if not "%WindowsSdkDir%" == "" @set INCLUDE=%WindowsSdkDir%include\%WindowsSDKVersion%shared;%WindowsSdkDir%include\%WindowsSDKVersion%um;%WindowsSdkDir%include\%WindowsSDKVersion%winrt;%INCLUDE%
if not "%WindowsSdkDir%" == "" @set LIB=%WindowsSdkDir%lib\%WindowsSDKLibVersion%um\x64;%LIB%
if not "%WindowsSdkDir%" == "" @set LIBPATH=%WindowsLibPath%;%ExtensionSDKDir%\Microsoft.VCLibs\14.0\References\CommonConfiguration\neutral;%LIBPATH%

@REM Set NETFXSDK include/lib path
@if not "%NETFXSDKDir%" == "" @set INCLUDE=%NETFXSDKDir%include\um;%INCLUDE%
@if not "%NETFXSDKDir%" == "" @set LIB=%NETFXSDKDir%lib\um\x64;%LIB%

@rem
@rem Set UniversalCRT include/lib path, the default is the latest installed version.
@rem
@if exist "%FrameworkDir%\%Framework40Version%" set PATH=%FrameworkDir%\%Framework40Version%;%PATH%
@if exist "%FrameworkDir%\%FrameworkVersion%" set PATH=%FrameworkDir%\%FrameworkVersion%;%PATH%
if not "%UCRTVersion%" == "" @set INCLUDE=%UniversalCRTSdkDir%include\%UCRTVersion%\ucrt;%INCLUDE%
if not "%UCRTVersion%" == "" @set LIB=%UniversalCRTSdkDir%lib\%UCRTVersion%\ucrt\x64;%LIB%

@rem PATH
@rem ----
if exist "%VCINSTALLDIR%VCPackages" set PATH=%VCINSTALLDIR%VCPackages;%PATH%
if exist "%VCINSTALLDIR%BIN\amd64" set PATH=%VCINSTALLDIR%BIN\amd64;%PATH%

if exist "%ProgramFiles%\MSBuild\14.0\bin\amd64" set PATH=%ProgramFiles%\MSBuild\14.0\bin\amd64;%PATH%
if exist "%ProgramFiles(x86)%\MSBuild\14.0\bin\amd64" set PATH=%ProgramFiles(x86)%\MSBuild\14.0\bin\amd64;%PATH%

@rem INCLUDE
@rem -------
if exist "%VCINSTALLDIR%ATLMFC\INCLUDE" set INCLUDE=%VCINSTALLDIR%ATLMFC\INCLUDE;%INCLUDE%
if exist "%VCINSTALLDIR%INCLUDE" set INCLUDE=%VCINSTALLDIR%INCLUDE;%INCLUDE%

@rem LIB
@rem ---
if exist "%VCINSTALLDIR%ATLMFC\LIB\amd64" set LIB=%VCINSTALLDIR%ATLMFC\LIB\amd64;%LIB%
if exist "%VCINSTALLDIR%LIB\amd64" set LIB=%VCINSTALLDIR%LIB\amd64;%LIB%

@rem LIBPATH
@rem -------
@if exist "%FrameworkDir%\%Framework40Version%" set LIBPATH=%FrameworkDir%\%Framework40Version%;%LIBPATH%
@if exist "%FrameworkDir%\%FrameworkVersion%" set LIBPATH=%FrameworkDir%\%FrameworkVersion%;%LIBPATH%
if exist "%VCINSTALLDIR%ATLMFC\LIB\amd64" set LIBPATH=%VCINSTALLDIR%ATLMFC\LIB\amd64;%LIBPATH%
if exist "%VCINSTALLDIR%LIB\amd64" set LIBPATH=%VCINSTALLDIR%LIB\amd64;%LIBPATH%

set Platform=X64
echo %PATH% > path.txt
echo %INCLUDE% > include.txt
echo %LIB% > lib.txt
echo %LIBPATH% > libpath.txt

@exit /B 0
