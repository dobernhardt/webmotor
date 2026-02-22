# WebMotor Cloud Integration Setup Guide

This guide walks you through setting up remote motor control via Azure cloud services.

## Architecture Overview

The cloud integration enables you to control your motor from anywhere with internet access using three main components:

```
┌─────────────────────────────────────────────────────────────────┐
│                         Cloud Control Flow                       │
├─────────────────────────────────────────────────────────────────┤
│                                                                   │
│  Cloud Frontend (Browser)                                        │
│       ↓ sends commands                                           │
│  Azure Function API                                              │
│       ↓ adds to queue                                            │
│  Azure Queue Storage                                             │
│       ↑ long polls (30s timeout)                                 │
│  Controller (ATOM S3)                                            │
│       ↓ pushes state (every 2s)                                  │
│  Azure Function API                                              │
│       ↓ stores in table                                          │
│  Azure Table Storage                                             │
│       ↑ polls (every 2s)                                         │
│  Cloud Frontend (Browser)                                        │
│                                                                   │
└─────────────────────────────────────────────────────────────────┘
```

### Key Features

- **Remote Access**: Control motor from anywhere with internet
- **Long Polling**: Efficient command delivery with 30-second long poll
- **State Sync**: Real-time motor state updates every 2 seconds
- **API Key Auth**: Secure communication with API key authentication
- **Dual Control**: Local and cloud control work simultaneously
- **Persistent Config**: Cloud settings stored in controller's NVS

## Prerequisites

Before starting, ensure you have:

1. **Azure Account** - [Sign up](https://azure.microsoft.com/free/) for free
2. **Azure Resources** (you mentioned you already have these):
   - Azure Storage Account
   - Azure Static Web App
3. **Tools**:
   - Azure CLI - `brew install azure-cli`
   - Azure Functions Core Tools - `brew install azure-functions-core-tools@4`
   - Python 3.8+ (for Azure Functions)

## Step 1: Set Up Azure Storage

### 1.1 Get Storage Connection String

```bash
# Login to Azure
az login

# List your storage accounts
az storage account list --output table

# Get connection string (replace with your names)
az storage account show-connection-string \
  --name YOUR_STORAGE_ACCOUNT_NAME \
  --resource-group YOUR_RESOURCE_GROUP \
  --query connectionString \
  --output tsv
```

Save this connection string - you'll need it for the Azure Function.

### 1.2 Verify Storage Account

The Azure Function will automatically create the required Queue and Table, but you can verify your storage account is accessible:

```bash
# List containers (should work if connection string is correct)
az storage container list \
  --connection-string "YOUR_CONNECTION_STRING"
```

## Step 2: Generate API Key

Generate a secure API key for authentication:

```bash
# On macOS/Linux
openssl rand -hex 32

# Or use Python
python3 -c "import secrets; print(secrets.token_hex(32))"
```

Save this API key securely - you'll need it for:
1. Azure Function configuration
2. Controller configuration (via web UI)
3. Cloud frontend configuration

## Step 3: Deploy Azure Function Backend

### 3.1 Set Up Local Development (Optional)

If you want to test locally first:

```bash
cd azure-function

# Copy example settings
cp local.settings.json.example local.settings.json

# Edit local.settings.json and add:
# - STORAGE_CONNECTION_STRING (from Step 1.1)
# - API_KEY (from Step 2)

# Install dependencies
pip3 install -r requirements.txt

# Run locally
func start
```

Test at http://localhost:7071/api/health

### 3.2 Create Azure Function App

```bash
# Create a Function App (replace with your names)
az functionapp create \
  --name YOUR_FUNCTION_APP_NAME \
  --resource-group YOUR_RESOURCE_GROUP \
  --storage-account YOUR_STORAGE_ACCOUNT_NAME \
  --consumption-plan-location YOUR_REGION \
  --runtime python \
  --runtime-version 3.11 \
  --functions-version 4 \
  --os-type Linux
```

### 3.3 Configure Function App Settings

```bash
# Set storage connection string
az functionapp config appsettings set \
  --name YOUR_FUNCTION_APP_NAME \
  --resource-group YOUR_RESOURCE_GROUP \
  --settings "STORAGE_CONNECTION_STRING=YOUR_CONNECTION_STRING"

# Set API key
az functionapp config appsettings set \
  --name YOUR_FUNCTION_APP_NAME \
  --resource-group YOUR_RESOURCE_GROUP \
  --settings "API_KEY=YOUR_API_KEY"

# Set queue name (optional, uses default)
az functionapp config appsettings set \
  --name YOUR_FUNCTION_APP_NAME \
  --resource-group YOUR_RESOURCE_GROUP \
  --settings "QUEUE_NAME=webmotor-commands"

# Set table name (optional, uses default)
az functionapp config appsettings set \
  --name YOUR_FUNCTION_APP_NAME \
  --resource-group YOUR_RESOURCE_GROUP \
  --settings "TABLE_NAME=webmotorstate"
```

### 3.4 Deploy Function Code

```bash
cd azure-function

# Deploy to Azure
func azure functionapp publish YOUR_FUNCTION_APP_NAME
```

### 3.5 Test Deployment

```bash
# Test health endpoint (no auth required)
curl https://YOUR_FUNCTION_APP_NAME.azurewebsites.net/api/health

# Should return: {"status":"healthy","timestamp":"..."}
```

## Step 4: Deploy Cloud Frontend

### 4.1 Configure Static Web App

Since you already have a Static Web App, deploy the cloud frontend:

```bash
cd cloud-frontend

# Deploy via Azure CLI
az staticwebapp deploy \
  --name YOUR_STATIC_WEB_APP_NAME \
  --resource-group YOUR_RESOURCE_GROUP \
  --source . \
  --app-location .
```

**Alternative: Deploy via VS Code**

1. Install "Azure Static Web Apps" extension
2. Right-click on `cloud-frontend` folder
3. Select "Deploy to Static Web App"
4. Follow prompts

### 4.2 Configure CORS

Allow your Static Web App to access the Function App:

```bash
# Get your Static Web App URL
az staticwebapp show \
  --name YOUR_STATIC_WEB_APP_NAME \
  --resource-group YOUR_RESOURCE_GROUP \
  --query "defaultHostname" \
  --output tsv

# Add CORS (replace with your Static Web App URL)
az functionapp cors add \
  --name YOUR_FUNCTION_APP_NAME \
  --resource-group YOUR_RESOURCE_GROUP \
  --allowed-origins "https://YOUR_STATIC_WEB_APP_URL"
```

### 4.3 Test Cloud Frontend

1. Open your Static Web App URL in a browser
2. Enter Azure Function endpoint: `https://YOUR_FUNCTION_APP_NAME.azurewebsites.net/api`
3. Enter your API key
4. Click "Save Configuration"
5. Click "Test Connection" - should see ✓ Connection successful

## Step 5: Configure Controller

### 5.1 Upload Firmware

Make sure your controller has the latest firmware with cloud support:

```bash
cd /path/to/WebMotor

# Build and upload
pio run -t upload

# Monitor serial output
pio device monitor
```

### 5.2 Connect to Controller

1. Connect to the controller's web interface (local network)
2. If in AP mode, connect to WiFi network "WebMotor-Config"
3. Open http://192.168.4.1 in browser

### 5.3 Configure WiFi (if needed)

In the local web UI:
1. Scroll to "WiFi Configuration"
2. Enter your WiFi SSID and password
3. Click "Save WiFi Config"
4. Wait for controller to connect (check serial monitor)

### 5.4 Configure Cloud Settings

In the local web UI:
1. Scroll to "☁️ Cloud Configuration"
2. Enter Azure Function Endpoint: `https://YOUR_FUNCTION_APP_NAME.azurewebsites.net/api`
3. Enter your API Key
4. Toggle "Enable Cloud Sync" to Enabled
5. Click "Save Cloud Config"
6. Click "Test Connection" to verify

You should see:
```
✓ Connection successful
```

The configuration is stored in the controller's NVS (non-volatile storage) and survives reboots.

## Step 6: Test End-to-End

### 6.1 Test from Cloud Frontend

1. Open your cloud frontend in a browser (anywhere with internet)
2. Configure API endpoint and key if not already done
3. Wait a few seconds for state to sync
4. Try controlling the motor:
   - Change frequency slider
   - Toggle direction
   - Click Start/Stop/Release buttons
5. Verify commands are executed on the physical motor
6. Verify state updates appear in the cloud frontend

### 6.2 Test Local and Cloud Together

1. Open local web UI (http://CONTROLLER_IP)
2. Open cloud frontend in another tab/device
3. Control motor from local UI - changes should appear in cloud
4. Control motor from cloud - changes should appear in local UI

Both interfaces work simultaneously!

## Troubleshooting

### Controller Not Connecting to Cloud

**Check Serial Monitor:**
```bash
pio device monitor
```

Look for:
```
[CLOUD] Cloud sync enabled
[CLOUD] Endpoint: https://...
```

**Common Issues:**
- WiFi not connected - configure WiFi first
- Wrong endpoint URL - must end with `/api`
- Wrong API key - must match Function App configuration
- Firewall blocking outbound HTTPS

### Commands Not Working

**Check Azure Function Logs:**
```bash
# Stream logs
func azure functionapp logstream YOUR_FUNCTION_APP_NAME
```

Look for errors when commands are sent.

**Verify Queue:**
- Login to Azure Portal
- Navigate to Storage Account → Queues
- Check if "webmotor-commands" queue exists
- Verify messages are being added

### State Not Updating

**Check Table Storage:**
- Login to Azure Portal
- Navigate to Storage Account → Tables
- Check if "webmotorstate" table exists
- Verify entity with PartitionKey="motor", RowKey="current" exists

**Controller Side:**
- Check serial monitor for "[CLOUD] State pushed successfully" (debug messages)
- Verify controller has internet connectivity
- Verify API key is correct

### CORS Errors

If you see CORS errors in browser console:

```bash
# Add your origin to CORS
az functionapp cors add \
  --name YOUR_FUNCTION_APP_NAME \
  --resource-group YOUR_RESOURCE_GROUP \
  --allowed-origins "https://YOUR_ORIGIN"
```

## Cost Estimation

All services use consumption-based pricing. For an educational project with light usage:

- **Azure Functions**: ~$0 (1M free executions/month)
- **Azure Storage**: ~$0.01-0.10/month (negligible for this use case)
- **Azure Static Web Apps**: Free tier available
- **Total**: Effectively free for educational use

## Security Best Practices

### For Production Use

If you plan to use this in a production environment:

1. **Use Azure Key Vault** for storing secrets instead of app settings
2. **Enable HTTPS only** (Azure does this by default)
3. **Rotate API keys** regularly
4. **Use Azure AD authentication** instead of API keys
5. **Enable Azure Monitor** for logging and alerts
6. **Set up rate limiting** on the Function App
7. **Use Virtual Networks** to isolate resources

### For Educational Use

The current API key-based authentication is sufficient:
- Keep API key secret
- Don't commit API key to version control
- Use HTTPS endpoints only (enforced by Azure)

## Architecture Details

### Long Polling Pattern

The controller uses long polling for efficient command delivery:

```
Controller → GET /api/commands/poll
  ↓
Backend waits up to 30 seconds
  ↓
If command available: return immediately
If timeout: return null
  ↓
Controller processes command or waits 100ms and polls again
```

Benefits:
- Near real-time command delivery
- Efficient (reduces unnecessary requests)
- Works with HTTP (no WebSocket needed)

### State Synchronization

State is pushed every 2 seconds:

```
Every 2 seconds:
  Controller → POST /api/state {motor state}
  Backend → Store in Table Storage

Every 2 seconds:
  Cloud Frontend → GET /api/state
  Backend → Return from Table Storage
```

This provides near real-time state updates while minimizing Azure costs.

## Next Steps

- **Add more commands**: Extend the command protocol for new features
- **Add homing**: Implement motor homing and position tracking
- **Multiple motors**: Support controlling multiple motors
- **Command history**: Store command history in Azure
- **Telemetry**: Add Application Insights for monitoring
- **Mobile app**: Create a mobile app using the same API

## Support

For issues or questions:
- Check serial monitor output on controller
- Check Azure Function logs
- Verify all configuration steps
- Review the code in `src/cloud_client.cpp` and `azure-function/function_app.py`

## Summary

You now have a complete cloud-based motor control system:
- ✅ Azure Function backend with API
- ✅ Cloud frontend for remote control
- ✅ Controller integration with cloud client
- ✅ Real-time command execution
- ✅ State synchronization
- ✅ Secure API key authentication
- ✅ Local and cloud control working together

Happy remote motor controlling! 🎉
