@echo off
title IoT Attendance Server - CT295
cd /d "%~dp0server"
echo ========================================================
echo      KHOI DONG SERVER DIEM DANH VA GHI DANH IOT
echo ========================================================
echo.
echo Dashboard Web: http://localhost:3000
echo Endpoint ESP32: http://192.168.137.1:3000/api/attendance/scan
echo.
node dist/index.js
pause
