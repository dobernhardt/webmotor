# WebMotor Cloud-Frontend

Statisches WebUI (HTML/JS/CSS, kein Build-Schritt) zur Steuerung der
Plattform über die Cloud-API ([api/](../api/)).

- Joystick und Pfeiltasten setzen das Drive-Target `{x, y}`
- Home-Button zentriert, Stop friert die Achsen ein
- Konfiguration der Achsen-Limits (Plattformwinkel); das Frontend ist
  die Quelle der Wahrheit, der ESP32 übernimmt die Limits per Sync
- Statusanzeige (Position, Bewegung, Geräte-Erreichbarkeit)

Der API-Key wird im Browser (`localStorage`) gespeichert und bei jedem
Request als `X-API-Key`-Header mitgeschickt.

Deployment automatisch per GitHub Actions als Azure Static Web App
(siehe [azure-static-web-apps.yml](../.github/workflows/azure-static-web-apps.yml)).

URL: https://calm-river-0a48e7503.6.azurestaticapps.net
