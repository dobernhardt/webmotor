#!/usr/bin/env python3
"""
WebMotor REST API Simulator

A Python-based simulator for the WebMotor ATOM S3 LITE device.
Serves the web UI and provides REST API endpoints for testing.
"""

import json
import logging
from datetime import datetime
from pathlib import Path
from typing import Dict, Any, Optional

from flask import Flask, request, jsonify, send_from_directory, send_file
from flask_cors import CORS
import jsonschema
from jsonschema import validate, ValidationError

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)

class MotorSimulator:
    """Simulates the motor controller state"""
    
    def __init__(self):
        self.frequency: int = 100  # Default frequency changed to 100
        self.direction: bool = False  # False=CCW, True=CW
        self.microsteps: int = 1
        self.mode: str = "STOPPED"
        self.connected: bool = True
        logger.info("Motor simulator initialized")
    
    def get_status(self) -> Dict[str, Any]:
        """Get current motor status"""
        status = {
            "frequency": self.frequency,
            "direction": self.direction,
            "microsteps": self.microsteps,
            "mode": self.mode,
            "connected": self.connected
        }
        logger.info(f"Motor status requested: {status}")
        return status
    
    def update_parameters(self, params: Dict[str, Any]) -> None:
        """Update motor parameters"""
        logger.info(f"Updating motor parameters: {params}")
        
        if "frequency" in params:
            self.frequency = params["frequency"]
            logger.info(f"Frequency set to {self.frequency} Hz")
        
        if "direction" in params:
            self.direction = params["direction"]
            direction_str = "CW" if self.direction else "CCW"
            logger.info(f"Direction set to {direction_str}")
        
        if "microsteps" in params:
            self.microsteps = params["microsteps"]
            logger.info(f"Microsteps set to {self.microsteps}")
        
        if "mode" in params:
            self.mode = params["mode"]
            logger.info(f"Mode set to {self.mode}")

class WiFiSimulator:
    """Simulates WiFi manager functionality"""
    
    def __init__(self):
        self.ssid: Optional[str] = "HomeNetwork"
        self.password: Optional[str] = None
        self.is_access_point: bool = False
        self.is_connected: bool = True
        self.ip_address: str = "192.168.1.100"
        logger.info("WiFi simulator initialized")
    
    def save_config(self, ssid: str, password: str) -> None:
        """Save WiFi configuration"""
        self.ssid = ssid
        self.password = password
        # Simulate connection after config
        self.is_access_point = False
        self.is_connected = True
        self.ip_address = f"192.168.1.{100 + hash(ssid) % 150}"
        logger.info(f"WiFi config saved - SSID: {ssid}, Password: {'*' * len(password)}")
    
    def get_status(self) -> Dict[str, Any]:
        """Get current WiFi status"""
        status = {
            "isAccessPoint": self.is_access_point,
            "isConnected": self.is_connected,
            "ssid": self.ssid if self.ssid else ("WebMotor-Config" if self.is_access_point else ""),
            "ipAddress": self.ip_address if (self.is_connected or self.is_access_point) else ""
        }
        logger.info(f"WiFi status requested: {status}")
        return status
    
    def set_mode(self, mode: str) -> None:
        """Set WiFi mode for testing"""
        if mode == "ap":
            self.is_access_point = True
            self.is_connected = False
            self.ssid = "WebMotor-Config"
            self.ip_address = "192.168.4.1"
        elif mode == "connected":
            self.is_access_point = False
            self.is_connected = True
            self.ssid = "HomeNetwork"
            self.ip_address = "192.168.1.100"
        elif mode == "disconnected":
            self.is_access_point = False
            self.is_connected = False
            self.ssid = ""
            self.ip_address = ""
        logger.info(f"WiFi mode set to {mode}")

class WebMotorSimulator:
    """Main simulator class"""
    
    # JSON Schema definitions for request validation
    MOTOR_CONTROL_SCHEMA = {
        "type": "object",
        "properties": {
            "frequency": {
                "type": "integer",
                "minimum": 0,
                "maximum": 10000
            },
            "direction": {
                "type": "boolean"
            },
            "microsteps": {
                "type": "integer",
                "enum": [1, 2, 4, 8, 16]
            },
            "mode": {
                "type": "string",
                "enum": ["RUNNING", "STOPPED", "RELEASED"]
            }
        },
        "minProperties": 1,
        "additionalProperties": False
    }
    
    WIFI_CONFIG_SCHEMA = {
        "type": "object",
        "properties": {
            "ssid": {
                "type": "string",
                "minLength": 1,
                "maxLength": 32
            },
            "password": {
                "type": "string",
                "minLength": 8,
                "maxLength": 63
            }
        },
        "required": ["ssid", "password"],
        "additionalProperties": False
    }
    
    def __init__(self):
        self.motor = MotorSimulator()
        self.wifi = WiFiSimulator()
        self.app = Flask(__name__)
        CORS(self.app)  # Enable CORS for web UI
        self._setup_routes()
        logger.info("WebMotor simulator initialized")
    
    def _setup_routes(self):
        """Setup Flask routes"""
        
        # Serve static web UI files
        @self.app.route('/')
        def index():
            return self._serve_static_file('index.html')
        
        @self.app.route('/<path:filename>')
        def static_files(filename):
            return self._serve_static_file(filename)
        
        # Motor status endpoint
        @self.app.route('/api/motor/status', methods=['GET'])
        def get_motor_status():
            try:
                status = self.motor.get_status()
                return jsonify(status), 200
            except Exception as e:
                logger.error(f"Error getting motor status: {e}")
                return jsonify({
                    "success": False,
                    "error": "Internal server error"
                }), 500
        
        # Motor control endpoint
        @self.app.route('/api/motor/control', methods=['POST'])
        def control_motor():
            try:
                # Validate request content type
                if not request.is_json:
                    return jsonify({
                        "success": False,
                        "error": "Content-Type must be application/json"
                    }), 400
                
                data = request.get_json()
                logger.info(f"Motor control request: {data}")
                
                # Validate request data
                try:
                    validate(instance=data, schema=self.MOTOR_CONTROL_SCHEMA)
                except ValidationError as e:
                    logger.warning(f"Invalid motor control request: {e.message}")
                    return jsonify({
                        "success": False,
                        "error": f"Invalid parameters: {e.message}"
                    }), 400
                
                # Update motor parameters
                self.motor.update_parameters(data)
                
                return jsonify({
                    "success": True,
                    "message": "Motor control command executed successfully"
                }), 200
                
            except Exception as e:
                logger.error(f"Error in motor control: {e}")
                return jsonify({
                    "success": False,
                    "error": "Internal server error"
                }), 500
        
        # WiFi configuration endpoint
        @self.app.route('/api/wifi/config', methods=['POST'])
        def configure_wifi():
            try:
                # Validate request content type
                if not request.is_json:
                    return jsonify({
                        "success": False,
                        "error": "Content-Type must be application/json"
                    }), 400
                
                data = request.get_json()
                logger.info(f"WiFi config request: {dict(data, password='***')}")
                
                # Validate request data
                try:
                    validate(instance=data, schema=self.WIFI_CONFIG_SCHEMA)
                except ValidationError as e:
                    logger.warning(f"Invalid WiFi config request: {e.message}")
                    return jsonify({
                        "success": False,
                        "error": f"Invalid parameters: {e.message}"
                    }), 400
                
                # Save WiFi configuration
                self.wifi.save_config(data["ssid"], data["password"])
                
                return jsonify({
                    "success": True,
                    "message": "WiFi configuration saved successfully"
                }), 200
                
            except Exception as e:
                logger.error(f"Error in WiFi configuration: {e}")
                return jsonify({
                    "success": False,
                    "error": "Internal server error"
                }), 500
        
        # WiFi status endpoint
        @self.app.route('/api/wifi/status', methods=['GET'])
        def get_wifi_status():
            try:
                status = self.wifi.get_status()
                return jsonify(status), 200
            except Exception as e:
                logger.error(f"Error getting WiFi status: {e}")
                return jsonify({
                    "success": False,
                    "error": "Internal server error"
                }), 500
        
        # Simulator control endpoint for testing WiFi modes
        @self.app.route('/simulator/wifi/set-mode', methods=['POST'])
        def set_wifi_mode():
            try:
                if not request.is_json:
                    return jsonify({
                        "success": False,
                        "error": "Content-Type must be application/json"
                    }), 400
                
                data = request.get_json()
                mode = data.get('mode', '')
                
                if mode not in ['ap', 'connected', 'disconnected']:
                    return jsonify({
                        "success": False,
                        "error": "Invalid mode. Use: ap, connected, or disconnected"
                    }), 400
                
                self.wifi.set_mode(mode)
                
                return jsonify({
                    "success": True,
                    "wifiState": self.wifi.get_status()
                }), 200
                
            except Exception as e:
                logger.error(f"Error setting WiFi mode: {e}")
                return jsonify({
                    "success": False,
                    "error": "Internal server error"
                }), 500
    
    def _serve_static_file(self, filename: str):
        """Serve static files from the data directory"""
        data_dir = Path(__file__).parent.parent / 'data'
        
        if not data_dir.exists():
            logger.error(f"Data directory not found: {data_dir}")
            return "Web UI files not found", 404
        
        file_path = data_dir / filename
        
        if not file_path.exists():
            logger.warning(f"File not found: {file_path}")
            return "File not found", 404
        
        try:
            return send_file(file_path)
        except Exception as e:
            logger.error(f"Error serving file {filename}: {e}")
            return "Error serving file", 500
    
    def run(self, host='127.0.0.1', port=8080, debug=False):
        """Start the simulator server"""
        wifi_status = f"Connected to {self.wifi.ssid}" if self.wifi.is_connected else "Disconnected"
        
        logger.info(f"Starting WebMotor simulator on http://{host}:{port}")
        logger.info("API endpoints:")
        logger.info("  GET  /api/motor/status")
        logger.info("  POST /api/motor/control")
        logger.info("  GET  /api/wifi/status")
        logger.info("  POST /api/wifi/config")
        logger.info("Simulator control endpoints:")
        logger.info("  POST /simulator/wifi/set-mode")
        logger.info(f"WiFi Status: {wifi_status}")
        logger.info(f"Motor Status: {self.motor.mode} ({self.motor.frequency} Hz)")
        logger.info("Web UI available at root URL")
        
        self.app.run(host=host, port=port, debug=debug)

def main():
    """Main entry point"""
    import argparse
    
    parser = argparse.ArgumentParser(description='WebMotor REST API Simulator')
    parser.add_argument('--host', default='127.0.0.1', 
                       help='Host to bind to (default: 127.0.0.1)')
    parser.add_argument('--port', type=int, default=8080,
                       help='Port to bind to (default: 8080)')
    parser.add_argument('--debug', action='store_true',
                       help='Enable debug mode')
    
    args = parser.parse_args()
    
    simulator = WebMotorSimulator()
    simulator.run(host=args.host, port=args.port, debug=args.debug)

if __name__ == '__main__':
    main()