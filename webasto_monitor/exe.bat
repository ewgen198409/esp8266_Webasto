@echo off
pyinstaller --onefile --windowed --icon=webasto_256x256.ico webasto.py
if %errorlevel% == 0 (
    echo Build successful. Cleaning up build folder...
    rmdir /s /q build
) else (
    echo Build failed. Build folder preserved for debugging.
)
pause
