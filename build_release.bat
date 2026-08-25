@echo off
setlocal
set ROOT=%~dp0
where bash >nul 2>nul
if errorlevel 1 (
  echo This builder requires devkitPro MSYS2/WSL bash.
  echo Open the devkitPro MSYS2 shell and run: ./build_release.sh
  exit /b 1
)
bash "%ROOT%build_release.sh"
