"""
WebMotor Cloud Backend - Azure Functions
Provides API endpoints for remote motor control via cloud
"""

import azure.functions as func
import json
import logging
import os
import time
from datetime import datetime, timezone
from azure.storage.queue import QueueClient
from azure.data.tables import TableServiceClient, UpdateMode

app = func.FunctionApp(http_auth_level=func.AuthLevel.ANONYMOUS)

# Configuration
STORAGE_CONNECTION_STRING = os.environ.get("STORAGE_CONNECTION_STRING")
API_KEY = os.environ.get("API_KEY")
QUEUE_NAME = os.environ.get("QUEUE_NAME", "webmotor-commands")
TABLE_NAME = os.environ.get("TABLE_NAME", "webmotorstate")
LONG_POLL_TIMEOUT = 30  # seconds

# Initialize storage clients
queue_client = None
table_client = None

def init_storage_clients():
    """Initialize Azure Storage clients"""
    global queue_client, table_client
    
    if not STORAGE_CONNECTION_STRING:
        logging.error("STORAGE_CONNECTION_STRING not set")
        return False
    
    try:
        # Initialize Queue client (using default text encoding for JSON)
        queue_client = QueueClient.from_connection_string(
            STORAGE_CONNECTION_STRING,
            QUEUE_NAME
        )
        queue_client.create_queue()
        
        # Initialize Table client
        table_service = TableServiceClient.from_connection_string(STORAGE_CONNECTION_STRING)
        table_client = table_service.get_table_client(TABLE_NAME)
        table_client.create_table()
        
        logging.info("Storage clients initialized successfully")
        return True
    except Exception as e:
        logging.error(f"Failed to initialize storage clients: {str(e)}")
        return False


def verify_api_key(req: func.HttpRequest) -> bool:
    """Verify API key from request header"""
    if not API_KEY:
        logging.warning("API_KEY not configured - authentication disabled")
        return True
    
    provided_key = req.headers.get("X-API-Key", "")
    return provided_key == API_KEY


def create_error_response(message: str, status_code: int = 400) -> func.HttpResponse:
    """Create error response"""
    return func.HttpResponse(
        json.dumps({"error": message}),
        status_code=status_code,
        mimetype="application/json"
    )


def create_success_response(data: dict = None, status_code: int = 200) -> func.HttpResponse:
    """Create success response"""
    body = data if data else {"status": "ok"}
    return func.HttpResponse(
        json.dumps(body),
        status_code=status_code,
        mimetype="application/json"
    )


@app.route(route="commands", methods=["POST"])
def add_command(req: func.HttpRequest) -> func.HttpResponse:
    """
    POST /api/commands
    Add a new command to the queue (called by frontend)
    """
    logging.info("add_command: Request received")
    
    # Initialize storage if needed
    if not queue_client:
        if not init_storage_clients():
            return create_error_response("Storage not initialized", 500)
    
    # Verify API key
    if not verify_api_key(req):
        return create_error_response("Unauthorized", 401)
    
    try:
        # Parse request body
        req_body = req.get_json()
        
        # Validate command structure
        if not req_body or "action" not in req_body:
            return create_error_response("Missing 'action' field")
        
        # Add timestamp
        req_body["timestamp"] = datetime.now(timezone.utc).isoformat()
        
        # Add to queue
        message = json.dumps(req_body)
        queue_client.send_message(message)
        
        logging.info(f"Command added to queue: {req_body.get('action')}")
        return create_success_response({"message": "Command queued"})
        
    except ValueError:
        return create_error_response("Invalid JSON")
    except Exception as e:
        logging.error(f"Error adding command: {str(e)}")
        return create_error_response(f"Internal error: {str(e)}", 500)


@app.route(route="commands/poll", methods=["GET"])
def poll_commands(req: func.HttpRequest) -> func.HttpResponse:
    """
    GET /api/commands/poll
    Long poll for commands (called by controller)
    Waits up to 30 seconds for a new command
    """
    logging.info("poll_commands: Request received")
    
    # Initialize storage if needed
    if not queue_client:
        if not init_storage_clients():
            return create_error_response("Storage not initialized", 500)
    
    # Verify API key
    if not verify_api_key(req):
        return create_error_response("Unauthorized", 401)
    
    try:
        start_time = time.time()
        
        # Long polling loop
        while time.time() - start_time < LONG_POLL_TIMEOUT:
            # Try to get a message
            messages = queue_client.receive_messages(max_messages=1, visibility_timeout=60)
            
            for message in messages:
                try:
                    # Parse command
                    command = json.loads(message.content)
                    
                    # Delete message from queue
                    queue_client.delete_message(message.id, message.pop_receipt)
                    
                    logging.info(f"Command retrieved: {command.get('action')}")
                    return create_success_response({"command": command})
                    
                except json.JSONDecodeError:
                    logging.error(f"Invalid JSON in queue message: {message.content}")
                    queue_client.delete_message(message.id, message.pop_receipt)
                    continue
            
            # No message yet, wait a bit before trying again
            time.sleep(1)
        
        # Timeout reached, no command available
        logging.info("poll_commands: Timeout reached, no commands")
        return create_success_response({"command": None})
        
    except Exception as e:
        logging.error(f"Error polling commands: {str(e)}")
        return create_error_response(f"Internal error: {str(e)}", 500)


@app.route(route="state", methods=["POST"])
def update_state(req: func.HttpRequest) -> func.HttpResponse:
    """
    POST /api/state
    Update motor state (called by controller)
    """
    logging.info("update_state: Request received")
    
    # Initialize storage if needed
    if not table_client:
        if not init_storage_clients():
            return create_error_response("Storage not initialized", 500)
    
    # Verify API key
    if not verify_api_key(req):
        return create_error_response("Unauthorized", 401)
    
    try:
        # Parse request body
        req_body = req.get_json()
        
        if not req_body:
            return create_error_response("Empty request body")
        
        # Create entity for table storage
        entity = {
            "PartitionKey": "motor",
            "RowKey": "current",
            "timestamp": datetime.now(timezone.utc).isoformat(),
            **req_body
        }
        
        # Upsert to table (insert or update)
        table_client.upsert_entity(entity, mode=UpdateMode.REPLACE)
        
        logging.info("State updated successfully")
        return create_success_response({"message": "State updated"})
        
    except ValueError:
        return create_error_response("Invalid JSON")
    except Exception as e:
        logging.error(f"Error updating state: {str(e)}")
        return create_error_response(f"Internal error: {str(e)}", 500)


@app.route(route="state", methods=["GET"])
def get_state(req: func.HttpRequest) -> func.HttpResponse:
    """
    GET /api/state
    Get current motor state (called by frontend)
    """
    logging.info("get_state: Request received")
    
    # Initialize storage if needed
    if not table_client:
        if not init_storage_clients():
            return create_error_response("Storage not initialized", 500)
    
    # Verify API key
    if not verify_api_key(req):
        return create_error_response("Unauthorized", 401)
    
    try:
        # Get entity from table storage
        entity = table_client.get_entity(partition_key="motor", row_key="current")
        
        # Remove Azure Table Storage metadata
        state = {k: v for k, v in entity.items() 
                if not k.startswith("_") and k not in ["PartitionKey", "RowKey"]}
        
        logging.info("State retrieved successfully")
        return create_success_response({"state": state})
        
    except Exception as e:
        if "ResourceNotFound" in str(e):
            logging.info("No state available yet")
            return create_success_response({"state": None})
        
        logging.error(f"Error getting state: {str(e)}")
        return create_error_response(f"Internal error: {str(e)}", 500)


@app.route(route="health", methods=["GET"])
def health_check(req: func.HttpRequest) -> func.HttpResponse:
    """
    GET /api/health
    Health check endpoint
    """
    return create_success_response({
        "status": "healthy",
        "timestamp": datetime.now(timezone.utc).isoformat()
    })
