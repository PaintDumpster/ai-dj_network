#!/usr/bin/env python3
"""
Audio Conversion Utility for YAMNet Classification

This script converts audio files (.mp3, .m4a, .wav) to the format required by YAMNet:
- Sample rate: 16kHz (YAMNet requirement)
- Channels: Mono
- Format: WAV PCM 16-bit

Usage:
    python3 convert_audio.py [--dry-run] [--overwrite]

Options:
    --dry-run     Show what would be converted without actually converting
    --overwrite   Overwrite existing .wav files

Required libraries:
    pip install pydub librosa soundfile numpy
    
System dependencies (Ubuntu/Debian):
    sudo apt install ffmpeg

Directory structure:
    sounds/
    ├── 1. Ciutat Vella/
    │   ├── file1.mp3 → file1.wav (16kHz, mono)
    │   └── file2.mp3 → file2.wav (16kHz, mono)
    └── ...
"""

import os
import sys
import argparse
import glob
from pathlib import Path

try:
    import librosa
    import soundfile as sf
    import numpy as np
    from pydub import AudioSegment
    PYDUB_AVAILABLE = True
except ImportError as e:
    PYDUB_AVAILABLE = False
    if 'pydub' not in str(e):
        print("ERROR: Missing required libraries!")
        print("\nPlease install dependencies:")
        print("  pip install pydub librosa soundfile numpy")
        print("\nSystem dependencies (Ubuntu/Debian):")
        print("  sudo apt install ffmpeg")
        sys.exit(1)

# YAMNet requirements
TARGET_SAMPLE_RATE = 16000  # 16kHz
TARGET_CHANNELS = 1  # Mono


def convert_audio_file(input_path, output_path, overwrite=False):
    """
    Convert audio file to YAMNet-compatible WAV format.
    
    Args:
        input_path (str): Path to input audio file
        output_path (str): Path to output WAV file
        overwrite (bool): Whether to overwrite existing files
    
    Returns:
        bool: True if conversion successful, False otherwise
    """
    if os.path.exists(output_path) and not overwrite:
        print(f"  ⏭️  SKIP: {os.path.basename(output_path)} (already exists)")
        return True
    
    # Try librosa first (more efficient)
    try:
        audio_data, sample_rate = librosa.load(
            input_path,
            sr=TARGET_SAMPLE_RATE,
            mono=True
        )
        
        sf.write(output_path, audio_data, TARGET_SAMPLE_RATE, subtype='PCM_16')
        
        duration = len(audio_data) / TARGET_SAMPLE_RATE
        print(f"  ✅ {os.path.basename(input_path)} → {os.path.basename(output_path)} ({duration:.2f}s)")
        return True
        
    except Exception as e:
        # If librosa fails, try pydub as fallback (better for .m4a)
        if PYDUB_AVAILABLE:
            try:
                # Load with pydub
                audio = AudioSegment.from_file(input_path)
                
                # Convert to mono
                if audio.channels > 1:
                    audio = audio.set_channels(1)
                
                # Resample to target rate
                if audio.frame_rate != TARGET_SAMPLE_RATE:
                    audio = audio.set_frame_rate(TARGET_SAMPLE_RATE)
                
                # Export as WAV
                audio.export(
                    output_path,
                    format='wav',
                    parameters=['-acodec', 'pcm_s16le']  # 16-bit PCM
                )
                
                duration = len(audio) / 1000.0  # pydub duration is in milliseconds
                print(f"  ✅ {os.path.basename(input_path)} → {os.path.basename(output_path)} ({duration:.2f}s) [pydub]")
                return True
                
            except Exception as pydub_error:
                print(f"  ❌ ERROR: {os.path.basename(input_path)} - {str(pydub_error)}")
                return False
        else:
            print(f"  ❌ ERROR: {os.path.basename(input_path)} - {str(e)}")
            return False


def find_audio_files(sounds_dir):
    """
    Find all audio files in district folders.
    
    Args:
        sounds_dir (str): Path to sounds directory
    
    Returns:
        dict: Dictionary mapping button folders to list of audio files
    """
    audio_extensions = ['.mp3', '.m4a', '.wav']
    button_folders = {}
    
    for folder in sorted(os.listdir(sounds_dir)):
        folder_path = os.path.join(sounds_dir, folder)
        
        # Skip files (like README.md, this script, etc.)
        if not os.path.isdir(folder_path):
            continue
        
        # Skip hidden folders
        if folder.startswith('.'):
            continue
        
        # Find all audio files in this folder
        audio_files = []
        for ext in audio_extensions:
            pattern = os.path.join(folder_path, f'*{ext}')
            files = glob.glob(pattern)
            # Filter out already-converted wav files and zone identifiers
            for f in files:
                basename = os.path.basename(f)
                if basename.endswith('.wav') and os.path.exists(f.replace('.wav', '.mp3')):
                    continue  # Skip if this is a converted file
                if ':Zone.Identifier' in basename:
                    continue  # Skip Windows zone identifiers
                audio_files.append(f)
        
        if audio_files:
            button_folders[folder] = sorted(audio_files)
    
    return button_folders


def convert_all_files(sounds_dir, dry_run=False, overwrite=False):
    """
    Convert all audio files in the sounds directory.
    
    Args:
        sounds_dir (str): Path to sounds directory
        dry_run (bool): If True, only show what would be converted
        overwrite (bool): Whether to overwrite existing files
    """
    button_folders = find_audio_files(sounds_dir)
    
    if not button_folders:
        print("❌ No button folders with audio files found!")
        return
    
    print(f"\n{'='*70}")
    print(f"Audio Conversion for YAMNet Classification")
    print(f"{'='*70}")
    print(f"Target format: WAV, 16kHz, Mono, 16-bit PCM")
    print(f"Mode: {'DRY RUN' if dry_run else 'CONVERT'}")
    print(f"Overwrite: {'Yes' if overwrite else 'No'}")
    print(f"{'='*70}\n")
    
    total_files = 0
    converted_files = 0
    skipped_files = 0
    failed_files = 0
    
    for folder_name, audio_files in button_folders.items():
        print(f"\n📁 {folder_name} ({len(audio_files)} files)")
        
        for input_file in audio_files:
            total_files += 1
            
            # Generate output filename
            input_path = Path(input_file)
            output_path = input_path.with_suffix('.wav')
            
            if dry_run:
                status = "exists" if output_path.exists() else "new"
                print(f"  🔍 {input_path.name} → {output_path.name} [{status}]")
                continue
            
            # Convert the file
            success = convert_audio_file(str(input_path), str(output_path), overwrite)
            
            if success:
                if output_path.exists() and not overwrite:
                    skipped_files += 1
                else:
                    converted_files += 1
            else:
                failed_files += 1
    
    # Summary
    print(f"\n{'='*70}")
    print(f"Summary:")
    print(f"  Total files found: {total_files}")
    
    if not dry_run:
        print(f"  ✅ Converted: {converted_files}")
        print(f"  ⏭️  Skipped: {skipped_files}")
        if failed_files > 0:
            print(f"  ❌ Failed: {failed_files}")
    else:
        print(f"  (Run without --dry-run to convert)")
    
    print(f"{'='*70}\n")


def main():
    parser = argparse.ArgumentParser(
        description="Convert audio files to YAMNet-compatible WAV format (16kHz, mono)",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Preview what will be converted
  python3 convert_audio.py --dry-run
  
  # Convert all files
  python3 convert_audio.py
  
  # Re-convert existing files
  python3 convert_audio.py --overwrite
        """
    )
    
    parser.add_argument(
        '--dry-run',
        action='store_true',
        help='Show what would be converted without actually converting'
    )
    
    parser.add_argument(
        '--overwrite',
        action='store_true',
        help='Overwrite existing .wav files'
    )
    
    args = parser.parse_args()
    
    # Get the sounds directory (same directory as this script)
    script_dir = os.path.dirname(os.path.abspath(__file__))
    
    convert_all_files(script_dir, dry_run=args.dry_run, overwrite=args.overwrite)


if __name__ == '__main__':
    main()
