import asyncio
import websockets
import json
from flask import Flask
from flask_socketio import SocketIO, emit
import threading

# Flask-SocketIO Setup
app = Flask(__name__)
socketio = SocketIO(app, cors_allowed_origins='*', async_mode='threading')

# Serve the dashboard (optional)
@app.route('/')
def index():
    return "✅ Flask-SocketIO Server is running!"

# SocketIO events for browser clients
@socketio.on('connect')
def on_connect():
    print("✅ Browser connected!")

@socketio.on('disconnect')
def on_disconnect():
    print("❌ Browser disconnected!")

# ESP32 WebSocket Server Handler
async def ws_handler(websocket):
    print(f"✅ ESP32 Connected from {websocket.remote_address}")

    try:
        async for message in websocket:
            print(f"📨 Received from ESP32 [{websocket.remote_address}]: {message}")

            try:
                data = json.loads(message)

                # Add IP as device_id if not present
                if 'device_id' not in data:
                    data['device_id'] = websocket.remote_address[0]

                # Emit to browser clients
                socketio.emit('update_dashboard', data)
                print(f"➡️ Emitted to browser clients: {data}")

            except json.JSONDecodeError:
                print("❌ Invalid JSON received from ESP32")

    except websockets.exceptions.ConnectionClosed:
        print(f"❌ ESP32 {websocket.remote_address} Disconnected!")

# Async WebSocket Server Runner
async def ws_main():
    print("🔥 ESP32 WebSocket Server running on 172.20.10.4:9000...")
    async with websockets.serve(ws_handler, "172.20.10.4", 9000):  # bind to this IP
        await asyncio.Future()  # Run forever

# Thread target to run the asyncio event loop for the WebSocket server
def start_websocket_server():
    asyncio.run(ws_main())

if __name__ == '__main__':
    # Start the WebSocket server in a separate thread
    ws_thread = threading.Thread(target=start_websocket_server)
    ws_thread.start()

    # Start the Flask-SocketIO server (main thread)
    print("🔥 Flask-SocketIO Server running on 172.20.10.4:5000...")
    socketio.run(app, host='172.20.10.4', port=5000)
