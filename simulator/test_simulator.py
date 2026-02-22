#!/usr/bin/env python3
"""
Test script for WebMotor simulator with new WiFi status functionality
"""

import requests
import json
import time

def test_simulator(base_url="http://localhost:3000"):
    """Test all simulator endpoints"""
    
    print("🧪 Testing WebMotor Simulator")
    print("=" * 50)
    
    # Test motor status
    print("\n1. Testing Motor Status:")
    try:
        response = requests.get(f"{base_url}/api/motor/status")
        print(f"   Status: {response.status_code}")
        print(f"   Response: {json.dumps(response.json(), indent=2)}")
    except Exception as e:
        print(f"   Error: {e}")
    
    # Test WiFi status (NEW)
    print("\n2. Testing WiFi Status:")
    try:
        response = requests.get(f"{base_url}/api/wifi/status")
        print(f"   Status: {response.status_code}")
        print(f"   Response: {json.dumps(response.json(), indent=2)}")
    except Exception as e:
        print(f"   Error: {e}")
    
    # Test motor control with debounced frequency
    print("\n3. Testing Motor Control (Frequency):")
    try:
        data = {"frequency": 150}
        response = requests.post(f"{base_url}/api/motor/control", 
                               json=data,
                               headers={"Content-Type": "application/json"})
        print(f"   Status: {response.status_code}")
        print(f"   Response: {json.dumps(response.json(), indent=2)}")
    except Exception as e:
        print(f"   Error: {e}")
    
    # Test WiFi mode switching
    print("\n4. Testing WiFi Mode Switching:")
    modes = ["ap", "connected", "disconnected"]
    
    for mode in modes:
        try:
            print(f"\n   Testing mode: {mode}")
            data = {"mode": mode}
            response = requests.post(f"{base_url}/simulator/wifi/set-mode",
                                   json=data,
                                   headers={"Content-Type": "application/json"})
            print(f"   Status: {response.status_code}")
            if response.status_code == 200:
                wifi_state = response.json().get("wifiState", {})
                print(f"   WiFi State: {json.dumps(wifi_state, indent=4)}")
            
            # Check status after mode change
            time.sleep(0.5)
            status_response = requests.get(f"{base_url}/api/wifi/status")
            print(f"   Current Status: {json.dumps(status_response.json(), indent=4)}")
            
        except Exception as e:
            print(f"   Error: {e}")
    
    print("\n✅ Simulator testing completed!")
    print("\nTo test the web UI:")
    print(f"   Open: {base_url}")
    print("   Check that:")
    print("   - Default frequency is 100 Hz")
    print("   - WiFi status is displayed")
    print("   - Frequency slider is debounced")

if __name__ == "__main__":
    test_simulator()