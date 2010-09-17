@echo off
echo.
echo Please use PowerShell script configure.ps1 instead.
echo.
echo You also must enable local scripts by:
echo set-executionpolicy remotesigned
echo.
echo Trying to run ".\configure.ps1 -h" (without the quotes) to see help.
echo Launching powershell:
powershell -ExecutionPolicy remotesigned -NoExit
