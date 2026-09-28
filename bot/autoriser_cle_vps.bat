@echo off
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0autoriser_cle_vps.ps1" %*
pause
