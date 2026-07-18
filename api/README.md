# WebMotor Cloud Backend (Azure Functions)

Python Azure Functions API zwischen Cloud-Frontend und ESP32.
Queue Storage transportiert diskrete Kommandos, Table Storage hält
Joystick-Target (Latest-Value-Semantik), Achsen-Limits und Gerätestatus.

## Endpoints

Alle Endpoints außer `/api/health` erwarten den Header `X-API-Key`.

| Endpoint | Aufrufer | Zweck |
|----------|----------|-------|
| `POST /api/drive` | Frontend | Joystick-Target `{x, y}` setzen (ersetzt das vorherige) |
| `GET /api/drive` | Debug | aktuelles Target lesen |
| `GET/POST /api/drive/config` | Frontend | Achsen-Limits `{rotationLimitDeg, tiltLimitDeg}` |
| `POST /api/commands` | Frontend | diskretes Kommando (`{"action": "stop"}` / `"center"`) in die Queue |
| `GET /api/sync` | ESP32 | Poll: liefert Target, Limits und höchstens ein Kommando |
| `POST /api/state` | ESP32 | Gerätestatus pushen |
| `GET /api/state` | Frontend | Gerätestatus lesen |
| `GET /api/device/status` | Frontend | Erreichbarkeit / Alter des letzten Status |
| `GET /api/health` | alle | Health-Check (ohne Auth) |

## Lokal entwickeln

```bash
cd api
cp local.settings.json.example local.settings.json   # Werte eintragen
pip install -r requirements.txt
func start    # Azure Functions Core Tools, API auf http://localhost:7071/api/
```

Benötigte Settings: `STORAGE_CONNECTION_STRING`, `API_KEY`,
`QUEUE_NAME` (Default `webmotor-commands`), `TABLE_NAME` (Default `webmotorstate`).

## Deployment

Läuft automatisch per GitHub Actions als integrierte API der Azure Static
Web App (siehe [azure-static-web-apps.yml](../.github/workflows/azure-static-web-apps.yml)).
Die Settings werden im Azure Portal an der Static Web App konfiguriert.
