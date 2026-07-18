#include "web_server.h"
#include <WiFi.h>
#include <ArduinoJson.h>
#include "platform_controller.h"
#include "wifi_manager.h"
#include "cloud_client.h"
#include "version.h"

namespace {
constexpr int kHttpPort = 80;

// WLAN setup page for AP mode, embedded in the firmware.
// Shown automatically as a captive portal.
const char kPortalPage[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="de">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>WebMotor WLAN-Setup</title>
<style>
body{font-family:-apple-system,Helvetica,Arial,sans-serif;margin:0;padding:24px;background:#f2f2f7;color:#1c1c1e}
.card{max-width:400px;margin:0 auto;background:#fff;border-radius:12px;padding:20px}
h1{font-size:1.3em;margin:0 0 4px}
p.sub{color:#6e6e73;margin:0 0 16px;font-size:.9em}
label{display:block;font-size:.85em;color:#6e6e73;margin:12px 0 4px}
select,input{width:100%;box-sizing:border-box;font-size:1em;padding:10px;border:1px solid #d1d1d6;border-radius:8px;background:#fff}
#save{width:100%;font-size:1em;padding:12px;border:none;border-radius:8px;background:#007aff;color:#fff;margin-top:16px;font-weight:600}
#save:disabled{opacity:.5}
#rescan{background:none;border:none;color:#007aff;font-size:.85em;margin-top:6px;padding:4px}
#status{margin-top:14px;font-size:.9em;text-align:center;min-height:1.2em}
</style>
</head>
<body>
<div class="card">
<h1>WebMotor</h1>
<p class="sub">Mit welchem WLAN soll sich der Controller verbinden?</p>
<label for="ssid">Netzwerk</label>
<select id="ssid"><option value="">Suche Netzwerke&hellip;</option></select>
<button id="rescan" type="button" onclick="scan()">Erneut suchen</button>
<label for="pass">Passwort</label>
<input id="pass" type="password" autocomplete="off">
<button id="save" type="button" onclick="save()">Verbinden</button>
<div id="status"></div>
</div>
<script>
function el(id){return document.getElementById(id)}
function esc(s){return s.replace(/[&<>"]/g,function(c){return{'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]})}
function scan(){
  var sel=el('ssid');
  sel.innerHTML='<option value="">Suche Netzwerke…</option>';
  fetch('/api/wifi/scan').then(function(r){return r.json()}).then(function(data){
    var seen={},options='';
    (data.networks||[]).sort(function(a,b){return b.rssi-a.rssi}).forEach(function(n){
      if(!n.ssid||seen[n.ssid])return;
      seen[n.ssid]=true;
      options+='<option value="'+esc(n.ssid)+'">'+esc(n.ssid)+(n.secure?' 🔒':'')+'</option>';
    });
    sel.innerHTML=options||'<option value="">Keine Netzwerke gefunden</option>';
  }).catch(function(){
    sel.innerHTML='<option value="">Suche fehlgeschlagen</option>';
  });
}
function save(){
  var ssid=el('ssid').value;
  if(!ssid){el('status').textContent='Bitte ein Netzwerk wählen.';return}
  el('save').disabled=true;
  el('status').textContent='Speichere…';
  fetch('/api/wifi/config',{method:'POST',headers:{'Content-Type':'application/json'},
    body:JSON.stringify({ssid:ssid,password:el('pass').value})})
  .then(function(r){
    if(!r.ok)throw 0;
    el('status').textContent='Gespeichert! Der Controller verbindet sich jetzt mit "'+ssid+'". Falls das Passwort falsch war, erscheint der Hotspot nach ca. 15 Sekunden wieder.';
  }).catch(function(){
    el('save').disabled=false;
    el('status').textContent='Fehler beim Speichern, bitte erneut versuchen.';
  });
}
scan();
</script>
</body>
</html>
)rawliteral";

// Status page for normal (STA) operation: device status plus a pointer
// to the cloud frontend, which hosts the actual control UI.
const char kStatusPage[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="de">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>WebMotor Status</title>
<style>
body{font-family:-apple-system,Helvetica,Arial,sans-serif;margin:0;padding:24px;background:#f2f2f7;color:#1c1c1e}
.card{max-width:420px;margin:0 auto 16px;background:#fff;border-radius:12px;padding:20px}
h1{font-size:1.3em;margin:0}
p.sub{color:#6e6e73;margin:2px 0 0;font-size:.85em}
h2{font-size:.8em;text-transform:uppercase;letter-spacing:.05em;color:#6e6e73;margin:0 0 8px}
.row{display:flex;justify-content:space-between;padding:7px 0;border-bottom:1px solid #f2f2f7;font-size:.95em}
.row:last-child{border-bottom:none}
.row span:first-child{color:#6e6e73}
.ok{color:#34c759}.warn{color:#ff9500}
a.cloud{display:block;text-align:center;background:#007aff;color:#fff;text-decoration:none;padding:12px;border-radius:8px;font-weight:600}
p.hint{color:#6e6e73;font-size:.85em;text-align:center;margin:10px 0 0}
summary{font-size:.8em;text-transform:uppercase;letter-spacing:.05em;color:#6e6e73;cursor:pointer}
label{display:block;font-size:.85em;color:#6e6e73;margin:12px 0 4px}
input[type=url],input[type=password]{width:100%;box-sizing:border-box;font-size:1em;padding:10px;border:1px solid #d1d1d6;border-radius:8px;background:#fff}
.chk{display:flex;align-items:center;gap:8px;color:#1c1c1e;font-size:.95em;margin-top:12px}
#csave{width:100%;font-size:1em;padding:12px;border:none;border-radius:8px;background:#007aff;color:#fff;margin-top:16px;font-weight:600}
#csave:disabled{opacity:.5}
#cstatus{margin-top:10px;font-size:.9em;text-align:center;min-height:1.2em}
</style>
</head>
<body>
<div class="card">
<h1>WebMotor</h1>
<p class="sub" id="version">&nbsp;</p>
</div>
<div class="card">
<h2>Status</h2>
<div class="row"><span>WLAN</span><span id="wifi">&hellip;</span></div>
<div class="row"><span>IP-Adresse</span><span id="ip">&hellip;</span></div>
<div class="row"><span>Rotation</span><span id="rot">&hellip;</span></div>
<div class="row"><span>Neigung</span><span id="tilt">&hellip;</span></div>
<div class="row"><span>Bewegung</span><span id="moving">&hellip;</span></div>
<div class="row"><span>Cloud</span><span id="cloud">&hellip;</span></div>
</div>
<div class="card">
<a class="cloud" id="cloudlink" href="https://calm-river-0a48e7503.6.azurestaticapps.net">Cloud-Frontend &ouml;ffnen</a>
<p class="hint">Die Steuerung l&auml;uft &uuml;ber das Cloud-Frontend &ndash; diese Seite zeigt nur den Ger&auml;testatus.</p>
</div>
<div class="card">
<details>
<summary>Cloud-Konfiguration</summary>
<label for="ep">API-Endpoint</label>
<input id="ep" type="url" placeholder="https://.../api">
<label for="key">API-Key</label>
<input id="key" type="password" autocomplete="off">
<label class="chk"><input id="en" type="checkbox"> Cloud-Sync aktiv</label>
<button id="csave" type="button" onclick="saveCloud()">Speichern</button>
<div id="cstatus"></div>
</details>
</div>
<script>
function el(id){return document.getElementById(id)}
function txt(id,v){el(id).textContent=v}
fetch('/api/info').then(function(r){return r.json()}).then(function(i){
  txt('version','Version '+i.version+' · '+(i.commit||'')+' · '+(i.buildTimestamp||''));
});
function loadCloud(){
  fetch('/api/cloud/status').then(function(r){return r.json()}).then(function(c){
    var active=c.enabled&&c.configured;
    txt('cloud',active?'aktiv':(c.configured?'konfiguriert, inaktiv':'nicht konfiguriert'));
    el('cloud').className=active?'ok':'warn';
    if(c.apiEndpoint){try{el('cloudlink').href=new URL(c.apiEndpoint).origin}catch(e){}}
    el('ep').value=c.apiEndpoint||'';
    el('en').checked=!!c.enabled;
    el('key').placeholder=c.apiKey?'Gespeichert – leer lassen zum Behalten':'';
  });
}
function saveCloud(){
  var btn=el('csave');
  btn.disabled=true;
  txt('cstatus','Speichere…');
  fetch('/api/cloud/config',{method:'POST',headers:{'Content-Type':'application/json'},
    body:JSON.stringify({apiEndpoint:el('ep').value.trim(),apiKey:el('key').value,enabled:el('en').checked})})
  .then(function(r){
    if(!r.ok)throw 0;
    txt('cstatus','Gespeichert.');
    el('key').value='';
    loadCloud();
  }).catch(function(){
    txt('cstatus','Fehler beim Speichern.');
  }).then(function(){btn.disabled=false});
}
loadCloud();
function refresh(){
  fetch('/api/wifi/status').then(function(r){return r.json()}).then(function(w){
    txt('wifi',w.ssid||'–');txt('ip',w.ipAddress||'–');
  });
  fetch('/api/drive/status').then(function(r){return r.json()}).then(function(d){
    txt('rot',d.rotationDeg.toFixed(1)+'° / ±'+d.rotationLimitDeg.toFixed(0)+'°');
    txt('tilt',d.tiltDeg.toFixed(1)+'° / ±'+d.tiltLimitDeg.toFixed(0)+'°');
    txt('moving',d.moving?'in Bewegung':'ruht');
  });
}
refresh();setInterval(refresh,5000);
</script>
</body>
</html>
)rawliteral";
}

WebServerController::WebServerController()
    : server(kHttpPort),
      platform(nullptr),
      wifi(nullptr),
      cloud(nullptr) {}

void WebServerController::begin(PlatformController& platformController, WifiManager& wifiManager, CloudClient& cloudClient) {
    Serial.println("[WEB] Initializing WebServer...");

    platform = &platformController;
    wifi = &wifiManager;
    cloud = &cloudClient;
    Serial.println("[WEB] Controllers linked");

    Serial.println("[WEB] Registering HTTP routes...");
    registerRoutes();

    server.begin();
    Serial.print("[WEB] HTTP server listening on port ");
    Serial.println(kHttpPort);
    Serial.println("[WEB] WebServer initialization complete");
}

void WebServerController::handle() {
    server.handleClient();
}

void WebServerController::registerRoutes() {
    Serial.println("[WEB] Setting up API endpoints...");

    // Version/Info route
    server.on("/api/info", HTTP_GET, [this]() {
        this->handleInfo();
    });

    // Drive routes (joystick control)
    // No per-request logging here: the joystick posts at ~10 Hz
    server.on("/api/drive", HTTP_POST, [this]() {
        this->handleDrive();
    });

    server.on("/api/drive/status", HTTP_GET, [this]() {
        this->handleDriveStatus();
    });

    server.on("/api/drive/config", HTTP_GET, [this]() {
        this->handleDriveConfigGet();
    });

    server.on("/api/drive/config", HTTP_POST, [this]() {
        this->handleDriveConfigPost();
    });

    server.on("/api/drive/center", HTTP_POST, [this]() {
        Serial.println("[API] POST /api/drive/center");
        this->handleDriveCenter();
    });

    server.on("/api/drive/stop", HTTP_POST, [this]() {
        Serial.println("[API] POST /api/drive/stop");
        this->handleDriveStop();
    });

    // WiFi routes
    server.on("/api/wifi/config", HTTP_POST, [this]() {
        Serial.println("[API] POST /api/wifi/config");
        this->handleWiFiConfig();
    });

    server.on("/api/wifi/status", HTTP_GET, [this]() {
        this->handleWiFiStatus();
    });

    server.on("/api/wifi/scan", HTTP_GET, [this]() {
        Serial.println("[API] GET /api/wifi/scan");
        this->handleWiFiScan();
    });

    // Cloud configuration routes
    server.on("/api/cloud/config", HTTP_POST, [this]() {
        Serial.println("[API] POST /api/cloud/config");
        this->handleCloudConfig();
    });

    server.on("/api/cloud/status", HTTP_GET, [this]() {
        Serial.println("[API] GET /api/cloud/status");
        this->handleCloudStatus();
    });

    server.on("/api/cloud/test", HTTP_GET, [this]() {
        Serial.println("[API] GET /api/cloud/test");
        this->handleCloudTest();
    });

    // Root page: setup portal in AP mode, embedded status page otherwise.
    // The control UI lives in the cloud frontend, not on the device.
    server.on("/", [this]() {
        if (wifi != nullptr && wifi->isAccessPoint()) {
            this->servePortalPage();
            return;
        }
        this->serveStatusPage();
    });

    // Catch-all for 404
    server.onNotFound([this]() {
        // Captive-portal probes (captive.apple.com, connectivitycheck, ...)
        // land here; the redirect makes phones open the setup page.
        if (wifi != nullptr && wifi->isAccessPoint()) {
            Serial.print("[WEB] Captive portal redirect: ");
            Serial.println(server.uri());
            server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/", true);
            server.send(302, "text/plain", "");
            return;
        }
        Serial.print("[WEB] 404 - File not found: ");
        Serial.println(server.uri());
        server.send(404, "text/plain", "File not found");
    });

    Serial.println("[WEB] Routes registered successfully");
}

void WebServerController::handleDrive() {
    if (platform == nullptr) {
        sendJson(500, "{\"error\":\"Platform controller unavailable\"}");
        return;
    }

    if (!server.hasArg("plain")) {
        sendJson(400, "{\"error\":\"No data received\"}");
        return;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    if (error || doc["x"].isNull() || doc["y"].isNull()) {
        sendJson(400, "{\"error\":\"Expected JSON with x and y\"}");
        return;
    }

    platform->setTarget(doc["x"].as<float>(), doc["y"].as<float>());
    server.send(200, "application/json", "{\"status\":\"ok\"}");
}

void WebServerController::handleDriveStatus() {
    if (platform == nullptr) {
        sendJson(500, "{\"error\":\"Platform controller unavailable\"}");
        return;
    }

    const PlatformStatus status = platform->getStatus();

    JsonDocument doc;
    doc["x"] = status.x;
    doc["y"] = status.y;
    doc["rotationDeg"] = status.rotationDeg;
    doc["tiltDeg"] = status.tiltDeg;
    doc["rotationLimitDeg"] = status.rotationLimitDeg;
    doc["tiltLimitDeg"] = status.tiltLimitDeg;
    doc["moving"] = status.moving;

    String jsonResponse;
    serializeJson(doc, jsonResponse);
    server.send(200, "application/json", jsonResponse);
}

void WebServerController::handleDriveConfigGet() {
    if (platform == nullptr) {
        sendJson(500, "{\"error\":\"Platform controller unavailable\"}");
        return;
    }

    JsonDocument doc;
    doc["rotationLimitDeg"] = platform->getRotationLimitDeg();
    doc["tiltLimitDeg"] = platform->getTiltLimitDeg();

    String jsonResponse;
    serializeJson(doc, jsonResponse);
    server.send(200, "application/json", jsonResponse);
}

void WebServerController::handleDriveConfigPost() {
    Serial.println("[API] POST /api/drive/config");

    if (platform == nullptr) {
        sendJson(500, "{\"error\":\"Platform controller unavailable\"}");
        return;
    }

    if (!server.hasArg("plain")) {
        sendJson(400, "{\"error\":\"No data received\"}");
        return;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    if (error) {
        sendJson(400, "{\"error\":\"Invalid JSON\"}");
        return;
    }

    const float rotationLimitDeg = doc["rotationLimitDeg"] | platform->getRotationLimitDeg();
    const float tiltLimitDeg = doc["tiltLimitDeg"] | platform->getTiltLimitDeg();

    if (!platform->setLimits(rotationLimitDeg, tiltLimitDeg)) {
        sendJson(400, "{\"error\":\"Invalid limit values\"}");
        return;
    }

    sendJson(200, "{\"status\":\"limits applied\"}");
}

void WebServerController::handleDriveCenter() {
    if (platform == nullptr) {
        sendJson(500, "{\"error\":\"Platform controller unavailable\"}");
        return;
    }

    platform->centerAxes();
    sendJson(200, "{\"status\":\"axes centered\"}");
}

void WebServerController::handleDriveStop() {
    if (platform == nullptr) {
        sendJson(500, "{\"error\":\"Platform controller unavailable\"}");
        return;
    }

    platform->stop();
    sendJson(200, "{\"status\":\"stopped\"}");
}

void WebServerController::handleInfo() {
    JsonDocument doc;
    doc["version"] = VERSION_SEMVER;
    doc["fullVersion"] = VERSION_FULL;
    doc["branch"] = VERSION_BRANCH;
    doc["commit"] = VERSION_SHORT_SHA;
    doc["commitFull"] = VERSION_SHA;
    doc["buildTimestamp"] = VERSION_BUILD_TIMESTAMP;
    doc["platform"] = "ESP32";
    doc["board"] = "ESP32-PICO-KIT V4";

    String jsonResponse;
    serializeJson(doc, jsonResponse);
    server.send(200, "application/json", jsonResponse);
}

void WebServerController::handleWiFiConfig() {
    Serial.println("[API] Processing WiFi config request");

    if (!server.hasArg("plain")) {
        Serial.println("[API] ERROR: No JSON payload for WiFi config");
        sendJson(400, "{\"error\":\"No data received\"}");
        return;
    }

    if (wifi == nullptr) {
        Serial.println("[API] ERROR: Wi-Fi manager unavailable");
        sendJson(500, "{\"error\":\"Wi-Fi manager unavailable\"}");
        return;
    }

    String body = server.arg("plain");

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, body);
    if (error || doc["ssid"].isNull() || doc["password"].isNull()) {
        Serial.print("[API] ERROR: WiFi JSON parse failed - ");
        if (error) {
            Serial.println(error.c_str());
        } else {
            Serial.println("Missing SSID or password");
        }
        sendJson(400, "{\"error\":\"Invalid JSON\"}");
        return;
    }

    const String ssid = doc["ssid"].as<String>();
    const String password = doc["password"].as<String>();

    Serial.print("[WIFI] Saving credentials for SSID: ");
    Serial.println(ssid);
    // Note: Password not logged for security

    wifi->saveCredentials(ssid, password);

    Serial.println("[WIFI] Credentials saved successfully");
    sendJson(200, "{\"status\":\"credentials saved\"}");
}

void WebServerController::handleWiFiStatus() {
    if (wifi == nullptr) {
        sendJson(500, "{\"error\":\"Wi-Fi manager unavailable\"}");
        return;
    }

    JsonDocument doc;
    doc["isAccessPoint"] = wifi->isAccessPoint();
    doc["isConnected"] = wifi->isConnected();

    if (!wifi->isAccessPoint() && wifi->isConnected()) {
        doc["ssid"] = wifi->getSSID();
        doc["ipAddress"] = WiFi.localIP().toString();
    } else if (wifi->isAccessPoint()) {
        doc["ssid"] = "WebMotor-Config";
        doc["ipAddress"] = WiFi.softAPIP().toString();
    } else {
        doc["ssid"] = "";
        doc["ipAddress"] = "";
    }

    String jsonResponse;
    serializeJson(doc, jsonResponse);
    server.send(200, "application/json", jsonResponse);
}

void WebServerController::handleWiFiScan() {
    // Blocks for ~2 s; only used from the setup page, where that is fine
    const int16_t count = WiFi.scanNetworks();

    JsonDocument doc;
    JsonArray networks = doc["networks"].to<JsonArray>();
    for (int16_t i = 0; i < count; ++i) {
        JsonObject network = networks.add<JsonObject>();
        network["ssid"] = WiFi.SSID(i);
        network["rssi"] = WiFi.RSSI(i);
        network["secure"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
    }
    WiFi.scanDelete();

    String jsonResponse;
    serializeJson(doc, jsonResponse);
    server.send(200, "application/json", jsonResponse);
}

void WebServerController::servePortalPage() {
    Serial.println("[WEB] Serving captive portal page");
    server.send(200, "text/html", kPortalPage);
}

void WebServerController::serveStatusPage() {
    Serial.println("[WEB] Serving status page");
    server.send(200, "text/html", kStatusPage);
}

void WebServerController::sendJson(int statusCode, const String& payload) {
    Serial.print("[HTTP] Response ");
    Serial.print(statusCode);
    Serial.print(": ");
    Serial.println(payload);

    server.send(statusCode, "application/json", payload);
}

void WebServerController::handleCloudConfig() {
    Serial.println("[API] Processing cloud config request");

    if (!server.hasArg("plain")) {
        Serial.println("[API] ERROR: No JSON payload for cloud config");
        sendJson(400, "{\"error\":\"No data received\"}");
        return;
    }

    if (cloud == nullptr) {
        Serial.println("[API] ERROR: Cloud client unavailable");
        sendJson(500, "{\"error\":\"Cloud client unavailable\"}");
        return;
    }

    String body = server.arg("plain");

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, body);
    if (error) {
        Serial.print("[API] ERROR: Cloud JSON parse failed - ");
        Serial.println(error.c_str());
        sendJson(400, "{\"error\":\"Invalid JSON\"}");
        return;
    }

    String apiEndpoint = doc["apiEndpoint"] | "";
    String apiKey = doc["apiKey"] | "";
    bool enabled = doc["enabled"] | false;

    Serial.print("[CLOUD] Saving configuration - Endpoint: ");
    Serial.println(apiEndpoint);
    Serial.print("[CLOUD] API Key provided: ");
    Serial.println(apiKey.isEmpty() ? "false (keeping existing)" : "true");
    Serial.print("[CLOUD] Enabled: ");
    Serial.println(enabled ? "true" : "false");

    // If API key is empty, keep the existing one
    if (apiKey.isEmpty()) {
        String currentEndpoint, currentKey;
        bool currentEnabled;
        cloud->getConfig(currentEndpoint, currentKey, currentEnabled);
        apiKey = currentKey;
        Serial.println("[CLOUD] Using existing API key");
    }

    if (cloud->setConfig(apiEndpoint, apiKey, enabled)) {
        Serial.println("[CLOUD] Configuration saved successfully");
        sendJson(200, "{\"status\":\"configuration saved\"}");
    } else {
        Serial.println("[CLOUD] ERROR: Failed to save configuration");
        sendJson(500, "{\"error\":\"Failed to save configuration\"}");
    }
}

void WebServerController::handleCloudStatus() {
    Serial.println("[API] Processing cloud status request");

    if (cloud == nullptr) {
        Serial.println("[API] ERROR: Cloud client unavailable");
        sendJson(500, "{\"error\":\"Cloud client unavailable\"}");
        return;
    }

    String apiEndpoint, apiKey;
    bool enabled;
    cloud->getConfig(apiEndpoint, apiKey, enabled);

    JsonDocument doc;
    doc["apiEndpoint"] = apiEndpoint;
    doc["apiKey"] = apiKey.isEmpty() ? "" : "********"; // Mask API key
    doc["enabled"] = enabled;
    doc["configured"] = !apiEndpoint.isEmpty() && !apiKey.isEmpty();

    String jsonResponse;
    serializeJson(doc, jsonResponse);

    sendJson(200, jsonResponse);
}

void WebServerController::handleCloudTest() {
    Serial.println("[API] Processing cloud test request");

    if (cloud == nullptr) {
        Serial.println("[API] ERROR: Cloud client unavailable");
        sendJson(500, "{\"error\":\"Cloud client unavailable\"}");
        return;
    }

    bool success = cloud->testConnection();

    JsonDocument doc;
    doc["success"] = success;
    doc["message"] = success ? "Connection successful" : "Connection failed";

    String jsonResponse;
    serializeJson(doc, jsonResponse);

    sendJson(200, jsonResponse);
}
