import os
import time
import json
import queue
import signal
import uvicorn
import logging
import asyncio
import threading
import websockets
from uvicorn import Config, Server
from http_server import create_app
from agent_thread import run_agent_thread
from handlers.audio_handler import AudioStreamSession

logging.basicConfig(level=logging.INFO)

UPLOAD_DIR = "./recordings"
os.makedirs(UPLOAD_DIR, exist_ok=True)

connected_clients = {}  # {websocket: last_hello_timestamp}
stream_sessions = {}  # Map client_id -> AudioStreamSession

agent_in_queue = queue.Queue() # Queue for incoming messages from ESP32 to the agent
agent_out_queue = queue.Queue() # Queue for outgoing messages from the agent to ESP32

shutdown_event = threading.Event() # Threads signal for shutdown

# ────────────── WebSocket Handler ──────────────
async def handle_client(websocket):
    client_id = str(id(websocket))
    logging.info(f"[WS] Client connected: {client_id}")
    connected_clients[websocket] = asyncio.get_event_loop().time()

    try:
        async for message in websocket:

            # Try parsing as JSON if starts with {
            if isinstance(message, str) and message.strip().startswith("{"):
                try:
                    logging.info(f"[WS] Received from {client_id}: {message}")
                    parsed = json.loads(message)
                    parsed["sender"] = client_id
                    msg_type = parsed.get("type")

                    if msg_type == "START_STREAM":
                        # Initialize a new audio stream session
                        logging.info(f"[WS] START_STREAM initialized for {client_id}")
                        filepath = os.path.join(UPLOAD_DIR, f"stream_{client_id}.wav")
                        # Remove old file if it exists
                        if os.path.exists(filepath):
                            os.remove(filepath)
                            logging.info(f"[WS] Removed old stream file: {filepath}")

                        # Create new audio session
                        stream_sessions[client_id] = AudioStreamSession(filepath)
                        parsed["filepath"] = filepath
                        # continue

                    elif msg_type == "STOP_STREAM":
                        # Stop the audio stream session
                        logging.info(f"[WS] STOP_STREAM received for {client_id}")
                        session = stream_sessions.get(client_id)
                        if session:
                            session.close()
                            del stream_sessions[client_id]
                            parsed["fname"] = session.filepath
                            logging.info(f"[WS] STOP_STREAM closed and saved to {session.filepath} for {client_id}")

                            # Send to agent for processing, wave file should be there
                            agent_in_queue.put({
                                "type": "ASSISTANT_PROCESS_WAVE",
                                   "fname": session.filepath,
                                "sender": client_id
                             })
                        # continue

                    else:
                        # Any other JSON
                        agent_in_queue.put(parsed)
                        logging.info(f"[WS] JSON message '{msg_type}' added to agent_in_queue")
                except json.JSONDecodeError as e:
                    logging.warning(f"[WS] Invalid JSON from {client_id}: {e}")

            elif isinstance(message, bytes):
                session = stream_sessions.get(client_id)
                if session:
                    session.write_chunk(message)
                    logging.debug(f"[WS] Audio chunk received from {client_id}, size: {bytes}")
                else:
                    logging.warning(f"[WS] Binary received from {client_id} but no stream session active")

            else:
               # Treat as raw command string (e.g. [HELLO])
               logging.info(f"[WS] Received from {client_id}: {message}")
               agent_in_queue.put({
                   "sender": client_id,
                   "type": message
               })


    except websockets.exceptions.ConnectionClosed:
        logging.info(f"[WS] Client disconnected: {client_id}")
    finally:
        connected_clients.pop(websocket, None)
        if client_id in stream_sessions:
            stream_sessions[client_id].close()
            del stream_sessions[client_id]
            logging.info(f"[WS] Cleaned up audio session for {client_id}")

# ────────────── Pinger Task ──────────────
async def ping_clients():
    while not shutdown_event.is_set():
        await asyncio.sleep(60)
        for ws in list(connected_clients.keys()):
            try:
                output = {
                    "sender": str(id(ws)),
                    "type": "ASSISTANT_TEXT_RESPONSE",
                    "content": "Ping from server",
                    "mode": "ping",
                    "language": "en",
                }
                # await ws.send(json.dumps(output))
                # logging.info(f"[ASSISTANT_TEXT_RESPONSE] sent to {id(ws)}")
            except websockets.exceptions.ConnectionClosed:
                connected_clients.pop(ws, None)

# ────────────── Agent Dispatcher ──────────────
async def dispatch_agent_responses():
    while not shutdown_event.is_set():
        try:
            msg = agent_out_queue.get_nowait()
            sender = msg.get("sender")
            payload = json.dumps(msg)

            if sender is None or sender == "" or sender == "None":
                logging.info("[WS] Broadcasting message to all clients")
                for ws in list(connected_clients.keys()):
                    try:
                        await ws.send(payload)
                    except Exception as e:
                        logging.warning(f"[WS] Failed to send to client: {e}")
            else:
                for ws in list(connected_clients.keys()):
                    if str(id(ws)) == str(sender):
                        await ws.send(payload)
                        logging.info(f"[WS] Sent response {msg.get('type')} to client: {sender}")
                        break
        except queue.Empty:
            await asyncio.sleep(0.1)


# ────────────── HTTP Server ──────────────
def run_fastapi_server():
    config = Config(app=create_app(agent_in_queue), host="0.0.0.0", port=8766, log_level="info")
    server = Server(config)
    logging.info("[HTTP] Starting FastAPI server at http://0.0.0.0:8766")
    asyncio.run(server.serve())

# ────────────── Websocket Server ──────────────
async def main():
    server = await websockets.serve(handle_client, "0.0.0.0", 8765)
    logging.info("[WS] Server started at ws://0.0.0.0:8765")

    asyncio.create_task(ping_clients())
    asyncio.create_task(dispatch_agent_responses())

    await server.wait_closed()

# ────────────── Signal Handling ──────────────
def shutdown_handler(sig, frame):
    logging.info("[SYS] Ctrl+C detected. Shutting down...")
    shutdown_event.set()
    agent_in_queue.put(None)  # Unblock the agent thread

signal.signal(signal.SIGINT, shutdown_handler)
signal.signal(signal.SIGTERM, shutdown_handler)

if __name__ == "__main__":
    # Start Fast API
    fastapi_thread = threading.Thread(target=run_fastapi_server, daemon=True)
    fastapi_thread.start()

    # Start agent thread
    agent_thread = threading.Thread(
        target=run_agent_thread,
        args=(agent_in_queue, agent_out_queue, shutdown_event),
        daemon=False  # daemon=False ensures we can join later
    )
    agent_thread.start()

    try:
        asyncio.run(main())
    finally:
        logging.info("[SYS] Waiting for agent to stop...")
        agent_thread.join()
        fastapi_thread.join()
        logging.info("[SYS] Server shutdown complete.")

    logging.info("[SYS] FastAPI server shutdown complete.")

