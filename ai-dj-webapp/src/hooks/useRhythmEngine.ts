import { useRef } from 'react';
import WaveSurfer from 'wavesurfer.js';
import * as Tone from 'tone';
import { gsap } from 'gsap';
import { soundFolders, type SoundButton } from '../state/soundConfig';
import { soundFiles } from '../state/soundFiles';

const PIXELS_PER_SECOND = 120;
const WAVEFORM_HEIGHT   = 80;

export function useRhythmEngine() {
  const trackRef      = useRef<HTMLDivElement>(null);
  const activePlayers = useRef(new Map<string, Tone.Player>());

  const getRandomSound = (button: SoundButton): string => {
    const files = soundFiles[button];
    const idx   = Math.floor(Math.random() * files.length);
    return `${soundFolders[button]}/${files[idx]}`;
  };

  const spawnWaveform = (url: string, buttonIndex: number) => {
    if (!trackRef.current) return;

    const track      = trackRef.current;
    const trackRect  = track.getBoundingClientRect();
    const btnCount   = 10;
    const btnWidth   = trackRect.width / btnCount;
    // Center of the button in track-relative coords
    const btnCenter  = btnWidth * buttonIndex + btnWidth / 2;
    // Shift right by half of WAVEFORM_HEIGHT so the rotated element is visually centred
    const leftPx     = btnCenter + WAVEFORM_HEIGHT / 2;

    const container = document.createElement('div');
    container.className        = 'waveform-clip';
    container.style.left       = `${leftPx}px`;
    container.style.bottom     = '0px';
    container.style.width      = '10px'; // placeholder until WaveSurfer ready
    container.style.height     = `${WAVEFORM_HEIGHT}px`;
    container.style.transformOrigin = 'bottom left';
    container.style.transform  = 'rotate(-90deg)';
    track.appendChild(container);

    const ws = WaveSurfer.create({
      container,
      waveColor:     '#00d4ff',
      progressColor: '#0077aa',
      height:        WAVEFORM_HEIGHT,
      interact:      false,
      cursorWidth:   0,
      url,
    });

    ws.on('ready', () => {
      const duration = ws.getDuration();
      const width    = duration * PIXELS_PER_SECOND;
      container.style.width = `${width}px`;
      ws.play();

      // Rise upward at exactly pixelsPerSecond — same speed as WaveSurfer renders
      gsap.to(container, {
        y:          -width,
        duration,
        ease:       'none',
        onComplete: () => { container.remove(); ws.destroy(); },
      });
    });

    ws.on('error', (e) => console.error('WaveSurfer error:', e));
  };

  const onPress = async (buttonKey: SoundButton, buttonIndex: number) => {
    if (activePlayers.current.has(buttonKey)) return; // already playing
    await Tone.start();

    const url    = getRandomSound(buttonKey);
    const player = new Tone.Player(url).toDestination();
    activePlayers.current.set(buttonKey, player); // register before await so onRelease can stop it
    try {
      await player.load(url);
      if (!activePlayers.current.has(buttonKey)) {
        player.dispose(); // was released during load
        return;
      }
      player.start();
      spawnWaveform(url, buttonIndex);
    } catch (e) {
      console.error('Tone.Player error:', e);
      activePlayers.current.delete(buttonKey);
      player.dispose();
    }
  };

  const onRelease = (buttonKey: SoundButton) => {
    const player = activePlayers.current.get(buttonKey);
    if (player) {
      try { player.stop(); } catch { /* already ended */ }
      player.dispose();
      activePlayers.current.delete(buttonKey);
    }
  };

  const stopAll = () => {
    activePlayers.current.forEach((p) => { try { p.stop(); } catch { /* */ } p.dispose(); });
    activePlayers.current.clear();
  };

  return { trackRef, onPress, onRelease, stopAll };
}
