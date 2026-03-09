# Barcelona District Sounds

This folder contains audio files for the 10 Barcelona districts. Each button corresponds to a district, and 5 sound files are assigned to each button for variety. When a button is pressed during recording, a random sound from that district plays both in the waveform and in the web browser.

## 🎵 Current Status

- ✅ **50 WAV files** converted and ready (5 per district)
- ✅ **Format**: 16kHz mono WAV (YAMNet compatible)
- ✅ **Integration**: build_waveform node loads and plays sounds
- ✅ **Web playback**: Browser plays sounds via HTML5 Audio API
- ✅ **Random selection**: Each button press selects random district sound

## Directory Structure

```
sounds/
├── 1. Ciutat Vella/          # Button 1 sounds (5 files)
│   ├── castellers-sant-jaume.wav
│   ├── sardanes-catedral.wav
│   ├── ciutat vella.wav
│   ├── barceloneta.wav
│   └── Correfocs gòtic.wav
├── 2. Eixample/              # Button 2 sounds (5 files)
├── 3. Sants-Montjuic/        # Button 3 sounds (5 files)
├── 4. Les Corts/             # Button 4 sounds (5 files)
├── 5. Sarrià-St Gervasi/     # Button 5 sounds (5 files)
├── 6. Gràcia/                # Button 6 sounds (5 files)
├── 7. Horta-Guinardó/        # Button 7 sounds (5 files)
├── 8. Nou Barris/            # Button 8 sounds (5 files)
├── 9. Sant Andreu/           # Button 9 sounds (5 files)
├── 10. Sant Martí/           # Button 10 sounds (5 files)
├── convert_audio.py          # Audio conversion utility
└── .venv/                    # Python virtual environment (for conversion)
```

## Audio Format Requirements

For YAMNet classification, audio files must be:
- **Format**: WAV (PCM 16-bit)
- **Sample rate**: 16kHz
- **Channels**: Mono

## Converting Audio Files

### Prerequisites

Install required dependencies:

```bash
# System dependencies
sudo apt install ffmpeg libsndfile1-dev

# Create virtual environment and install Python dependencies
cd sounds/
python3 -m venv .venv
source .venv/bin/activate
pip install librosa soundfile numpy
```

### Usage

1. **Place original audio files** (.mp3, .m4a, etc.) in the district folders

2. **Preview conversion** (dry run):
   ```bash
   python3 convert_audio.py --dry-run
   ```

3. **Convert all files**:
   ```bash
   python3 convert_audio.py
   ```

The script will:
- Scan subdirectories 1-10 for audio files
- Convert to 16kHz mono WAV format
- Skip already-converted files
- Report success/failure for each file

### Conversion Output Example

```
======================================================================
Audio Conversion for YAMNet Classification
======================================================================
Target format: 16kHz, mono, WAV

Processing: 1. Ciutat Vella
  castellers.mp3 → castellers.wav ✓
  barceloneta.m4a → barceloneta.wav ✓
  
Processing: 2. Eixample
  sant-jordi.mp3 → sant-jordi.wav ✓
  
...

Summary: 50 files found, 33 converted, 17 failed
```

## Integration with ROS2 System

### C++ Node (build_waveform)

The `build_waveform` node automatically:
1. Scans `sounds/` subdirectories at startup
2. Loads available WAV files per button (1-10)
3. On button press, randomly selects a sound file
4. Loads WAV data using libsndfile
5. Adds audio samples to the waveform matrix

### Web Browser (webapp)

The webapp automatically:
1. Fetches sound mappings from `/api/sounds` endpoint
2. Receives button press events via WebSocket
3. Plays corresponding district sound using HTML5 Audio
4. Requires user interaction to enable audio (browser policy)

## Adding New Sounds

1. **Add audio file** to appropriate district folder (any format)
2. **Run conversion script**:
   ```bash
   cd sounds/
   python3 convert_audio.py
   ```
3. **Restart build_waveform** node to reload sound mappings
4. **Restart web_bridge** to update `/api/sounds` endpoint

No code changes required - system automatically detects new files!

## Barcelona Districts (Button Mapping)

- **Button 1**: Ciutat Vella (Gothic Quarter, La Rambla, Barceloneta)
- **Button 2**: Eixample (Sagrada Família, Casa Batlló, modernist architecture)
- **Button 3**: Sants-Montjuïc (Montjuïc hill, parks, cultural venues)
- **Button 4**: Les Corts (Camp Nou, university area)
- **Button 5**: Sarrià-Sant Gervasi (upscale residential, parks)
- **Button 6**: Gràcia (bohemian neighborhood, Festa Major)
- **Button 7**: Horta-Guinardó (Parc del Laberint, residential)
- **Button 8**: Nou Barris (working-class area, parks)
- **Button 9**: Sant Andreu (traditional neighborhood, markets)
- **Button 10**: Sant Martí (beach area, tech hub, 22@)
   python3 convert_audio.py --dry-run
   ```

2. **Convert all files**:
   ```bash
   python3 convert_audio.py
   ```
   This will convert all .mp3, .m4a, and other audio files to .wav format (16kHz, mono).

3. **Re-convert existing files**:
   ```bash
   python3 convert_audio.py --overwrite
   ```

### What the Script Does

- Scans all district folders (1-10)
- Finds audio files (.mp3, .m4a, .wav, etc.)
- Converts to YAMNet-compatible WAV format:
  - Resamples to 16kHz sample rate
  - Converts to mono channel
  - Saves as WAV PCM 16-bit
- Preserves original files (creates .wav versions)

## How It Works in ROS2

When a button is pressed during recording:
1. The system looks for the corresponding district folder
2. **Randomly selects** one .wav file from that folder
3. Loads and mixes the audio into the current waveform
4. Updates the LED matrix visualization

**Benefits of multiple sounds per button:**
- Adds variety to compositions
- Each recording session sounds unique
- Represents the diversity of each Barcelona district

## Missing Sound Files

If a button's folder has no .wav files, the system will generate a placeholder sine wave at a frequency corresponding to the button number (220-880 Hz range).
