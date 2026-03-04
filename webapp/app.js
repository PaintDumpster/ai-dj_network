// WebSocket connection
let ws = null;
let reconnectInterval = null;

// DOM elements
const connectionStatus = document.getElementById('connection-status');
const arduinoLog = document.getElementById('arduino-log');
const buttons = document.querySelectorAll('.button');

// Model result containers
const model1Results = document.getElementById('model1-results');
const model2Results = document.getElementById('model2-results');
const model3Results = document.getElementById('model3-results');

// Connect to WebSocket
function connectWebSocket() {
    const wsUrl = 'ws://localhost:8000/ws';
    
    try {
        ws = new WebSocket(wsUrl);
        
        ws.onopen = () => {
            console.log('WebSocket connected');
            connectionStatus.textContent = '● Connected';
            connectionStatus.className = 'status-connected';
            
            // Clear reconnect interval if exists
            if (reconnectInterval) {
                clearInterval(reconnectInterval);
                reconnectInterval = null;
            }
            
            // Send ping every 30 seconds to keep alive
            setInterval(() => {
                if (ws.readyState === WebSocket.OPEN) {
                    ws.send('ping');
                }
            }, 30000);
        };
        
        ws.onmessage = (event) => {
            try {
                const data = JSON.parse(event.data);
                handleMessage(data);
            } catch (e) {
                console.log('Non-JSON message:', event.data);
            }
        };
        
        ws.onerror = (error) => {
            console.error('WebSocket error:', error);
        };
        
        ws.onclose = () => {
            console.log('WebSocket disconnected');
            connectionStatus.textContent = '● Disconnected';
            connectionStatus.className = 'status-disconnected';
            
            // Try to reconnect after 3 seconds
            if (!reconnectInterval) {
                reconnectInterval = setInterval(connectWebSocket, 3000);
            }
        };
    } catch (error) {
        console.error('Failed to connect:', error);
    }
}

// Handle incoming WebSocket messages
function handleMessage(data) {
    console.log('Received:', data);
    
    if (data.type === 'arduino') {
        handleArduinoData(data);
    } else if (data.type === 'classification') {
        handleClassificationData(data);
    }
}

// Handle Arduino button press data
function handleArduinoData(data) {
    const buttonNumber = data.button.trim();
    
    // Highlight the pressed button
    buttons.forEach(btn => {
        if (btn.dataset.button === buttonNumber) {
            btn.classList.add('active');
            setTimeout(() => btn.classList.remove('active'), 500);
        }
    });
    
    // Add to event log
    const logEntry = document.createElement('p');
    const timestamp = new Date(data.timestamp * 1000).toLocaleTimeString();
    logEntry.textContent = `[${timestamp}] Button ${buttonNumber} pressed`;
    arduinoLog.insertBefore(logEntry, arduinoLog.firstChild);
    
    // Keep only last 10 entries
    while (arduinoLog.children.length > 10) {
        arduinoLog.removeChild(arduinoLog.lastChild);
    }
}

// Handle YAMNet classification results
function handleClassificationData(data) {
    const modelName = data.model;
    const results = data.results;
    
    let targetElement;
    if (modelName === 'model1') targetElement = model1Results;
    else if (modelName === 'model2') targetElement = model2Results;
    else if (modelName === 'model3') targetElement = model3Results;
    else return;
    
    // Clear "no data" message
    targetElement.innerHTML = '';
    
    // Parse and display results
    const resultEntry = document.createElement('div');
    resultEntry.className = 'result-item';
    
    const timestamp = new Date(data.timestamp * 1000).toLocaleTimeString();
    resultEntry.innerHTML = `
        <div style="font-size: 0.8rem; color: var(--text-muted); margin-bottom: 0.5rem;">
            ${timestamp}
        </div>
        <pre style="white-space: pre-wrap; font-size: 0.85rem;">${results}</pre>
    `;
    
    targetElement.insertBefore(resultEntry, targetElement.firstChild);
    
    // Keep only last 3 results per model
    while (targetElement.children.length > 3) {
        targetElement.removeChild(targetElement.lastChild);
    }
}

// Fetch initial data from REST API
async function fetchInitialData() {
    try {
        // Fetch latest Arduino data
        const arduinoResponse = await fetch('http://localhost:8000/api/arduino/latest');
        const arduinoData = await arduinoResponse.json();
        
        if (arduinoData.data && arduinoData.data.length > 0) {
            arduinoData.data.reverse().forEach(item => {
                const logEntry = document.createElement('p');
                const timestamp = new Date(item.timestamp * 1000).toLocaleTimeString();
                logEntry.textContent = `[${timestamp}] Button ${item.button} pressed`;
                arduinoLog.appendChild(logEntry);
            });
        }
        
        // Fetch classification results
        const classificationsResponse = await fetch('http://localhost:8000/api/classifications');
        const classifications = await classificationsResponse.json();
        
        // Display results for each model
        Object.entries(classifications).forEach(([modelName, results]) => {
            if (results.length > 0) {
                results.slice(-3).forEach(result => {
                    handleClassificationData(result);
                });
            }
        });
        
    } catch (error) {
        console.error('Failed to fetch initial data:', error);
    }
}

// Initialize on page load
document.addEventListener('DOMContentLoaded', () => {
    console.log('Initializing ROS2 Web Interface');
    connectWebSocket();
    fetchInitialData();
    
    // Refresh data every 5 seconds as fallback
    setInterval(fetchInitialData, 5000);
});
