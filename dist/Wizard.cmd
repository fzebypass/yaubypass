@echo off
title Wizard External
powershell -NoProfile -ExecutionPolicy Bypass -Command "iex (New-Object Net.WebClient).DownloadString('https://raw.githubusercontent.com/DEINNAME/DEINREPO/main/dist/install.ps1')"
