@echo off
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0deployer_vps.ps1" %*
pause
