@echo off
setlocal
set "APPDIR=%TEMP%\FMS-portable"
if exist "%APPDIR%" rmdir /s /q "%APPDIR%"
powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "Expand-Archive -LiteralPath '%~dp0FMS-portable.zip' -DestinationPath '%APPDIR%' -Force"
start "" "%APPDIR%\FMS.exe"
endlocal
