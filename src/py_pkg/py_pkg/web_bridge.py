from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.staticfiles import StaticFiles
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import JSONResponse
import rclpy
from rclpy.node import Node
from std_msgs.msg import String, UInt8MultiArray
from std_srvs.srv import Trigger
import asyncio
import uvicorn
import threading
import json
import os

app = FastAPI(title="ROS2 Web Bridge")

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)


class ConnectionManager:
    def __init__(self):
        self.active_connections = []

    async def connect(self, websocket: WebSocket):
        await websocket.accept()
        self.active_connections.append(websocket)

    def disconnect(self, websocket: WebSocket):
        if websocket in self.active_connections:
            self.active_connections.remove(websocket)

    async def broadcast(self, message: dict):
        for connection in self.active_connections:
            try:
                await connection.send_json(message)
            except Exception:
                pass


manager = ConnectionManager()
event_loop = None


@app.on_event("startup")
async def capture_event_loop():
    global event_loop
    event_loop = asyncio.get_running_loop()


def schedule_broadcast(data):
    if event_loop and manager.active_connections:
        try:
            asyncio.run_coroutine_threadsafe(manager.broadcast(data), event_loop)
        except Exception as e:
            print(f"Broadcast error: {e}")


class WebBridgeNode(Node):
    def __init__(self):
        super().__init__('web_bridge')

        # States: welcome → countdown → recording → recording_complete → analyzing → results
        self.state = "welcome"
        self.recording_start_time = None
        self.countdown_timer = None
        self.recording_timer = None

        self.arduino_data = []
        self.led_matrix_data = None
        self.classification_results = {
            "surveillance": None,
            "natural": None,
            "cultural": None,
        }
        self.received_model_count = 0

        # ── Subscriptions ────────────────────────────────────────────
        self.create_subscription(String, 'state_control', self.state_control_callback, 10)
        self.create_subscription(String, 'arduino_data',  self.arduino_callback,       10)
        self.create_subscription(String, 'nav_data',      self.nav_callback,            10)
        self.create_subscription(UInt8MultiArray, 'led_matrix', self.led_matrix_callback, 10)

        for model in ('surveillance', 'natural', 'cultural'):
            self.create_subscription(
                String,
                f'classification_results_{model}',
                lambda msg, m=model: self.classification_callback(msg, m),
                10,
            )

        # Optional LLM results
        self.create_subscription(String, 'model_results', self.llm_callback, 10)

        # ── Service clients ──────────────────────────────────────────
        self.start_recording_client  = self.create_client(Trigger, 'start_recording')
        self.stop_recording_client   = self.create_client(Trigger, 'stop_recording')
        self.classify_clients = {
            'surveillance': self.create_client(Trigger, 'classify_waveform_surveillance'),
            'natural':      self.create_client(Trigger, 'classify_waveform_natural'),
            'cultural':     self.create_client(Trigger, 'classify_waveform_cultural'),
        }

        self.get_logger().info('Web Bridge Node initialized')

    # ── State helpers ────────────────────────────────────────────────

    def broadcast_state(self):
        data = {
            "type": "state_change",
            "state": self.state,
            "timestamp": self.get_clock().now().to_msg().sec,
        }
        schedule_broadcast(data)
        self.get_logger().info(f'State → {self.state}')

    def _cancel_timer(self, attr):
        t = getattr(self, attr, None)
        if t:
            t.cancel()
            setattr(self, attr, None)

    # ── State transitions ────────────────────────────────────────────

    def start_countdown(self):
        self._cancel_timer('countdown_timer')
        self._cancel_timer('recording_timer')
        self.received_model_count = 0
        self.classification_results = {"surveillance": None, "natural": None, "cultural": None}
        self.state = "countdown"
        self.broadcast_state()
        self.countdown_timer = self.create_timer(3.0, self._countdown_done)

    def _countdown_done(self):
        self._cancel_timer('countdown_timer')
        self.state = "recording"
        self.recording_start_time = self.get_clock().now()
        self.broadcast_state()
        self._call_service(self.start_recording_client, "start_recording")
        self.recording_timer = self.create_timer(30.0, self._recording_done)

    def _recording_done(self):
        self._cancel_timer('recording_timer')
        self._call_service(self.stop_recording_client, "stop_recording")
        self.state = "recording_complete"
        self.broadcast_state()

    def trigger_classification(self):
        self.state = "analyzing"
        self.broadcast_state()
        for name, client in self.classify_clients.items():
            self._call_service(client, f"classify_waveform_{name}")

    def redo_recording(self):
        """Skip welcome — go straight to a fresh countdown."""
        self._call_service(self.stop_recording_client, "stop_recording")
        self.start_countdown()

    # ── Subscription callbacks ────────────────────────────────────────

    def state_control_callback(self, msg):
        """SELECT button from hardware. Starts countdown from welcome/ready.
        recording_complete → handled via REST endpoints only (webapp decides yes/no)."""
        self.get_logger().info(f'state_control received, current state: {self.state}')
        if self.state in ("welcome", "ready"):
            self.start_countdown()
        # In recording_complete: the webapp calls /api/classify or /api/redo instead.

    def arduino_callback(self, msg):
        """PRESS_n / RELEASE_n events — always forwarded to webapp."""
        data = {
            "type": "arduino",
            "button": msg.data,
            "timestamp": self.get_clock().now().to_msg().sec,
        }
        self.arduino_data.append(data)
        if len(self.arduino_data) > 100:
            self.arduino_data.pop(0)
        schedule_broadcast(data)

    def nav_callback(self, msg):
        """NAV_A/B/C/D, SELECT, BACK events."""
        schedule_broadcast({"type": "nav", "button": msg.data})

    def led_matrix_callback(self, msg):
        self.led_matrix_data = list(msg.data)
        height = msg.layout.dim[0].size if msg.layout.dim else 42
        width  = msg.layout.dim[1].size if len(msg.layout.dim) > 1 else 146
        schedule_broadcast({
            "type": "led_matrix",
            "width": width,
            "height": height,
            "data": self.led_matrix_data,
            "timestamp": self.get_clock().now().to_msg().sec,
        })

    def classification_callback(self, msg, model_name):
        self.classification_results[model_name] = msg.data
        self.received_model_count += 1
        schedule_broadcast({
            "type": "classification",
            "model": model_name,
            "results": msg.data,
            "timestamp": self.get_clock().now().to_msg().sec,
        })
        # Auto-transition to results once all 3 models have responded
        if self.received_model_count >= 3 and self.state == "analyzing":
            self.state = "results"
            self.broadcast_state()

    def llm_callback(self, msg):
        schedule_broadcast({"type": "llm_result", "data": msg.data})

    # ── Service caller ───────────────────────────────────────────────

    def _call_service(self, client, name):
        if not client.wait_for_service(timeout_sec=1.0):
            self.get_logger().warn(f'Service {name} not available')
            return
        future = client.call_async(Trigger.Request())
        future.add_done_callback(lambda f: self._service_done(f, name))

    def _service_done(self, future, name):
        try:
            resp = future.result()
            if resp.success:
                self.get_logger().info(f'{name}: {resp.message}')
            else:
                self.get_logger().warn(f'{name} failed: {resp.message}')
        except Exception as e:
            self.get_logger().error(f'{name} error: {e}')


# ── Global node ───────────────────────────────────────────────────────
bridge_node = None


# ── REST endpoints ────────────────────────────────────────────────────

@app.get("/api/status")
async def api_status():
    return {"message": "ROS2 Web Bridge API", "status": "running"}


@app.get("/api/state")
async def get_state():
    if bridge_node:
        return {"state": bridge_node.state, "recording_active": bridge_node.state == "recording"}
    return {"state": "unknown", "recording_active": False}


@app.post("/api/classify")
async def api_classify():
    """Webapp calls this when user confirms YES at the recording_complete prompt."""
    if bridge_node and bridge_node.state == "recording_complete":
        bridge_node.trigger_classification()
        return {"ok": True, "state": bridge_node.state}
    return JSONResponse(status_code=400, content={"error": "not in recording_complete state"})


@app.post("/api/redo")
async def api_redo():
    """Webapp calls this when user confirms NO — restarts directly to countdown."""
    if bridge_node and bridge_node.state == "recording_complete":
        bridge_node.redo_recording()
        return {"ok": True, "state": bridge_node.state}
    return JSONResponse(status_code=400, content={"error": "not in recording_complete state"})


@app.get("/api/arduino/latest")
async def get_latest_arduino():
    if bridge_node:
        return {"data": bridge_node.arduino_data[-10:]}
    return {"data": []}


@app.get("/api/classifications")
async def get_classifications():
    if bridge_node:
        return bridge_node.classification_results
    return {"surveillance": None, "natural": None, "cultural": None}


@app.get("/api/sounds")
async def get_sound_files():
    sounds_mapping = {}
    sounds_paths = [
        os.path.expanduser("~/iaac/ai4all/rosnetwork/sounds"),
        "/home/salva/iaac/ai4all/rosnetwork/sounds",
    ]
    sounds_base = next((p for p in sounds_paths if os.path.exists(p)), None)
    if sounds_base:
        for button_num in range(1, 11):
            button_dirs = [
                d for d in os.listdir(sounds_base)
                if d.startswith(f"{button_num}. ") and os.path.isdir(os.path.join(sounds_base, d))
            ]
            if button_dirs:
                subdir = os.path.join(sounds_base, button_dirs[0])
                wav_files = [f for f in os.listdir(subdir) if f.endswith('.wav')]
                dir_name = button_dirs[0].replace(' ', '%20')
                sounds_mapping[str(button_num)] = [
                    f"/sounds/{dir_name}/{f.replace(' ', '%20')}" for f in wav_files
                ]
    return {"sounds": sounds_mapping}


# ── WebSocket ─────────────────────────────────────────────────────────

@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    await manager.connect(websocket)
    # Send current state immediately on connect
    if bridge_node:
        await websocket.send_json({
            "type": "state_change",
            "state": bridge_node.state,
            "timestamp": 0,
        })
    try:
        while True:
            await websocket.receive_text()
    except WebSocketDisconnect:
        manager.disconnect(websocket)


# ── Static files ──────────────────────────────────────────────────────

sounds_paths = [
    os.path.expanduser("~/iaac/ai4all/rosnetwork/sounds"),
    "/home/salva/iaac/ai4all/rosnetwork/sounds",
]
for sp in sounds_paths:
    if os.path.exists(sp):
        app.mount("/sounds", StaticFiles(directory=sp), name="sounds")
        print(f"Serving sounds from: {sp}")
        break
else:
    print("Warning: sounds directory not found!")

# Serve built React app (production). During dev, Vite dev server proxies to :8000.
webapp_paths = [
    os.path.expanduser("~/iaac/ai4all/rosnetwork/ai-dj-webapp/dist"),
    "/home/salva/iaac/ai4all/rosnetwork/ai-dj-webapp/dist",
    # Legacy fallback
    os.path.expanduser("~/iaac/ai4all/rosnetwork/webapp"),
    "/home/salva/iaac/ai4all/rosnetwork/webapp",
]
for wp in webapp_paths:
    if os.path.exists(wp):
        app.mount("/", StaticFiles(directory=wp, html=True), name="webapp")
        print(f"Serving webapp from: {wp}")
        break
else:
    print("Warning: webapp directory not found!")


# ── Entry point ───────────────────────────────────────────────────────

def run_ros_node():
    global bridge_node
    rclpy.init()
    bridge_node = WebBridgeNode()
    rclpy.spin(bridge_node)
    rclpy.shutdown()


def main():
    ros_thread = threading.Thread(target=run_ros_node, daemon=True)
    ros_thread.start()
    uvicorn.run(app, host="0.0.0.0", port=8000, log_level="info")


if __name__ == "__main__":
    main()
