@echo off
rem SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
rem SPDX-License-Identifier: GPL-3.0-or-later
rem
rem Red Storm Rising: the launch screen (CRT look, fullscreen or a window),
rem then the game. Put RedStormRising_v1.0_1603.lha next to this file (see
rem README.txt). Options skip the launch screen, for example:
rem   run.bat --fullscreen --crt-lottes-fast
cd /d "%~dp0"
rsr.exe %*
if errorlevel 1 pause
