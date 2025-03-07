@ECHO off
set DIR=%1

set "DIR=%DIR:/=\%"

:: Save Orignal Working Path
set WORK_PATH=%CD%

:: For 'git' command Working PATH should enter submoudle directory
CD %DIR%

:: Changes the active console code page to US English
chcp 437

echo /* Do not change the content here, it's auto generated */ >  %DIR%\WIFI_IOT\common\_wifi_version.h
echo /* Do not change the content here, it's auto generated */ >  %DIR%\Firmware\GD32W51x_wifi_driver\osal\wifi_driver_build_date.h

REM Check whether git is configured
git --version 2> NUL 1>&2
if %ERRORLEVEL% neq 0 goto end

REM WiFi release version
set cmd="git describe --tags"
FOR /F "tokens=*" %%i IN (' %cmd% ') DO SET tag=%%i

REM WiFi build revision
set cmd="git log --format="%%H" -n 1"
FOR /F "tokens=*" %%i IN (' %cmd% ') DO SET revision=%%i

echo #define WIFI_GIT_REVISION "%tag%-%revision:~0,16%" >> %DIR%\WIFI_IOT\common\_wifi_version.h
:: echo #define WIFI_RELEASE_VERSION "%tag%" >> %DIR%\WIFI_IOT\common\_wifi_version.h
:: echo #define WIFI_BUILD_REVISION"%revision%" >> %DIR%\WIFI_IOT\common\_wifi_version.h

::REM WiFi build date
::echo #define WIFI_BUILD_DATE "%DATE:~-10% %TIME:~0,-3%" >> %DIR%\WIFI_IOT\common\_wifi_version.h
echo #define WIFI_DRIVER_BUILD_DATE "%DATE:~-10%" >> %DIR%\Firmware\GD32W51x_wifi_driver\osal\wifi_driver_build_date.h

echo #define WIFI_VERSION_GENERATED >> %DIR%\WIFI_IOT\common\_wifi_version.h

:: Recovery Working Path
CD %WORK_PATH%

:end
