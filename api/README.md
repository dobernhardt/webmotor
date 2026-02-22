# WebMotor Cloud Backend - Azure Functions

This directory contains the Azure Functions backend for remote motor control.

## Architecture

- **Queue Storage**: Stores commands from the cloud frontend
- **Table Storage**: Stores current motor state
- **Azure Functions**: Provides REST API endpoints

## API Endpoints

### POST /api/commands
Add a new command to the queue (called by frontend)

**Headers:**
- `X-API-Key`: API key for authentication
- `Content-Type`: application/json

**Request Body:**
```json
{
  "action": "move|stop|home|config",
  "parameters": {
    // action-specific parameters
  }
}
```

**Response:**
```json
{
  "message": "Command queued"
}
```

### GET /api/commands/poll
Long poll for commands (called by controller)
Waits up to 30 seconds for a new command.

**Headers:**
- `X-API-Key`: API key for authentication

**Response:**
```json
{
  "command": {
    "action": "move",
    "parameters": {...},
    "timestamp": "2026-02-22T10:30:00Z"
  }
}
```

Or if no command available after timeout:
```json
{
  "command": null
}
```

### POST /api/state
Update motor state (called by controller)

**Headers:**
- `X-API-Key`: API key for authentication
- `Content-Type`: application/json

**Request Body:**
```json
{
  "position": 1000,
  "targetPosition": 2000,
  "speed": 100,
  "enabled": true,
  "moving": true,
  "homed": true
}
```

**Response:**
```json
{
  "message": "State updated"
}
```

### GET /api/state
Get current motor state (called by frontend)

**Headers:**
- `X-API-Key`: API key for authentication

**Response:**
```json
{
  "state": {
    "position": 1000,
    "targetPosition": 2000,
    "speed": 100,
    "enabled": true,
    "moving": true,
    "homed": true,
    "timestamp": "2026-02-22T10:30:00Z"
  }
}
```

### GET /api/health
Health check endpoint (no authentication required)

**Response:**
```json
{
  "status": "healthy",
  "timestamp": "2026-02-22T10:30:00Z"
}
```

## Setup

### Prerequisites
- Azure account
- Azure Storage Account (you already have this)
- Azure Static Web App (you already have this)

### Configuration

1. Copy `local.settings.json.example` to `local.settings.json`:
   ```bash
   cp local.settings.json.example local.settings.json
   ```

2. Update `local.settings.json` with your values:
   - `STORAGE_CONNECTION_STRING`: Your Azure Storage connection string
   - `API_KEY`: Generate a secure API key (e.g., using `openssl rand -hex 32`)
   - `QUEUE_NAME`: Queue name (default: "webmotor-commands")
   - `TABLE_NAME`: Table name (default: "webmotorstate")

### Local Development

1. Install Azure Functions Core Tools:
   ```bash
   brew install azure-functions-core-tools@4
   ```

2. Install Python dependencies:
   ```bash
   cd azure-function
   pip install -r requirements.txt
   ```

3. Run locally:
   ```bash
   func start
   ```

   The API will be available at `http://localhost:7071/api/`

### Deployment to Azure

1. Make sure you have Azure CLI installed:
   ```bash
   brew install azure-cli
   az login
   ```

2. Deploy to your Azure Function App:
   ```bash
   cd azure-function
   func azure functionapp publish YOUR_FUNCTION_APP_NAME
   ```

3. Configure application settings in Azure Portal:
   - Go to your Function App → Configuration
   - Add the following application settings:
     - `STORAGE_CONNECTION_STRING`
     - `API_KEY`
     - `QUEUE_NAME`
     - `TABLE_NAME`

## Testing

Test the API using curl:

```bash
# Health check
curl http://localhost:7071/api/health

# Add command (with API key)
curl -X POST http://localhost:7071/api/commands \
  -H "X-API-Key: your-api-key" \
  -H "Content-Type: application/json" \
  -d '{"action":"move","parameters":{"position":1000,"speed":100}}'

# Poll for commands (will wait up to 30s)
curl http://localhost:7071/api/commands/poll \
  -H "X-API-Key: your-api-key"

# Update state
curl -X POST http://localhost:7071/api/state \
  -H "X-API-Key: your-api-key" \
  -H "Content-Type: application/json" \
  -d '{"position":500,"enabled":true,"moving":false}'

# Get state
curl http://localhost:7071/api/state \
  -H "X-API-Key: your-api-key"
```

## Security Notes

- Always use HTTPS in production
- Keep your API keys secure and never commit them to version control
- Consider using Azure Key Vault for production secrets
- For this educational project, API key authentication is sufficient
