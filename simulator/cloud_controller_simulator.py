#!/usr/bin/env python3
"""
WebMotor Cloud Controller Simulator

Simulates an ESP32 controller connecting to the Azure cloud backend.
- Polls for commands from the cloud (long polling)
- Pushes motor state updates to the cloud
- Simulates motor position changes based on commands
"""

import requests
import json
import time
import logging
from datetime import datetime
from typing import Dict, Any, Optional
import argparse

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)


class CloudMotorController:
    """Simulates a motor controller connected to Azure cloud backend"""
    
    def __init__(self, api_endpoint: str, api_key: str):
        self.api_endpoint = api_endpoint.rstrip('/')
        self.api_key = api_key
        self.headers = {
            'X-API-Key': api_key,
            'Content-Type': 'application/json'
        }
        
        # Motor state
        self.frequency = 100  # Hz
        self.direction = True  # True=CW, False=CCW
        self.microsteps = 16  # 1/16 microstepping
        self.mode = "STOPPED"  # STOPPED, RUNNING, RELEASED
        self.position = 0  # Current position in steps
        
        # Simulation parameters
        self.last_state_push = 0
        self.state_push_interval = 2.0  # Push state every 2 seconds
        self.running = True
        
        logger.info(f"Cloud controller initialized - API: {self.api_endpoint}")
    
    def get_current_state(self) -> Dict[str, Any]:
        """Get current motor state"""
        return {
            "frequency": self.frequency,
            "direction": "CW" if self.direction else "CCW",
            "microsteps": f"1/{self.microsteps}",
            "mode": self.mode,
            "position": self.position,
            "timestamp": datetime.now().isoformat()
        }
    
    def push_state(self) -> bool:
        """Push current state to cloud backend"""
        try:
            state = self.get_current_state()
            url = f"{self.api_endpoint}/state"
            
            response = requests.post(url, headers=self.headers, json=state, timeout=10)
            
            if response.status_code == 200:
                logger.debug(f"State pushed successfully: {state}")
                return True
            else:
                logger.error(f"Failed to push state: HTTP {response.status_code} - {response.text}")
                return False
                
        except Exception as e:
            logger.error(f"Error pushing state: {e}")
            return False
    
    def poll_commands(self) -> Optional[Dict[str, Any]]:
        """Poll for commands from cloud backend (long polling)"""
        try:
            url = f"{self.api_endpoint}/commands/poll"
            
            # Long poll with 35 second timeout (backend waits 30s)
            response = requests.get(url, headers=self.headers, timeout=35)
            
            if response.status_code == 200:
                data = response.json()
                command = data.get('command')
                
                if command:
                    logger.info(f"Command received: {command}")
                    return command
                else:
                    logger.debug("No commands available")
                    return None
            else:
                logger.error(f"Failed to poll commands: HTTP {response.status_code} - {response.text}")
                return None
                
        except requests.exceptions.Timeout:
            # Timeout is expected with long polling
            logger.debug("Poll timeout (normal)")
            return None
        except Exception as e:
            logger.error(f"Error polling commands: {e}")
            return None
    
    def process_command(self, command: Dict[str, Any]) -> None:
        """Process a command from the cloud"""
        try:
            action = command.get('action')
            parameters = command.get('parameters', {})
            
            logger.info(f"Processing command - Action: {action}, Parameters: {parameters}")
            
            # Apply parameters
            if 'frequency' in parameters:
                self.frequency = int(parameters['frequency'])
                logger.info(f"  → Frequency: {self.frequency} Hz")
            
            if 'direction' in parameters:
                direction_str = parameters['direction']
                self.direction = direction_str == "CW"
                logger.info(f"  → Direction: {direction_str}")
            
            if 'microsteps' in parameters:
                microsteps_str = parameters['microsteps']
                # Parse "1/16" format
                if '/' in str(microsteps_str):
                    self.microsteps = int(str(microsteps_str).split('/')[-1])
                else:
                    self.microsteps = int(microsteps_str)
                logger.info(f"  → Microsteps: 1/{self.microsteps}")
            
            # Apply mode/action
            if action == 'start':
                self.mode = "RUNNING"
                logger.info("  → Motor STARTED")
            elif action == 'stop':
                self.mode = "STOPPED"
                logger.info("  → Motor STOPPED")
            elif action == 'release':
                self.mode = "RELEASED"
                logger.info("  → Motor RELEASED")
            elif action == 'frequency':
                # Just frequency update, keep current mode
                pass
            elif action == 'direction':
                # Just direction update, keep current mode
                pass
            elif action == 'microsteps':
                # Just microsteps update, keep current mode
                pass
            
        except Exception as e:
            logger.error(f"Error processing command: {e}")
    
    def update_position(self, delta_time: float) -> None:
        """Update motor position based on current state"""
        if self.mode == "RUNNING" and self.frequency > 0:
            # Calculate steps moved in delta_time
            steps_per_second = self.frequency * self.microsteps
            steps = int(steps_per_second * delta_time)
            
            if self.direction:  # CW
                self.position += steps
            else:  # CCW
                self.position -= steps
            
            logger.debug(f"Position updated: {self.position} (Δ{steps} steps)")
    
    def test_connection(self) -> bool:
        """Test connection to cloud backend"""
        try:
            url = f"{self.api_endpoint}/health"
            response = requests.get(url, timeout=5)
            
            if response.status_code == 200:
                logger.info("✓ Connection successful - Backend is healthy")
                return True
            else:
                logger.error(f"✗ Connection failed - HTTP {response.status_code}")
                return False
                
        except Exception as e:
            logger.error(f"✗ Connection failed: {e}")
            return False
    
    def run(self) -> None:
        """Main control loop"""
        logger.info("Starting cloud controller simulator...")
        
        # Test connection first
        if not self.test_connection():
            logger.error("Failed to connect to backend. Please check API endpoint and key.")
            return
        
        logger.info("Controller is running. Press Ctrl+C to stop.")
        
        last_position_update = time.time()
        
        try:
            while self.running:
                current_time = time.time()
                
                # Update position simulation
                delta_time = current_time - last_position_update
                self.update_position(delta_time)
                last_position_update = current_time
                
                # Push state periodically
                if current_time - self.last_state_push >= self.state_push_interval:
                    self.push_state()
                    self.last_state_push = current_time
                
                # Poll for commands (this blocks for up to 30 seconds)
                command = self.poll_commands()
                
                if command:
                    self.process_command(command)
                    # Push state immediately after processing command
                    self.push_state()
                    self.last_state_push = current_time
                
        except KeyboardInterrupt:
            logger.info("\nShutting down controller simulator...")
        except Exception as e:
            logger.error(f"Unexpected error: {e}")
        finally:
            # Push final state
            logger.info("Pushing final state...")
            self.mode = "STOPPED"
            self.push_state()


def main():
    """Main entry point"""
    parser = argparse.ArgumentParser(
        description='WebMotor Cloud Controller Simulator',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Use deployed Azure Static Web App API
  python cloud_controller_simulator.py \\
    --endpoint https://calm-river-0a48e7503.6.azurestaticapps.net/api \\
    --key YOUR_API_KEY
  
  # Use relative endpoint (testing locally with Azure SWA)
  python cloud_controller_simulator.py \\
    --endpoint /api \\
    --key YOUR_API_KEY
        """
    )
    
    parser.add_argument(
        '--endpoint',
        type=str,
        default='https://calm-river-0a48e7503.6.azurestaticapps.net/api',
        help='Azure Function API endpoint'
    )
    
    parser.add_argument(
        '--key',
        type=str,
        required=True,
        help='API key for authentication'
    )
    
    parser.add_argument(
        '--verbose',
        '-v',
        action='store_true',
        help='Enable verbose debug logging'
    )
    
    args = parser.parse_args()
    
    # Set logging level
    if args.verbose:
        logger.setLevel(logging.DEBUG)
    
    # Create and run simulator
    controller = CloudMotorController(args.endpoint, args.key)
    controller.run()


if __name__ == '__main__':
    main()
