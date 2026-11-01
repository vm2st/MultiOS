@echo off
rem MultiOS | vm2 studios | t.me/v2vproj
where py >nul 2>nul
if errorlevel 1 goto fallback
py -3 "%~dp0customize.py" --gui
goto done
:fallback
python "%~dp0customize.py" --gui
:done
if errorlevel 1 pause
