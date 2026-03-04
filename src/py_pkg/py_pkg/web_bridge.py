from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.staticfiles import StaticFiles
from fastapi.responses import HTMLResponse
from fastapi.middleware.cors import CORSMiddleware
import rclpy
from rclpy.node import Node
from std_msgs.msg import String
from std_srvs.srv import Trigger
import asyncio
import uvicorn
import threading
import json
from typing import List
import os

app = FastAPI(title="ROS2 Web Bridge")

# CORS middleware for development
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# Store connected WebSocket clients
class ConnectionManager:
    def __init__(self):
        self.active_connections: List[WebSocket] = []

    async def connect(self, websocket: WebSocket):
        await websocket.accept()
        self.active_connections.append(websocket)

    def disconnect(self, websocket: WebSocket):
        self.active_connections.remove(websocket)

    async def broadcast(self, message: dict):
        for connection in self.active_connections:
            try:
                await connection.send_json(message)
            except:
                pass

manager = ConnectionManager()


class WebBridgeNode(Node):
    def __init__(self):
        super().__init__('web_bridge')
        
        # Application state machine
        self.state = "welcome"  # States: welcome, ready, recording, analyzing, results
        self.recording_start_time = None
        self.waveform_file = None
        
        # Store latest data
        self.arduino_data = []
        self.led_matrix_data = None
        self.waveform_status = {"recording": False, "duration": 0}
        self.classification_results = {
            "model1": [],
            "model2": [],
            "model3": []
        }
        
        # Subscribe to state control (button 11)
        self.state_control_sub = self.create_subscription(
            String,
            'state_control',
            self.state_control_callback,
            10
        )
        
        # Subscribe to Arduino data (buttons 1-10)
        self.arduino_sub = self.create_subscription(
            String,
            'arduino_data',
            self.arduino_callback,
            10
        )
        
        # Subscribe to LED matrix updates
        self.led_matrix_sub = self.create_subscription(
            String,
            'led_matrix_status',  # You may need to add this publisher to build_waveform
            self.led_matrix_callback,
            10
        )
        
        # Subscribe to classification results from 3 models
        self.classification_sub1 = self.create_subscription(
            String,
            'classification_results_model1',
            lambda msg: self.classification_callback(msg, 'model1'),
            10
        )
        
        self.classification_sub2 = self.create_subscription(
            String,
            'classification_results_model2',
            lambda msg: self.classification_callback(msg, 'model2'),
            10
        )
        
        self.classification_sub3 = self.create_subscription(
            String,
            'classification_results_model3',
            lambda msg: self.classification_callback(msg, 'model3'),
            10
        )
        
        # Service clients for controlling other nodes
        self.start_recording_client = self.create_client(Trigger, 'start_recording')
        self.stop_recording_client = self.create_client(Trigger, 'stop_recording')
        
        # Clients for YAMNet classification services (names match three_yamnet_models.launch.py)
        self.classify_model1_client = self.create_client(Trigger, 'classify_waveform_Model_1_Original')
        self.classify_model2_client = self.create_client(Trigger, 'classify_waveform_Model_2_DatasetA')
        self.classify_model3_client = self.create_client(Trigger, 'classify_waveform_Model_3_DatasetB')
        
        self.get_logger().info('Web Bridge Node initialized')
    
    def state_control_callback(self, msg):
        """Handle button 11 press for state transitions"""
        self.get_logger().info(f'State control pressed. Current state: {self.state}')
        
        if self.state == "welcome":
            self.state = "ready"
            self.broadcast_state_change()
        elif self.state == "ready":
            self.state = "countdown"
            self.broadcast_state_change()
            # Start countdown timer (3 seconds)
            self.create_timer(3.0, self.start_recording)
        elif self.state == "recording_complete":
            self.state = "analyzing"
            self.broadcast_state_change()
            # Trigger classification on all 3 YAMNet models
            self.trigger_classification()
        
    def start_recording(self):
        """Called after countdown completes"""
        self.state = "recording"
        self.recording_start_time = self.get_clock().now()
        self.broadcast_state_change()
        
        # Call service to start recording on build_waveform node
        self.call_start_recording_service()
        
        # Start 30-second recording timer
        self.create_timer(30.0, self.complete_recording)
    
    def complete_recording(self):
        """Called after 30 seconds of recording"""
        # Call service to stop recording
        self.call_stop_recording_service()
        
        self.state = "recording_complete"
        self.broadcast_state_change()
    
    def broadcast_state_change(self):
        """Broadcast state change to all connected clients"""
        data = {
            "type": "state_change",
            "state": self.state,
            "timestamp": self.get_clock().now().to_msg().sec
        }
        asyncio.run(manager.broadcast(data))
        self.get_logger().info(f'State changed to: {self.state}')
    
    def call_start_recording_service(self):
        """Call service to start recording on build_waveform node"""
        if not self.start_recording_client.wait_for_service(timeout_sec=1.0):
            self.get_logger().warn('start_recording service not available')
            return
        
        request = Trigger.Request()
        future = self.start_recording_client.call_async(request)
        future.add_done_callback(self.start_recording_response_callback)
    
    def start_recording_response_callback(self, future):
        try:
            response = future.result()
            if response.success:
                self.get_logger().info(f'Recording started: {response.message}')
            else:
                self.get_logger().warn(f'Failed to start recording: {response.message}')
        except Exception as e:
            self.get_logger().error(f'Service call failed: {e}')
    
    def call_stop_recording_service(self):
        """Call service to stop recording on build_waveform node"""
        if not self.stop_recording_client.wait_for_service(timeout_sec=1.0):
            self.get_logger().warn('stop_recording service not available')
            return
        
        request = Trigger.Request()
        future = self.stop_recording_client.call_async(request)
        future.add_done_callback(self.stop_recording_response_callback)
    
    def stop_recording_response_callback(self, future):
        try:
            response = future.result()
            if response.success:
                self.get_logger().info(f'Recording stopped: {response.message}')
                # Extract waveform file from response if available
                self.waveform_file = response.message
            else:
                self.get_logger().warn(f'Failed to stop recording: {response.message}')
        except Exception as e:
            self.get_logger().error(f'Service call failed: {e}')
    
    def trigger_classification(self):
        """Trigger classification on all 3 YAMNet models"""
        self.get_logger().info('Triggering classification on all models')
        
        # Call classification service for each model
        for client, model_name in [
            (self.classify_model1_client, 'Model 1'),
            (self.classify_model2_client, 'Model 2'),
            (self.classify_model3_client, 'Model 3')
        ]:
            if not client.wait_for_service(timeout_sec=1.0):
                self.get_logger().warn(f'Classification service for {model_name} not available')
                continue
            
            request = Trigger.Request()
            future = client.call_async(request)
            future.add_done_callback(
                lambda f, name=model_name: self.classification_service_callback(f, name)
            )
    
    def classification_service_callback(self, future, model_name):
        try:
            response = future.result()
            if response.success:
                self.get_logger().info(f'{model_name} classification: {response.message}')
            else:
                self.get_logger().warn(f'{model_name} classification failed: {response.message}')
        except Exception as e:
            self.get_logger().error(f'{model_name} service call failed: {e}')
    
    def led_matrix_callback(self, msg):
        """Handle LED matrix updates"""
        self.led_matrix_data = msg.data
        data = {
            "type": "led_matrix",
            "data": msg.data,
            "timestamp": self.get_clock().now().to_msg().sec
        }
        asyncio.run(manager.broadcast(data))
    
    def arduino_callback(self, msg):
        """Handle Arduino button press data (buttons 1-10)"""
        # Only process button presses during recording state
        if self.state != "recording":
            return
            
        data = {
            "type": "arduino",
            "button": msg.data,
            "timestamp": self.get_clock().now().to_msg().sec
        }
        self.arduino_data.append(data)
        
        # Keep only last 100 events
        if len(self.arduino_data) > 100:
            self.arduino_data.pop(0)
        
        # Broadcast to all connected clients
        asyncio.run(manager.broadcast(data))
        self.get_logger().info(f'Arduino data: {msg.data}')
    
    def classification_callback(self, msg, model_name):
        """Handle classification results from YAMNet models"""
        data = {
            "type": "classification",
            "model": model_name,
            "results": msg.data,
            "timestamp": self.get_clock().now().to_msg().sec
        }
        self.classification_results[model_name].append(data)
        
        # Keep only last 10 results per model
        if len(self.classification_results[model_name]) > 10:
            self.classification_results[model_name].pop(0)
        
        # Broadcast to all connected clients
        asyncio.run(manager.broadcast(data))
        self.get_logger().info(f'Classification from {model_name}')


# Global node instance
bridge_node = None


# REST API Endpoints
@app.get("/")
async def root():
    return {"message": "ROS2 Web Bridge API", "status": "running"}


@app.get("/api/arduino/latest")
async def get_latest_arduino():
    """Get latest Arduino button presses"""
    if bridge_node:
        return {"data": bridge_node.arduino_data[-10:]}
    return {"data": []}


@app.get("/api/state")
async def get_state():
    """Get current application state"""
    if bridge_node:
        return {
            "state": bridge_node.state,
            "recording_active": bridge_node.state == "recording"
        }
    return {"state": "unknown", "recording_active": False}


@app.get("/api/classifications")
async def get_classifications():
    """Get all classification results from 3 models"""
    if bridge_node:
        return {
            "model1": bridge_node.classification_results["model1"],
            "model2": bridge_node.classification_results["model2"],
            "model3": bridge_node.classification_results["model3"]
        }
    return {"model1": [], "model2": [], "model3": []}


@app.get("/api/classifications/{model_name}")
async def get_model_classifications(model_name: str):
    """Get classification results for a specific model"""
    if bridge_node and model_name in bridge_node.classification_results:
        return {"data": bridge_node.classification_results[model_name]}
    return {"data": []}


@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    """WebSocket endpoint for real-time updates"""
    await manager.connect(websocket)
    try:
        while True:
            # Keep connection alive
            data = await websocket.receive_text()
            # Echo back for ping/pong
            await websocket.send_text(f"Echo: {data}")
    except WebSocketDisconnect:
        manager.disconnect(websocket)


# Serve static files (webapp folder)
webapp_path = os.path.expanduser("~/rosnetwork/webapp")
if os.path.exists(webapp_path):
    app.mount("/app", StaticFiles(directory=webapp_path, html=True), name="webapp")


def run_ros_node():
    """Run ROS2 node in separate thread"""
    global bridge_node
    rclpy.init()
    bridge_node = WebBridgeNode()
    rclpy.spin(bridge_node)
    rclpy.shutdown()


def main():
    """Main entry point"""
    # Start ROS2 node in background thread
    ros_thread = threading.Thread(target=run_ros_node, daemon=True)
    ros_thread.start()
    
    # Start FastAPI server
    uvicorn.run(
        app,
        host="0.0.0.0",
        port=8000,
        log_level="info"
    )


if __name__ == "__main__":
    main()
