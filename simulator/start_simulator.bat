@echo off
REM Windows batch file to run the WebMotor simulator

echo =============================================
echo WebMotor REST API Simulator
echo =============================================
echo Educational project for ATOM S3 LITE testing
echo.

REM Check if Python is installed
python --version >nul 2>&1
if errorlevel 1 (
    echo Error: Python is not installed or not in PATH
    echo Please install Python 3.8 or later
    pause
    exit /b 1
)

REM Check if pip is available
pip --version >nul 2>&1
if errorlevel 1 (
    echo Error: pip is not available
    echo Please ensure pip is installed with Python
    pause
    exit /b 1
)

REM Install requirements if not already installed
echo Checking Python dependencies...
python -c "import flask, flask_cors, jsonschema" >nul 2>&1
if errorlevel 1 (
    echo Installing required packages...
    pip install -r requirements.txt
    if errorlevel 1 (
        echo Error: Failed to install requirements
        pause
        exit /b 1
    )
)

REM Run the simulator
echo Starting WebMotor simulator...
echo Open http://127.0.0.1:8080 in your web browser
echo Press Ctrl+C to stop the simulator
echo.

python run_simulator.py

pause