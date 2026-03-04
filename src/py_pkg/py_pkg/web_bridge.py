from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.staticfiles import StaticFiles
from fastapi.responses import HTMLResponse
from fastapi.middleware.cors import CORSMiddleware
import rclpy
from rclpy.node import Node
from std_msgs.msg import String
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
        
        # Store latest data
        self.arduino_data = []
        self.waveform_status = {"recording": False, "duration": 0}
        self.classification_results = {
            "model1": [],
            "model2": [],
            "model3": []
        }
        
        # Subscribe to Arduino data
        self.arduino_sub = self.create_subscription(
            String,
            'arduino_data',
            self.arduino_callback,
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
        
        self.get_logger().info('Web Bridge Node initialized')
    
    def arduino_callback(self, msg):
        """Handle Arduino button press data"""
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
