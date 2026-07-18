# WebMotor

Schulprojekt: eine schwenkbare Plattform mit zwei positionsgeregelten Achsen
(Drehung um Z, Neigung um X), gesteuert von einem ESP32 über ein Cloud-Frontend.

## Hardware

- ESP32-PICO-KIT V4
- 2× Schrittmotor 28BYJ-48 mit ULN2003-Treiber (Drehachse und Neigeachse)
- Beide Achsen mit Untersetzungsgetriebe zwischen Motor und Plattform

Pinbelegung und alle Konstanten: [include/config.h](include/config.h).
Die Untersetzung wird dort pro Achse über `ROTATION_GEAR_RATIO` /
`TILT_GEAR_RATIO` (bzw. direkt `*_STEPS_PER_PLATFORM_DEG`) konfiguriert.
Alle Winkel im WebUI und in der Statusausgabe sind Plattformwinkel,
nicht Motorwinkel.

## Funktionsweise

- Beide Achsen fahren mit fester Schrittfrequenz auf ihre Zielposition
  (esp_timer) und halten dauerhaft ihr Haltemoment - die Neigeachse trägt Last.
- Einschaltposition = Mitte (0°); "Center" definiert die aktuelle Position
  als neue Mitte.
- Die Winkellimits gehören dem WebUI: Sie werden nicht auf dem Gerät
  persistiert, sondern nach jedem Boot per Cloud-Sync neu angewendet.
- WLAN-Setup: Ohne gespeicherte Zugangsdaten öffnet der ESP den Hotspot
  "WebMotor-Config" mit Captive Portal. Im Normalbetrieb liefert er nur eine
  Statusseite (inkl. Cloud-Konfiguration) - die Steuerung läuft über das
  Cloud-Frontend.

## Cloud

- Azure Static Web App: statisches Frontend ([cloud-frontend/](cloud-frontend/))
  plus Python Azure Functions API ([api/](api/))
- Queue Storage für diskrete Kommandos (stop, center), Table Storage für
  Joystick-Target, Achsen-Limits und Gerätestatus
- Der ESP32 pollt `GET /api/sync` und pusht seinen Status; das Frontend
  sendet Targets und Limits
- Deployment automatisch per GitHub Actions
  ([azure-static-web-apps.yml](.github/workflows/azure-static-web-apps.yml))
- Frontend: https://calm-river-0a48e7503.6.azurestaticapps.net

## Entwicklung

Firmware bauen und flashen mit PlatformIO (Board-Umgebung `esp32_pico_kit`):

```bash
pio run              # bauen
pio run -t upload    # flashen
pio device monitor   # serielle Ausgabe (115200 Baud)
```

WLAN-Zugangsdaten können als Build-Flags in [platformio.ini](platformio.ini)
gesetzt werden; ansonsten läuft das Setup über das Captive Portal.
Die Versionsinfo (`include/version.h`) erzeugt [generate_version.py](generate_version.py)
beim Build aus GitVersion.

## Struktur

```
include/, src/     ESP32-Firmware (PlatformIO, Arduino-Framework)
api/               Azure Functions Backend (Python)
cloud-frontend/    Cloud-WebUI (statisches HTML/JS/CSS)
```
