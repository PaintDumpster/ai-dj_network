# AI DJ Web Application

Real-time web visualization interface for the AI DJ audio classification system.

## 🎨 Overview

A futuristic dark-themed web application that provides real-time visualization of:
- Live audio waveform (42×146 LED matrix)
- Button press indicators (1-10)
- State machine transitions
- Audio classification results from 3 YAMNet models

## 📁 Project Structure

```
webapp/
├── index.html           # Main HTML entry point
├── css/
│   └── style.css       # Futuristic neon-themed styles
├── js/
│   └── app.js          # WebSocket client & visualization logic
├── assets/
│   ├── images/         # Image assets (logos, icons)
│   ├── fonts/          # Custom fonts
│   └── .gitkeep
├── docs/
│   └── .gitkeep        # Documentation and design specs
├── package.json        # Node.js dependencies (future)
└── README.md           # This file
```

## 🚀 Quick Start

### Access the Webapp

1. **Start the ROS2 web bridge**:
   ```bash
   cd ~/iaac/ai4all/rosnetwork
   source install/setup.bash
   ros2 run py_pkg web_bridge
   ```

2. **Open in browser**:
   ```
   http://localhost:8000
   ```

### Development

The webapp is served directly by the FastAPI server in `web_bridge.py`. No build process required - just edit and refresh!

## 🎯 Features

### State Machine UI
- **Welcome Screen**: Connection status, press-to-start prompt
- **Countdown**: 3-2-1-GO animation before recording
- **Recording Screen**: Live waveform + timer + button indicators
- **Recording Complete**: Summary of button presses
- **Analyzing**: Glitch effect animation during classification
- **Results**: Three-model classification display with audio map

### Real-Time Updates
- **WebSocket Connection**: Bidirectional communication with ROS2
- **10 Hz Updates**: LED matrix data streamed at 10 frames per second
- **Button Feedback**: Visual highlights when buttons 1-10 are pressed
- **State Synchronization**: Frontend follows backend state machine

### Visualization
- **LED Matrix Canvas**: 42×146 pixel waveform (scaled 4x for visibility)
- **Spectrum Analyzer**: Background animation on welcome screen
- **Button Grid**: 10 button indicators with active states
- **Neon Effects**: Glowing borders, shadows, and text effects

## 🔌 WebSocket API

### Received Messages

**State Changes**:
```json
{
  "type": "state_change",
  "state": "recording",
  "timestamp": 1772971804
}
```

**LED Matrix Updates**:
```json
{
  "type": "led_matrix",
  "width": 146,
  "height": 42,
  "data": [0, 0, 255, ...],
  "timestamp": 1772971804
}
```

**Arduino Button Presses**:
```json
{
  "type": "arduino",
  "button": "5",
  "timestamp": 1772971805
}
```

**Classification Results**:
```json
{
  "type": "classification",
  "model": "model1",
  "results": "1. Music: 0.95\n2. Piano: 0.87\n..."
}
```

### REST API Endpoints

- **GET /api/state** - Current system state
- **GET /api/classifications** - All model results
- **GET /api/classifications/{model_name}** - Specific model results

## 🎨 Design System

### Color Palette
- **Background**: `#0a0a0a` (near black)
- **Primary Text**: `#e0e0e0` (light gray)
- **Neon Cyan**: `#00ffff` (primary accent)
- **Neon Red**: `#ff0040` (alerts, recording indicator)
- **Neon Yellow**: `#ffff00` (highlights)

### Typography
- **Primary Font**: `Orbitron` (futuristic, geometric)
- **Monospace**: `Courier New` (data display, timers)

### Effects
- **Glow**: `box-shadow: 0 0 20px rgba(0, 255, 255, 0.6)`
- **Glitch**: CSS animation with `clip-path` for analyzing screen
- **Pulse**: Keyframe animation for "awaiting input" text

## 🛠️ Development Guidelines

### Code Style
- **JavaScript**: ES6+ features, async/await for WebSocket
- **CSS**: BEM-like naming for clarity
- **HTML**: Semantic elements, accessibility attributes

### Performance
- **Canvas Rendering**: RAF (requestAnimationFrame) for smooth 60fps animations
- **WebSocket Throttling**: Backend sends at 10Hz to avoid flooding
- **State Management**: Single `currentState` variable, functional state transitions

### Browser Compatibility
- **Target**: Modern browsers (Chrome 90+, Firefox 88+, Safari 14+)
- **WebSocket Support**: Required (all modern browsers)
- **Canvas 2D API**: Required for visualizations

## 📝 Known Issues

See [main README.md](../README.md#known-issues) for waveform visualization issues.

### Workarounds
- Waveform appears sparse due to placeholder sine wave design
- Issue is cosmetic only - system functionality not affected
- High-resolution waveform data saved correctly in CSV files

## 🔮 Future Enhancements

### Planned Features
- [ ] Waveform thickness/intensity improvements
- [ ] Real-time audio level meters
- [ ] Button press timeline overlay
- [ ] Classification confidence graphs
- [ ] Export results as JSON/PDF
- [ ] Dark/light theme toggle
- [ ] Keyboard shortcuts (spacebar = button 11)

### Build System (Future)
```json
{
  "scripts": {
    "dev": "vite serve",
    "build": "vite build",
    "preview": "vite preview"
  }
}
```

Currently served directly by FastAPI - no build process required!

## 📚 Resources

- **FastAPI WebSocket**: https://fastapi.tiangolo.com/advanced/websockets/
- **Canvas API**: https://developer.mozilla.org/en-US/docs/Web/API/Canvas_API
- **WebSocket API**: https://developer.mozilla.org/en-US/docs/Web/API/WebSocket

## 📄 License

Part of the AI DJ project. See [main README](../README.md) for license information.
