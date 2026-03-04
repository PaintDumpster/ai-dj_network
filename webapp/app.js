// Application State Management
let currentState = 'welcome';
let ws = null;
let reconnectInterval = null;
let recordingStartTime = null;
let recordingTimer = null;
let buttonPresses = [];

// LED Matrix Configuration
const MATRIX_WIDTH = 146; // 30 seconds
const MATRIX_HEIGHT = 42;  // amplitude

// Initialize on page load
document.addEventListener('DOMContentLoaded', () => {
    connectWebSocket();
    initializeLEDCanvas();
});

// WebSocket Connection
function connectWebSocket() {
    const wsUrl = 'ws://localhost:8000/ws';
    
    try {
        ws = new WebSocket(wsUrl);
        
        ws.onopen = () => {
            console.log('WebSocket connected');
            updateConnectionStatus(true);
            
            if (reconnectInterval) {
                clearInterval(reconnectInterval);
                reconnectInterval = null;
            }
            
            // Request current state
            fetch('/api/state')
                .then(r => r.json())
                .then(data => changeState(data.state));
        };
        
        ws.onmessage = (event) => {
            try {
                const data = JSON.parse(event.data);
                handleMessage(data);
            } catch (e) {
                console.log('Message:', event.data);
            }
        };
        
        ws.onerror = (error) => {
            console.error('WebSocket error:', error);
        };
        
        ws.onclose = () => {
            console.log('WebSocket disconnected');
            updateConnectionStatus(false);
            
            if (!reconnectInterval) {
                reconnectInterval = setInterval(connectWebSocket, 3000);
            }
        };
    } catch (error) {
        console.error('Failed to connect:', error);
    }
}

// Handle incoming messages
function handleMessage(data) {
    console.log('Received:', data);
    
    if (data.type === 'state_change') {
        handleStateChange(data);
    } else if (data.type === 'arduino') {
        handleArduinoButtonPress(data);
    } else if (data.type === 'led_matrix') {
        handleLEDMatrixUpdate(data);
    } else if (data.type === 'classification') {
        handleClassificationResult(data);
    }
}

// State Management
function handleStateChange(data) {
    console.log('State change:', data.state);
    changeState(data.state);
}

function changeState(newState) {
    currentState = newState;
    
    // Hide all screens
    document.querySelectorAll('.screen').forEach(s => s.classList.remove('active'));
    
    // Show appropriate screen
    switch (newState) {
        case 'welcome':
            document.getElementById('screen-welcome').classList.add('active');
            break;
        case 'ready':
            document.getElementById('screen-ready').classList.add('active');
            break;
        case 'countdown':
            document.getElementById('screen-countdown').classList.add('active');
            startCountdown();
            break;
        case 'recording':
            document.getElementById('screen-recording').classList.add('active');
            startRecordingDisplay();
            break;
        case 'recording_complete':
            document.getElementById('screen-complete').classList.add('active');
            document.getElementById('press-count').textContent = buttonPresses.length;
            break;
        case 'analyzing':
            document.getElementById('screen-analyzing').classList.add('active');
            break;
        case 'results':
            document.getElementById('screen-results').classList.add('active');
            break;
    }
}

// Countdown (3, 2, 1)
function startCountdown() {
    let count = 3;
    const display = document.getElementById('countdown-display');
    display.textContent = count;
    
    const interval = setInterval(() => {
        count--;
        if (count > 0) {
            display.textContent = count;
        } else {
            clearInterval(interval);
            display.textContent = 'GO!';
        }
    }, 1000);
}

// Recording Display
function startRecordingDisplay() {
    recordingStartTime = Date.now();
    buttonPresses = [];
    clearLEDMatrix();
    
    // Update timer every 100ms
    recordingTimer = setInterval(() => {
        const elapsed = Math.floor((Date.now() - recordingStartTime) / 1000);
        const minutes = Math.floor(elapsed / 60);
        const seconds = elapsed % 60;
        document.getElementById('recording-timer').textContent = 
            `${minutes.toString().padStart(2, '0')}:${seconds.toString().padStart(2, '0')}`;
        
        // Stop at 30 seconds
        if (elapsed >= 30) {
            clearInterval(recordingTimer);
        }
    }, 100);
}

// Arduino Button Press Handler
function handleArduinoButtonPress(data) {
    if (currentState !== 'recording') return;
    
    const buttonNum = data.button.trim();
    buttonPresses.push({
        button: buttonNum,
        time: (Date.now() - recordingStartTime) / 1000
    });
    
    // Highlight button
    const btnElement = document.querySelector(`.btn-indicator[data-btn="${buttonNum}"]`);
    if (btnElement) {
        btnElement.classList.add('active');
        setTimeout(() => btnElement.classList.remove('active'), 300);
    }
}

// LED Matrix Visualization
let ledCanvas = null;
let ledCtx = null;
let ledMatrix = null;

function initializeLEDCanvas() {
    ledCanvas = document.getElementById('led-matrix-canvas');
    if (!ledCanvas) return;
    
    ledCtx = ledCanvas.getContext('2d');
    
    // Set canvas size
    ledCanvas.width = MATRIX_WIDTH * 4;  // Scale up for visibility
    ledCanvas.height = MATRIX_HEIGHT * 4;
    
    // Initialize matrix data
    ledMatrix = new Array(MATRIX_HEIGHT).fill(null).map(() => 
        new Array(MATRIX_WIDTH).fill(null).map(() => ({ r: 0, g: 0, b: 0 }))
    );
    
    clearLEDMatrix();
}

function clearLEDMatrix() {
    if (!ledCtx) return;
    
    ledCtx.fillStyle = '#0a0a0a';
    ledCtx.fillRect(0, 0, ledCanvas.width, ledCanvas.height);
    
    // Reset matrix data
    if (ledMatrix) {
        for (let i = 0; i < MATRIX_HEIGHT; i++) {
            for (let j = 0; j < MATRIX_WIDTH; j++) {
                ledMatrix[i][j] = { r: 0, g: 0, b: 0 };
            }
        }
    }
}

function handleLEDMatrixUpdate(data) {
    if (!ledCtx) return;
    
    // Parse LED matrix data and update display
    // Data format would come from ROS2 node
    // For now, this is a placeholder
    console.log('LED matrix update received');
}

function updateLEDMatrix(matrixData) {
    if (!ledCtx) return;
    
    const pixelWidth = ledCanvas.width / MATRIX_WIDTH;
    const pixelHeight = ledCanvas.height / MATRIX_HEIGHT;
    
    for (let row = 0; row < MATRIX_HEIGHT; row++) {
        for (let col = 0; col < MATRIX_WIDTH; col++) {
            const pixel = matrixData[row][col];
            const x = col * pixelWidth;
            const y = row * pixelHeight;
            
            ledCtx.fillStyle = `rgb(${pixel.r}, ${pixel.g}, ${pixel.b})`;
            ledCtx.fillRect(x, y, pixelWidth, pixelHeight);
        }
    }
}

// Classification Results Handler
function handleClassificationResult(data) {
    const modelId = `results-${data.model}`;
    const container = document.getElementById(modelId);
    
    if (!container) return;
    
    // Parse classification results
    // Assuming data.results is a string with format:
    // "1. Class Name: score\n2. Class Name: score\n..."
    const lines = data.results.split('\n').filter(l => l.trim());
    
    container.innerHTML = '';
    lines.forEach(line => {
        const resultItem = document.createElement('div');
        resultItem.className = 'result-item';
        resultItem.textContent = line;
        container.appendChild(resultItem);
    });
    
    // If this is the last model result, change to results state
    // (In practice, you'd track which models have reported)
}

// Connection Status
function updateConnectionStatus(connected) {
    const statusDot = document.getElementById('connection-status');
    const statusText = document.getElementById('connection-text');
    
    if (connected) {
        statusDot.className = 'status-dot connected';
        if (statusText) statusText.textContent = 'Connected';
    } else {
        statusDot.className = 'status-dot disconnected';
        if (statusText) statusText.textContent = 'Disconnected';
    }
}

// Restart Application
function restartApp() {
    buttonPresses = [];
    clearLEDMatrix();
    changeState('welcome');
}

// Export for global access
window.restartApp = restartApp;
