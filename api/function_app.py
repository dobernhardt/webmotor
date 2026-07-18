"""
WebMotor Cloud Backend - Azure Functions
Provides API endpoints for remote motor control via cloud
"""

import azure.functions as func
import json
import logging
import os
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
# Drive targets older than this are reported but considered stale by the ESP
DRIVE_TARGET_MAX_AGE_S = 2.0

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


def get_entity_or_none(partition_key: str, row_key: str):
    """Read a table entity, returning None if it does not exist"""
    try:
        return table_client.get_entity(partition_key=partition_key, row_key=row_key)
    except Exception as e:
        if "ResourceNotFound" in str(e):
            return None
        raise


def get_drive_target():
    """Return the latest drive target as dict with age_s, or None"""
    entity = get_entity_or_none("drive", "target")
    if not entity:
        return None

    age_s = None
    ts_str = entity.get("ts")
    if ts_str:
        ts = datetime.fromisoformat(ts_str.replace('Z', '+00:00'))
        age_s = (datetime.now(timezone.utc) - ts).total_seconds()

    return {
        "x": entity.get("x", 0.0),
        "y": entity.get("y", 0.0),
        "ts": ts_str,
        "age_s": age_s
    }


def get_drive_config():
    """Return the axis limit configuration as dict, or None if never set"""
    entity = get_entity_or_none("drive", "config")
    if not entity:
        return None

    return {
        "rotationLimitDeg": entity.get("rotationLimitDeg"),
        "tiltLimitDeg": entity.get("tiltLimitDeg")
    }


def receive_one_command():
    """Fetch and delete at most one command from the queue (non-blocking)"""
    messages = queue_client.receive_messages(max_messages=1, visibility_timeout=60)
    for message in messages:
        try:
            command = json.loads(message.content)
            queue_client.delete_message(message.id, message.pop_receipt)
            logging.info(f"Command retrieved: {command.get('action')}")
            return command
        except json.JSONDecodeError:
            logging.error(f"Invalid JSON in queue message: {message.content}")
            queue_client.delete_message(message.id, message.pop_receipt)
    return None


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

        # Joystick values do NOT go through the queue (they would pile up
        # and be executed stale) - they use POST /drive instead.
        if not req_body or "action" not in req_body:
            return create_error_response("Missing 'action' in request body")

        req_body["timestamp"] = datetime.now(timezone.utc).isoformat()

        message = json.dumps(req_body)
        queue_client.send_message(message)

        logging.info(f"Command added to queue: {req_body.get('action')}")
        return create_success_response({"message": "Command queued"})

    except ValueError:
        return create_error_response("Invalid JSON")
    except Exception as e:
        logging.error(f"Error adding command: {str(e)}")
        return create_error_response(f"Internal error: {str(e)}", 500)


@app.route(route="sync", methods=["GET"])
def sync(req: func.HttpRequest) -> func.HttpResponse:
    """
    GET /api/sync
    Single short-poll endpoint for the ESP32. Returns the latest drive
    target (latest-value semantics), the drive configuration and at most
    one discrete command from the queue. Answers immediately - the ESP
    polls this every few hundred milliseconds while driving.
    """
    if not queue_client or not table_client:
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

        return create_success_response({
            "drive": get_drive_target(),
            "config": get_drive_config(),
            "command": receive_one_command()
        })

    except Exception as e:
        logging.error(f"Error in sync: {str(e)}")
        return create_error_response(f"Internal error: {str(e)}", 500)


@app.route(route="drive", methods=["POST"])
def set_drive_target(req: func.HttpRequest) -> func.HttpResponse:
    """
    POST /api/drive
    Store the desired drive state {x, y} (called by frontend at ~10 Hz).
    Latest-value semantics: each write replaces the previous target, so the
    ESP always acts on the most recent joystick position.
    """
    if not table_client:
        if not init_storage_clients():
            return create_error_response("Storage not initialized", 500)

    if not verify_api_key(req):
        return create_error_response("Unauthorized", 401)

    try:
        req_body = req.get_json()

        if not req_body or "x" not in req_body or "y" not in req_body:
            return create_error_response("Missing 'x' or 'y' in request body")

        x = max(-1.0, min(1.0, float(req_body["x"])))
        y = max(-1.0, min(1.0, float(req_body["y"])))

        entity = {
            "PartitionKey": "drive",
            "RowKey": "target",
            "x": x,
            "y": y,
            "ts": datetime.now(timezone.utc).isoformat()
        }
        table_client.upsert_entity(entity, mode=UpdateMode.REPLACE)

        return create_success_response({"message": "Drive target updated"})

    except (ValueError, TypeError):
        return create_error_response("Invalid JSON or non-numeric x/y")
    except Exception as e:
        logging.error(f"Error setting drive target: {str(e)}")
        return create_error_response(f"Internal error: {str(e)}", 500)


@app.route(route="drive", methods=["GET"])
def read_drive_target(req: func.HttpRequest) -> func.HttpResponse:
    """GET /api/drive - return the latest drive target (debugging aid)"""
    if not table_client:
        if not init_storage_clients():
            return create_error_response("Storage not initialized", 500)

    if not verify_api_key(req):
        return create_error_response("Unauthorized", 401)

    try:
        return create_success_response({"drive": get_drive_target()})
    except Exception as e:
        logging.error(f"Error reading drive target: {str(e)}")
        return create_error_response(f"Internal error: {str(e)}", 500)


@app.route(route="drive/config", methods=["GET"])
def read_drive_config(req: func.HttpRequest) -> func.HttpResponse:
    """GET /api/drive/config - return the axis limits"""
    if not table_client:
        if not init_storage_clients():
            return create_error_response("Storage not initialized", 500)

    if not verify_api_key(req):
        return create_error_response("Unauthorized", 401)

    try:
        return create_success_response({"config": get_drive_config()})
    except Exception as e:
        logging.error(f"Error reading drive config: {str(e)}")
        return create_error_response(f"Internal error: {str(e)}", 500)


@app.route(route="drive/config", methods=["POST"])
def set_drive_config(req: func.HttpRequest) -> func.HttpResponse:
    """
    POST /api/drive/config
    Store the axis limits {rotationLimitDeg, tiltLimitDeg}. The WebUI owns
    these values; the ESP picks up changes via /sync and applies them
    without persisting (they are re-applied after every boot).
    """
    if not table_client:
        if not init_storage_clients():
            return create_error_response("Storage not initialized", 500)

    if not verify_api_key(req):
        return create_error_response("Unauthorized", 401)

    try:
        req_body = req.get_json()

        if not req_body:
            return create_error_response("Empty request body")

        rotation_limit_deg = float(req_body.get("rotationLimitDeg", -1))
        tilt_limit_deg = float(req_body.get("tiltLimitDeg", -1))

        if not (0 <= rotation_limit_deg <= 180):
            return create_error_response("rotationLimitDeg must be between 0 and 180")
        if not (0 <= tilt_limit_deg <= 90):
            return create_error_response("tiltLimitDeg must be between 0 and 90")

        entity = {
            "PartitionKey": "drive",
            "RowKey": "config",
            "rotationLimitDeg": rotation_limit_deg,
            "tiltLimitDeg": tilt_limit_deg,
            "ts": datetime.now(timezone.utc).isoformat()
        }
        table_client.upsert_entity(entity, mode=UpdateMode.REPLACE)

        logging.info(f"Axis limits updated: rotation={rotation_limit_deg} deg, tilt={tilt_limit_deg} deg")
        return create_success_response({"message": "Axis limits updated"})

    except (ValueError, TypeError):
        return create_error_response("Invalid JSON or non-numeric values")
    except Exception as e:
        logging.error(f"Error setting drive config: {str(e)}")
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
