# WebMotor Cloud Frontend

This is the cloud-based web interface for remote motor control. It connects to the Azure Function backend to control the motor from anywhere with internet access.

## Features

- **Remote Control**: Control your motor from anywhere
- **Real-time State Monitoring**: View motor position, speed, and status
- **Secure**: API key-based authentication
- **Similar Interface**: Familiar UI based on the local web interface

## Setup

### 1. Configure Azure Backend

First, make sure you have deployed the Azure Function backend from the `azure-function/` directory.

### 2. Deploy to Azure Static Web App

Since you already have an Azure Static Web App created, you can deploy this frontend to it.

**Option A: Deploy via Azure CLI**

```bash
# Login to Azure
az login

# Deploy to your Static Web App
az staticwebapp deploy \
  --name YOUR_STATIC_WEB_APP_NAME \
  --resource-group YOUR_RESOURCE_GROUP \
  --source ./cloud-frontend \
  --app-location .
```

**Option B: Deploy via GitHub Actions**

Configure GitHub Actions to automatically deploy on push:

1. In Azure Portal, go to your Static Web App
2. Get the deployment token
3. Add it as a GitHub secret named `AZURE_STATIC_WEB_APPS_API_TOKEN`
4. Create `.github/workflows/azure-static-web-apps.yml` (see below)

**Option C: Deploy via VS Code Extension**

1. Install "Azure Static Web Apps" extension
2. Right-click on `cloud-frontend` folder
3. Select "Deploy to Static Web App"

### 3. Configure CORS

In your Azure Function App:

1. Go to Azure Portal → Your Function App → CORS
2. Add your Static Web App URL (e.g., `https://your-app.azurestaticapps.net`)
3. Save

### Local Testing

You can test the frontend locally:

```bash
cd cloud-frontend
python3 -m http.server 8000
```

Then open http://localhost:8000 in your browser.

## Usage

### First Time Setup

1. Open the cloud frontend in your browser
2. Enter your Azure Function endpoint URL:
   - Example: `https://your-function-app.azurewebsites.net/api`
3. Enter your API key (the same one configured in your Azure Function)
4. Click "Save Configuration"
5. Click "Test Connection" to verify

The configuration is saved in your browser's localStorage, so you only need to do this once per browser.

### Controlling the Motor

Once configured:

1. The interface will automatically poll for motor state every 2 seconds
2. Use the controls just like the local web interface:
   - Adjust microstepping
   - Set frequency (speed)
   - Toggle direction
   - Start/Stop/Release motor
3. The status bar shows:
   - Cloud connection status
   - Motor mode
   - Current position
   - Last update timestamp

## Architecture

```
Cloud Frontend (Browser)
    ↓ (commands)
Azure Function API
    ↓ (queue)
Azure Queue Storage
    ↑ (long poll)
Controller (ATOM S3)
    ↓ (state updates)
Azure Function API
    ↓ (table storage)
Azure Table Storage
    ↑ (polling)
Cloud Frontend (Browser)
```

### Command Flow
1. User interacts with cloud frontend
2. Frontend sends command to Azure Function `/api/commands` endpoint
3. Azure Function adds command to Queue Storage
4. Controller long-polls `/api/commands/poll` endpoint (30s timeout)
5. Controller receives command and executes it

### State Flow
1. Controller pushes state to Azure Function `/api/state` endpoint every 1-2s
2. Azure Function stores state in Table Storage
3. Frontend polls `/api/state` endpoint every 2s
4. Frontend displays current motor state

## Security Notes

- **API Key**: Keep your API key secure. Never share it publicly.
- **HTTPS**: Always use HTTPS in production (Azure Static Web Apps provides this automatically).
- **Browser Storage**: The API key is stored in localStorage, which is isolated per origin but accessible to JavaScript.
- **For Educational Use**: This security model is suitable for educational projects. For production, consider Azure AD authentication.

## Troubleshooting

### Connection Failed
- Verify Azure Function is deployed and running
- Check that the API endpoint URL is correct (should end with `/api`)
- Ensure CORS is configured correctly
- Verify API key matches the one in Azure Function configuration

### No State Updates
- Make sure the controller is configured with cloud settings
- Check that the controller has internet connectivity
- Verify the controller is pushing state (check Azure Function logs)
- Ensure API key is correct

### Commands Not Working
- Test connection first using "Test Connection" button
- Check browser console for error messages
- Verify Azure Storage Queue is created and accessible
- Make sure controller is polling for commands
