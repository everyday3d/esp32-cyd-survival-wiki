@echo off
rem Opens the point-and-click offline map downloader.
powershell -NoProfile -ExecutionPolicy Bypass -STA -File "%~dp0map-downloader-gui.ps1"
