# Sound Files for Button Presses

This folder should contain 10 audio files (.wav format) corresponding to each button:

- `button_1.wav` - Sound for button 1
- `button_2.wav` - Sound for button 2
- `button_3.wav` - Sound for button 3
- `button_4.wav` - Sound for button 4
- `button_5.wav` - Sound for button 5
- `button_6.wav` - Sound for button 6
- `button_7.wav` - Sound for button 7
- `button_8.wav` - Sound for button 8
- `button_9.wav` - Sound for button 9
- `button_10.wav` - Sound for button 10

## Usage

Place your .wav audio files here with the exact naming convention above.

If a sound file is missing when a button is pressed, the system will generate a placeholder sine wave at a frequency corresponding to the button number.

## Waveform Output

The built waveform will be saved to `/tmp/waveform_<timestamp>.csv` after the 30-second recording period ends.
