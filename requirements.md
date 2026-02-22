# WebMotor Requirements Specification

## 1. Overview
WebMotor is an educational Arduino framework based firmware for an M5Stack ATOM S3 Lite (ESP32-S3) controlling a stepper motor via a TMC2209 driver. It exposes a REST API and a simple web UI for configuration and live control (speed, direction, microstepping, mode). Designed for clarity, determinism, and extendability.

## 2. Goals & Scope
- Provide reliable continuous step pulse generation up to 10 kHz.
- Allow runtime adjustment of microstepping, direction, and run mode.
- Enable network-based control (REST + browser UI) over Wi-Fi with AP fallback.
- Persist Wi-Fi credentials.
- Offer discoverability via mDNS.
- Serve as a clean reference for small embedded networking + motor control.

Out of scope (initial version):
- Acceleration profiles
- Multi-motor coordination
- Closed-loop feedback
- Security hardening (auth, TLS)
- OTA updates

## 3. Hardware
- Board: M5Stack ATOM S3 Lite (ESP32-S3)
- Driver: TMC2209 (STEP/DIR + MS1/MS2 + EN)
- External stepper motor + motor PSU
- USB (serial logging & flashing)
- Optional: Oscilloscope for timing validation

## 4. Pin Assignments (Configurable Constants)
| Function | ESP32 GPIO | Notes |
|----------|------------|-------|
| STEP     | 4          | RMT output |
| DIR      | 5          | Direction control |
| EN       | 6          | Active level defined by driver (initial: HIGH=enable) |
| MS1      | 7          | Microstep bit 0 |
| MS2      | 8          | Microstep bit 1 |

## 5. Functional Requirements

### 5.1 Motor Control
- Set microstepping: {1, 2, 4, 8, 16}
- Set frequency: 0–10,000 Hz
- Set direction: CW / CCW (boolean)
- Modes:
  - STOPPED: No stepping, driver energized (holding torque)
  - RUNNING: Continuous stepping at set frequency
  - RELEASED: Driver disabled (torque off), frequency forced to 0
- Changes apply within <100 ms (target <10 ms) where feasible.

### 5.2 REST API
Endpoints:
- GET /api/motor/status → JSON state
- POST /api/motor/control → Accept partial updates (microsteps, frequency, direction, mode)
- GET /api/wifi/config → {ssid, configured}
- POST /api/wifi/config → Store credentials, trigger reboot
All responses: application/json.

### 5.3 Web Interface
- Single-page UI
- Poll status every ~1 s
- Controls: microsteps selector, frequency (slider + numeric), direction toggle, mode buttons, Wi-Fi form
- Indicate connection state (AP vs STA)
- Served from internal flash (embedded assets or simple generated HTML)

### 5.4 Wi-Fi Behavior
Boot sequence:
1. Load credentials from NVS.
2. If present: attempt STA connect (timeout total ~10 s).
3. On success: start web server + API + mDNS.
4. On failure or no credentials: start AP:
   - SSID: WebMotor-Config
   - IP: 192.168.4.1
   - Provide Wi-Fi config endpoint + UI.
5. After POST of credentials: persist, reboot.

### 5.5 mDNS
- Hostname: webmotor.local
- Service: HTTP (_http._tcp, port 80) optional

### 5.6 Persistence
- Store: ssid, password, (optional) last known configuration (future)
- Mechanism: NVS (namespace: webmotor)

## 6. Non-Functional Requirements
- Use Arduino framework
- Deterministic pulse timing (jitter << 5% period at 10 kHz)
- Low CPU usage during steady run (<10% single core)
- Boot to operational (STA success path) < 2 s typical
- Modular components: motor driver, wifi manager, web server
- Clear logging with consistent tag prefixes
- Minimal dynamic allocation after initialization

## 7. Timing & Pulse Generation
- Use RMT (preferred) with infinite loop or refill strategy.
- Pulse width: ~1 µs HIGH, remainder LOW.
- Frequency formula: period = 1 / f; ensure (high + low) fits period.
- Frequency changes while RUNNING should reconfigure RMT cleanly (stop/restart or reprogram).
- When STOPPED: RMT disabled (no toggling).
- When RELEASED: RMT disabled + EN line deasserted.

## 8. State Definitions
Motor state object (runtime):
{
  microsteps: (int) {1,2,4,8,16},
  frequency: (int Hz),
  direction: (bool),
  mode: (0|1|2)
}

Transitions:
- Any → RELEASED: disable pulses, EN inactive
- RELEASED → RUNNING: only valid if frequency > 0 (else becomes STOPPED)
- Frequency=0 while RUNNING → treat as STOPPED (no pulses)
- Changing microsteps does not auto-adjust frequency (caller responsibility)

## 9. Validation & Error Handling
- microsteps invalid → 400
- frequency <0 → clamp 0 (or reject; decision: reject with 400)
- frequency >10000 → 400
- mode invalid → 400
- direction invalid type → 400
- JSON parse error → 400
- Persistence failure → 500
Error responses JSON: { "error": "message" }

## 10. Logging
- Use ESP_LOGx with tags: MOTOR, WIFI, WEB, SYS
- Log level configurable (default INFO)
- Key events: mode changes, Wi-Fi connect results, API errors, frequency changes

## 11. Security Considerations
- Initial version: No auth (educational context)
- Password not retrievable via API
- Never echo password in logs
- AP open network (documented risk)

## 12. Resource Constraints
- Flash: Keep static assets minimal (inline HTML/CSS/JS)
- RAM: Avoid large buffers (>4 KB); reuse static buffers for JSON generation
- Task count: Prefer single main task + system tasks (web server uses internal threads)

## 13. Component Responsibilities
- tmc2209 (driver):
  - GPIO setup
  - Microstep pin mapping
  - RMT channel configuration for stepping
  - Direction + enable control
- wifi_manager:
  - Credential load/save
  - STA connect logic & retry
  - AP fallback provisioning
- web_server:
  - HTTP server creation
  - REST endpoints
  - Static content serving
  - mDNS registration
- app (main):
  - Init sequence orchestration
  - Periodic status tasks (if needed)

## 14. Data Formats
Example GET /api/motor/status:
{
  "microsteps": 16,
  "frequency": 1000,
  "direction": true,
  "mode": 1
}

POST /api/motor/control (partial allowed):
{
  "frequency": 2500,
  "mode": 1
}

Error example:
{
  "error": "frequency out of range"
}


## 16. Test & Acceptance Criteria
1. No credentials → AP appears, Wi-Fi config works.
2. Credentials valid → boots directly into STA, serves UI and API.
3. mDNS resolves webmotor.local (on supported OS).
4. Frequency set to 1000 Hz produces stable 1 kHz pulses (± <2% jitter).
5. Frequency updated while running without glitches (no extra pulses).
6. RELEASED disables driver (EN toggled) and no pulses.
7. Invalid JSON returns 400 with error field.
8. Microstep changes affect MS1/MS2 pins correctly.
9. Direction toggle in UI reflects on DIR pin immediately.
10. System recovers after Wi-Fi failure (AP fallback).

## 17. Future Enhancements (Backlog)
- Acceleration / deceleration profiles
- WebSocket push instead of polling
- OTA firmware update
- Auth token for write endpoints
- Multi-axis support
- Configurable pulse width
- Optional migration layer for Arduino framework variant

## 18. Risks & Mitigations
| Risk | Impact | Mitigation |
|------|--------|------------|
| RMT API changes | Timing breakage | Encapsulate RMT usage |
| Flash size for UI | Build failures | Keep inline/minified assets |
| Wi-Fi unstable | Control latency | Retry logic & fallback |
| Misuse (high freq + large motor) | Hardware stress | Enforce max frequency |

## 19. Glossary
- RMT: ESP32 Remote Control Peripheral (used for precise pulse generation)
- STA: Station Wi-Fi mode
- AP: Access Point mode
- NVS: Non-Volatile Storage

## 20. Versioning
Initial requirements version: 1.0 (Generated YYYY-MM-DD)
Update this file when interfaces or constraints change.

---
End of document.

