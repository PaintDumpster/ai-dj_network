# AI DJ Web Application

Real-time web visualization interface for the AI DJ audio classification system with integrated Barcelona district sound playback.

## 🎨 Overview

A futuristic dark-themed web application that provides real-time visualization of:
- Live audio waveform (42×146 LED matrix)
- Button press indicators (1-10) with visual feedback
- **Barcelona district audio playback** (HTML5 Audio)
- State machine transitions
- Audio classification results from 3 YAMNet models

## ✨ Features

### Visual
- **Real-time waveform display**: 42×146 pixel LED matrix canvas
- **Button indicators**: Visual feedback when buttons 1-10 are pressed
- **State transitions**: Smooth animated transitions between screens
- **Futuristic design**: Dark neon theme with glitch effects
- **Responsive layout**: Works on desktop and mobile

### Audio **NEW** ✅
- **Barcelona sound playback**: Plays district sounds in browser when buttons pressed
- **Random selection**: Each button randomly selects from 5 district sounds
- **HTML5 Audio API**: No plugins required, works in all modern browsers
- **Auto-enable**: User click activates audio (browser autoplay policy)
- **Volume control**: Set to 70% by default
- **Debug logging**: Emoji-marked console logs for troubleshooting

### Data
- **WebSocket connection**: Real-time updates from ROS2 system
- **Sound mapping API**: Fetches available sounds from `/api/sounds`
- **LED matrix data**: Receives 6,132-byte updates at 10 Hz
- **Button events**: Instant notification of hardware button presses
- **State synchronization**: Backend state machine controls UI flow

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

3. **Enable audio**: Click anywhere on the page to enable sound playback (browser requirement)

4. **Start recording**: Press button 11 (or click welcome screen)

5. **Press buttons 1-10**: Hear Barcelona district sounds and see waveform build!

### Development

The webapp is served directly by the FastAPI server in `web_bridge.py`. No build process required - just edit and refresh!

**Note**: After editing JavaScript files, do a hard refresh in browser:
- **Windows/Linux**: Ctrl+Shift+R or Ctrl+F5
- **Mac**: Cmd+Shift+R

## 🎵 Barcelona District Sounds

The webapp plays authentic Barcelona district sounds when buttons are pressed:

| Button | District | Example Sounds |
|--------|----------|----------------|
| 1 | Ciutat Vella | Castellers, Sardanes, Gothic Quarter |
| 2 | Eixample | Sant Jordi, Modernist tours |
| 3 | Sants-Montjuïc | Montjuïc parks, festivals |
| 4 | Les Corts | Camp Nou, university area |
| 5 | Sarrià-St Gervasi | Upscale areas, parks |
| 6 | Gràcia | Festa Major, bohemian streets |
| 7 | Horta-Guinardó | Parks, residential areas |
| 8 | Nou Barris | Working-class neighborhoods |
| 9 | Sant Andreu | Traditional markets |
| 10 | Sant Martí | Beach, 22@ tech district |

Each button has **5 different sounds** that are randomly selected on each press.

## 🔧 Audio System Details

### How It Works

1. **Page Load**: 
   - Webapp fetches sound mappings from `/api/sounds` endpoint
   - Sound paths are stored in `soundsMapping` object
   - Console shows: `✅ Sound mappings loaded: 10 buttons`

2. **User Interaction**:
   - First click enables audio playback (browser requirement)
   - Console shows: `🔊 Audio enabled by user interaction`

3. **Button Press** (during recording):
   - Hardware button press → ROS2 → WebSocket → Browser
   - `playRandomSound(buttonNumber)` function called
   - Random sound selected from button's district
   - HTML5 Audio element created and played
   - Console shows:
     ```
     🔘 Arduino button press: {button: "1"}
     🎵 playRandomSound called for button 1
     🎧 Playing: /sounds/1.%20Ciutat%20Vella/castellers.wav
     ✅ Audio loaded successfully
     ▶️ Audio playing
     ```

### Browser Compatibility

- ✅ Chrome/Edge (recommended)
- ✅ Firefox
- ✅ Safari
- ✅ Mobile browsers

### Troubleshooting

**No sound playing?**
1. Open browser console (F12)
2. Look for error messages starting with ❌
3. Check if you clicked the page to enable audio (look for 🔊 message)
4. Verify sound files exist: `curl http://localhost:8000/api/sounds`
5. Test manual playback: Click anywhere, watch console logs

**Console shows "Audio might be blocked"?**
- Click anywhere on the page before/during recording
- Browser autoplay policy requires user gesture

**Sound files not found (404)?**
- Check `sounds/` directory has WAV files
- Restart `web_bridge` node
- Verify `/api/sounds` returns file paths

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

## � API Endpoints

### REST API

**GET `/api/state`** - Get current system state
```json
{
  "state": "recording",
  "recording_active": true
}
```

**GET `/api/sounds`** - Get available sound files per button
```json
{
  "sounds": {
    "1": [
      "/sounds/1.%20Ciutat%20Vella/castellers-sant-jaume.wav",
      "/sounds/1.%20Ciutat%20Vella/sardanes-catedral.wav",
      ...
    ],
    "2": [...],
    ...
  }
}
```

**GET `/api/arduino/latest`** - Get recent button presses
```json
{
  "data": [
    {"type": "arduino", "button": "1", "timestamp": 123456},
    ...
  ]
}
```

**GET `/api/classifications`** - Get all classification results
```json
{
  "model1": ["Class A: 0.95", "Class B: 0.82", ...],
  "model2": [...],
  "model3": [...]
}
```

**Static Files**:
- `GET /sounds/{district}/{filename.wav}` - Direct audio file access
- `GET /`, `/css/*`, `/js/*`, `/assets/*` - Webapp files

## �🔌 WebSocket API

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
