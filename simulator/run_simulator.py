#!/usr/bin/env python3
"""
Simple script to run the WebMotor simulator with default settings
"""

import sys
import subprocess
from pathlib import Path

def check_requirements():
    """Check if required packages are installed"""
    try:
        import flask
        import flask_cors
        import jsonschema
        return True
    except ImportError as e:
        print(f"Missing required package: {e}")
        print("Please install requirements:")
        print("  pip install -r requirements.txt")
        return False

def main():
    if not check_requirements():
        sys.exit(1)
    
    # Import and run simulator
    from webmotor_simulator import WebMotorSimulator
    
    print("=" * 50)
    print("WebMotor REST API Simulator")
    print("=" * 50)
    print("Educational project for ATOM S3 LITE testing")
    print()
    
    simulator = WebMotorSimulator()
    
    try:
        simulator.run(host='127.0.0.1', port=8080, debug=True)
    except KeyboardInterrupt:
        print("\nSimulator stopped by user")
    except Exception as e:
        print(f"Error running simulator: {e}")
        sys.exit(1)

if __name__ == '__main__':
    main()