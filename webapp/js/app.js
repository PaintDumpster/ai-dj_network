// =====================================================
// APPLICATION STATE MANAGEMENT
// =====================================================
let currentState = 'welcome';
let ws = null;
let reconnectInterval = null;
let recordingStartTime = null;
let recordingTimer = null;
let buttonPresses = [];

// LED Matrix Configuration
const MATRIX_WIDTH = 146; // 30 seconds
const MATRIX_HEIGHT = 42;  // amplitude

// Spectrum Visualization
let spectrumAnimationId = null;
let spectrumCanvas = null;
let spectrumCtx = null;
let spectrumBarsCanvas = null;
let spectrumBarsCtx = null;
let mapCanvas = null;
let mapCtx = null;
let spectrumBars = new Array(64).fill(0);

// =====================================================
// INITIALIZATION
// =====================================================
document.addEventListener('DOMContentLoaded', () => {
    connectWebSocket();
    initializeSpectrumBackgrounds();
    initializeLEDCanvas();
});

// =====================================================
// SPECTRUM BACKGROUND VISUALIZATION
// =====================================================
function initializeSpectrumBackgrounds() {
    // Main spectrum background
    spectrumCanvas = document.getElementById('spectrum-canvas');
    if (spectrumCanvas) {
        spectrumCtx = spectrumCanvas.getContext('2d');
        spectrumCanvas.width = spectrumCanvas.offsetWidth;
        spectrumCanvas.height = spectrumCanvas.offsetHeight;
        animateSpectrumBackground();
    }
    
    // Recording spectrum bars
    spectrumBarsCanvas = document.getElementById('spectrum-bars-canvas');
    if (spectrumBarsCanvas) {
        spectrumBarsCtx = spectrumBarsCanvas.getContext('2d');
        spectrumBarsCanvas.width = spectrumBarsCanvas.offsetWidth;
        spectrumBarsCanvas.height = spectrumBarsCanvas.offsetHeight;
    }
    
    // Results map canvas
    mapCanvas = document.getElementById('map-canvas');
    if (mapCanvas) {
        mapCtx = mapCanvas.getContext('2d');
        mapCanvas.width = mapCanvas.offsetWidth;
        mapCanvas.height = mapCanvas.offsetHeight;
    }
}

function animateSpectrumBackground() {
    if (!spectrumCtx) return;
    
    const width = spectrumCanvas.width;
    const height = spectrumCanvas.height;
    const barCount = 64;
    const barWidth = width / barCount;
    
    // Generate random spectrum bars
    for (let i = 0; i < barCount; i++) {
        spectrumBars[i] = Math.random() * Math.random() * height * 0.7;
    }
    
    // Clear canvas
    spectrumCtx.fillStyle = 'rgba(0, 0, 0, 0.1)';
    spectrumCtx.fillRect(0, 0, width, height);
    
    // Draw spectrum with gradient
    for (let i = 0; i < barCount; i++) {
        const x = i * barWidth;
        const barHeight = spectrumBars[i];
        
        // Create gradient
        const gradient = spectrumCtx.createLinearGradient(0, height - barHeight, 0, height);
        gradient.addColorStop(0, `rgba(255, 0, 0, 0.8)`);
        gradient.addColorStop(0.5, `rgba(255, 0, 0, 0.4)`);
        gradient.addColorStop(1, `rgba(255, 255, 255, 0.2)`);
        
        spectrumCtx.fillStyle = gradient;
        spectrumCtx.fillRect(x + 1, height - barHeight, barWidth - 2, barHeight);
    }
    
    spectrumAnimationId = requestAnimationFrame(animateSpectrumBackground);
}

function drawRecordingSpectrum() {
    if (!spectrumBarsCtx) return;
    
    const width = spectrumBarsCanvas.width;
    const height = spectrumBarsCanvas.height;
    const barCount = 32;
    const barWidth = width / barCount;
    
    // Smooth animation of bars (more "realistic")
    for (let i = 0; i < barCount; i++) {
        // Simulate audio spectrum with smooth variation
        spectrumBars[i] = spectrumBars[i] * 0.85 + Math.random() * Math.random() * height * 0.5;
    }
    
    // Clear with fade effect
    spectrumBarsCtx.fillStyle = 'rgba(0, 0, 0, 0.2)';
    spectrumBarsCtx.fillRect(0, 0, width, height);
    
    // Draw bars with red/white gradient
    for (let i = 0; i < barCount; i++) {
        const x = i * barWidth;
        const barHeight = spectrumBars[i];
        
        // Create gradient from red to white
        const gradient = spectrumBarsCtx.createLinearGradient(0, height - barHeight, 0, height);
        gradient.addColorStop(0, 'rgba(255, 0, 0, 0.9)');
        gradient.addColorStop(0.6, 'rgba(255, 255, 255, 0.7)');
        gradient.addColorStop(1, 'rgba(255, 255, 255, 0.3)');
        
        spectrumBarsCtx.fillStyle = gradient;
        spectrumBarsCtx.fillRect(x + 1, height - barHeight, barWidth - 2, barHeight);
        
        // Add glow effect
        spectrumBarsCtx.strokeStyle = `rgba(255, 0, 0, ${0.5 * (barHeight / height)})`;
        spectrumBarsCtx.lineWidth = 2;
        spectrumBarsCtx.strokeRect(x + 1, height - barHeight, barWidth - 2, barHeight);
    }
}

function drawMapVisualization() {
    if (!mapCtx) return;
    
    const width = mapCanvas.width;
    const height = mapCanvas.height;
    
    // Draw background gradient (Cyan to Orange)
    const bgGradient = mapCtx.createLinearGradient(0, 0, width, height);
    bgGradient.addColorStop(0, 'rgba(0, 255, 255, 0.1)');
    bgGradient.addColorStop(1, 'rgba(255, 102, 0, 0.1)');
    
    mapCtx.fillStyle = bgGradient;
    mapCtx.fillRect(0, 0, width, height);
    
    // Draw frequency nodes based on button presses
    mapCtx.save();
    mapCtx.translate(width / 2, height / 2);
    
    // Draw concentric circles
    for (let r = 0; r < 3; r++) {
        const radius = (r + 1) * (Math.min(width, height) / 8);
        mapCtx.strokeStyle = `rgba(0, 255, 255, ${0.4 - r * 0.1})`;
        mapCtx.lineWidth = 1;
        mapCtx.beginPath();
        mapCtx.arc(0, 0, radius, 0, Math.PI * 2);
        mapCtx.stroke();
    }
    
    // Plot button press locations
    if (buttonPresses.length > 0) {
        buttonPresses.forEach((press, idx) => {
            const angle = (parseInt(press.button) - 1) * (Math.PI * 2 / 10);
            const dist = 60 * (press.time / 30); // Normalized by 30 second duration
            const x = Math.cos(angle) * dist;
            const y = Math.sin(angle) * dist;
            
            // Determine color by button number
            const hue = (parseInt(press.button) - 1) * 36; // 360 / 10
            const color = `hsl(${hue}, 100%, 50%)`;
            
            // Draw point
            mapCtx.fillStyle = color;
            mapCtx.beginPath();
            mapCtx.arc(x, y, 4, 0, Math.PI * 2);
            mapCtx.fill();
            
            // Add glow
            mapCtx.strokeStyle = color;
            mapCtx.lineWidth = 2;
            mapCtx.globalAlpha = 0.5;
            mapCtx.stroke();
            mapCtx.globalAlpha = 1;
            
            // Draw connecting line from center
            mapCtx.strokeStyle = `rgba(0, 255, 255, 0.2)`;
            mapCtx.lineWidth = 1;
            mapCtx.beginPath();
            mapCtx.moveTo(0, 0);
            mapCtx.lineTo(x, y);
            mapCtx.stroke();
        });
    }
    
    mapCtx.restore();
}

// =====================================================
// WEBSOCKET CONNECTION
// =====================================================
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
                .then(data => {
                    console.log('Initial state from server:', data);
                    changeState(data.state);
                })
                .catch(err => console.error('Failed to fetch initial state:', err));
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
    console.log(`Changing state from '${currentState}' to '${newState}'`);
    currentState = newState;
    
    // Hide all screens
    document.querySelectorAll('.screen').forEach(s => s.classList.remove('active'));
    
    // Show appropriate screen
    switch (newState) {
        case 'welcome':
            document.getElementById('screen-welcome').classList.add('active');
            console.log('Displaying welcome screen');
            break;
        case 'ready':
            // Keep showing welcome screen - user just pressed button, waiting for ROS2 response
            document.getElementById('screen-welcome').classList.add('active');
            console.log('State is ready, showing welcome screen');
            break;
        case 'countdown':
            document.getElementById('screen-countdown').classList.add('active');
            startCountdown();
            console.log('Starting countdown');
            break;
        case 'recording':
            document.getElementById('screen-recording').classList.add('active');
            startRecordingDisplay();
            console.log('Starting recording display');
            break;
        case 'recording_complete':
            document.getElementById('screen-complete').classList.add('active');
            document.getElementById('press-count').textContent = buttonPresses.length;
            console.log('Recording complete, showing summary');
            break;
        case 'analyzing':
            document.getElementById('screen-analyzing').classList.add('active');
            console.log('Analyzing...');
            break;
        case 'results':
            document.getElementById('screen-results').classList.add('active');
            drawMapVisualization();
            console.log('Displaying results');
            break;
        default:
            console.error(`Unknown state: ${newState}`);
            document.getElementById('screen-welcome').classList.add('active');
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
    
    // Initialize LED canvas if not already done
    if (!ledCanvas) {
        initializeLEDCanvas();
    } else {
        clearLEDMatrix();
    }
    
    // Update timer every 1 second
    recordingTimer = setInterval(() => {
        const elapsed = Math.floor((Date.now() - recordingStartTime) / 1000);
        const minutes = Math.floor(elapsed / 60);
        const seconds = elapsed % 60;
        document.getElementById('recording-timer').textContent = 
            `${minutes.toString().padStart(2, '0')}:${seconds.toString().padStart(2, '0')}`;
        
        // Spectrum animation removed - it was causing jitter and visual bugs
        
        // Stop at 30 seconds
        if (elapsed >= 30) {
            clearInterval(recordingTimer);
        }
    }, 1000);
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
    
    // Parse LED matrix data from ROS2 (flattened UInt8Array)
    // Data format: { width, height, data: [flattenedArray] }
    const width = data.width || MATRIX_WIDTH;
    const height = data.height || MATRIX_HEIGHT;
    const matrixData = data.data;
    
    if (!matrixData || matrixData.length !== width * height) {
        console.log('Invalid matrix data received');
        return;
    }
    
    console.log(`LED matrix update received: ${width}x${height}`);
    
    // Update the display
    drawLEDMatrixWaveform(matrixData, width, height);
}

function drawLEDMatrixWaveform(matrixData, width, height) {
    if (!ledCtx) return;
    
    const pixelWidth = ledCanvas.width / width;
    const pixelHeight = ledCanvas.height / height;
    
    // Clear canvas
    ledCtx.fillStyle = '#0a0a0a';
    ledCtx.fillRect(0, 0, ledCanvas.width, ledCanvas.height);
    
    // Draw waveform pixels
    for (let row = 0; row < height; row++) {
        for (let col = 0; col < width; col++) {
            const index = row * width + col;
            const value = matrixData[index];
            
            if (value > 0) {
                const x = col * pixelWidth;
                const y = row * pixelHeight;
                
                // White grayscale for waveform
                ledCtx.fillStyle = `rgb(${value}, ${value}, ${value})`;
                ledCtx.fillRect(x, y, pixelWidth, pixelHeight);
                
                // Add slight glow for active pixels
                if (value > 200) {
                    ledCtx.shadowColor = `rgba(255, 255, 255, 0.5)`;
                    ledCtx.shadowBlur = 2;
                    ledCtx.fillRect(x, y, pixelWidth, pixelHeight);
                    ledCtx.shadowBlur = 0;
                }
            }
        }
    }
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
