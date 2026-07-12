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
from pathlib import Path
from azure.storage.queue import QueueClient
from azure.data.tables import TableServiceClient, UpdateMode

app = func.FunctionApp(http_auth_level=func.AuthLevel.ANONYMOUS)

# Configuration
STORAGE_CONNECTION_STRING = os.environ.get("STORAGE_CONNECTION_STRING")
API_KEY = os.environ.get("API_KEY")
QUEUE_NAME = os.environ.get("QUEUE_NAME", "webmotor-commands")
TABLE_NAME = os.environ.get("TABLE_NAME", "webmotorstate")
LONG_POLL_TIMEOUT = 30  # seconds - ESP32 now uses background task, can handle long polls

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
        # Initialize Queue client
        queue_client = QueueClient.from_connection_string(
            STORAGE_CONNECTION_STRING,
            QUEUE_NAME
        )
        try:
            queue_client.create_queue()
            logging.info(f"Created queue: {QUEUE_NAME}")
        except Exception as e:
            if "already exists" in str(e).lower():
                logging.info(f"Queue {QUEUE_NAME} already exists")
            else:
                raise
        
        # Initialize Table client
        table_service = TableServiceClient.from_connection_string(STORAGE_CONNECTION_STRING)
        table_client = table_service.get_table_client(TABLE_NAME)
        try:
            table_client.create_table()
            logging.info(f"Created table: {TABLE_NAME}")
        except Exception as e:
            if "already exists" in str(e).lower():
                logging.info(f"Table {TABLE_NAME} already exists")
            else:
                raise
        
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
    return func.HttpResponse(
        json.dumps({"error": message}),
        status_code=status_code,
        mimetype="application/json"
    )


def create_success_response(data: dict = None, status_code: int = 200) -> func.HttpResponse:
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
    
    if not queue_client:
        if not init_storage_clients():
            return create_error_response("Storage not initialized", 500)
    
    if not verify_api_key(req):
        return create_error_response("Unauthorized", 401)
    
    try:
        req_body = req.get_json()
        
        # ✅ MINIMALE ÄNDERUNG: erweitert für Joystick
        if not req_body:
            return create_error_response("Empty body")

        if "action" not in req_body and "joystick" not in req_body:
            return create_error_response("Missing 'action' or 'joystick'")
        
        req_body["timestamp"] = datetime.now(timezone.utc).isoformat()
        
        message = json.dumps(req_body)
        queue_client.send_message(message)
        
        logging.info(f"Command added to queue: {req_body.get('action') or 'joystick'}")
        return create_success_response({"message": "Command queued"})
        
    except ValueError:
        return create_error_response("Invalid JSON")
    except Exception as e:
        logging.error(f"Error adding command: {str(e)}")
        return create_error_response(f"Internal error: {str(e)}", 500)


@app.route(route="commands/poll", methods=["GET"])
def poll_commands(req: func.HttpRequest) -> func.HttpResponse:
    logging.info("poll_commands: Request received")
    
    if not queue_client:
        if not init_storage_clients():
            return create_error_response("Storage not initialized", 500)
    
    if not verify_api_key(req):
        return create_error_response("Unauthorized", 401)
    
    try:
        try:
            device_status = {
                "PartitionKey": "device",
                "RowKey": "status",
                "last_seen": datetime.now(timezone.utc).isoformat()
            }
            table_client.upsert_entity(device_status, mode=UpdateMode.REPLACE)
        except Exception as e:
            logging.warning(f"Could not update device status: {str(e)}")
        
        start_time = time.time()
        
        while time.time() - start_time < LONG_POLL_TIMEOUT:
            messages = queue_client.receive_messages(max_messages=1, visibility_timeout=60)
            
            for message in messages:
                try:
                    command = json.loads(message.content)
                    
                    queue_client.delete_message(message.id, message.pop_receipt)
                    
                    logging.info(f"Command retrieved: {command.get('action')}")
                    return create_success_response({"command": command})
                    
                except json.JSONDecodeError:
                    logging.error(f"Invalid JSON in queue message: {message.content}")
                    queue_client.delete_message(message.id, message.pop_receipt)
                    continue
            
            time.sleep(1)
        
        logging.info("poll_commands: Timeout reached, no commands")
        return create_success_response({"command": None})
        
    except Exception as e:
        logging.error(f"Error polling commands: {str(e)}")
        return create_error_response(f"Internal error: {str(e)}", 500)


@app.route(route="state", methods=["POST"])
def update_state(req: func.HttpRequest) -> func.HttpResponse:
    logging.info("update_state: Request received")
    
    if not table_client:
        if not init_storage_clients():
            return create_error_response("Storage not initialized", 500)
    
    if not verify_api_key(req):
        return create_error_response("Unauthorized", 401)
    
    try:
        req_body = req.get_json()
        
        if not req_body:
            return create_error_response("Empty request body")
        
        entity = {
            "PartitionKey": "motor",
            "RowKey": "current",
            "timestamp": datetime.now(timezone.utc).isoformat(),
            **req_body
        }
        
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
    logging.info("get_state: Request received")
    
    if not table_client:
        if not init_storage_clients():
            return create_error_response("Storage not initialized", 500)
    
    if not verify_api_key(req):
        return create_error_response("Unauthorized", 401)
    
    try:
        entity = table_client.get_entity(partition_key="motor", row_key="current")
        
        state = {k: v for k, v in entity.items() 
                if not k.startswith("_") and k not in ["PartitionKey", "RowKey"]}
        
        logging.info("State retrieved successfully")
        return create_success_response({"state": state})
        
    except Exception as e:
        if "ResourceNotFound" in str(e):
            return create_success_response({"state": None})
        
        logging.error(f"Error getting state: {str(e)}")
        return create_error_response(f"Internal error: {str(e)}", 500)


@app.route(route="device/status", methods=["GET"])
def get_device_status(req: func.HttpRequest) -> func.HttpResponse:
    logging.info("get_device_status: Request received")
    
    if not table_client:
        if not init_storage_clients():
            return create_error_response("Storage not initialized", 500)
    
    if not verify_api_key(req):
        return create_error_response("Unauthorized", 401)
    
    try:
        entity = table_client.get_entity(partition_key="device", row_key="status")
        
        last_seen_str = entity.get("last_seen")
        if last_seen_str:
            last_seen = datetime.fromisoformat(last_seen_str.replace('Z', '+00:00'))
            now = datetime.now(timezone.utc)
            seconds_ago = (now - last_seen).total_seconds()
            
            is_online = seconds_ago < 60
            
            response_data = {
                "online": is_online,
                "last_seen": last_seen_str,
                "seconds_ago": int(seconds_ago)
            }
        else:
            response_data = {
                "online": False,
                "last_seen": None,
                "seconds_ago": None
            }
        
        return create_success_response(response_data)
        
    except Exception as e:
        if "ResourceNotFound" in str(e):
            return create_success_response({
                "online": False,
                "last_seen": None,
                "seconds_ago": None
            })
        
        logging.error(f"Error getting device status: {str(e)}")
        return create_error_response(f"Internal error: {str(e)}", 500)


@app.route(route="health", methods=["GET"])
def health_check(req: func.HttpRequest) -> func.HttpResponse:
    response_data = {
        "status": "healthy",
        "timestamp": datetime.now(timezone.utc).isoformat()
    }
    
    try:
        version_file = Path(__file__).parent / "version.json"
        if version_file.exists():
            with open(version_file, 'r') as f:
                version_info = json.load(f)
                response_data["version"] = version_info
    except Exception as e:
        logging.warning(f"Could not load version info: {e}")
        response_data["version"] = {"error": "Version info not available"}
    
    return create_success_response(response_data)
