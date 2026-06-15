import { useEffect, useRef, useState } from 'react';
import { gsap } from 'gsap';
import type { useWebSocket } from '../hooks/useWebSocket';
import { useRhythmEngine } from '../hooks/useRhythmEngine';
import LedMatrix from '../components/LedMatrix';
import GamePrompt from '../components/GamePrompt';
import type { SoundButton } from '../state/soundConfig';

interface Props { ws: ReturnType<typeof useWebSocket>; }

const BUTTON_KEYS: SoundButton[] = ['b1','b2','b3','b4','b5','b6','b7','b8','b9','b10'];
const BUTTON_LABELS = ['1','2','3','4','5','6','7','8','9','0'];
const TOTAL_SECONDS = 30;

function formatTime(sec: number) {
  const s = Math.max(0, Math.round(sec));
  return `${String(Math.floor(s / 60)).padStart(2,'0')}:${String(s % 60).padStart(2,'0')}`;
}

export default function Game({ ws }: Props) {
  const { trackRef, onPress, onRelease, stopAll } = useRhythmEngine();
  const [elapsed, setElapsed]   = useState(0);
  const timerBarRef             = useRef<HTMLDivElement>(null);
  const intervalRef             = useRef<ReturnType<typeof setInterval> | null>(null);
  const prevPressedRef          = useRef<Set<string>>(new Set());

  // Start / stop the visual timer when rosState changes
  useEffect(() => {
    if (ws.rosState === 'recording') {
      setElapsed(0);
      // Animate timer bar from 100% → 0% over 30s
      gsap.fromTo(timerBarRef.current,
        { scaleX: 1 },
        { scaleX: 0, duration: TOTAL_SECONDS, ease: 'none' }
      );
      intervalRef.current = setInterval(() => {
        setElapsed(e => e + 1);
      }, 1000);
    } else {
      if (intervalRef.current) { clearInterval(intervalRef.current); intervalRef.current = null; }
      gsap.killTweensOf(timerBarRef.current);
    }
    return () => {
      if (intervalRef.current) clearInterval(intervalRef.current);
    };
  }, [ws.rosState]);

  // Handle press/release events from WebSocket
  useEffect(() => {
    const current  = ws.pressedButtons;
    const previous = prevPressedRef.current;

    // New presses
    current.forEach(btn => {
      if (!previous.has(btn)) {
        const idx = parseInt(btn, 10) - 1; // "1"→0 … "10"→9
        if (idx >= 0 && idx < BUTTON_KEYS.length) {
          onPress(BUTTON_KEYS[idx], idx);
        }
      }
    });

    // Releases
    previous.forEach(btn => {
      if (!current.has(btn)) {
        const idx = parseInt(btn, 10) - 1;
        if (idx >= 0 && idx < BUTTON_KEYS.length) {
          onRelease(BUTTON_KEYS[idx]);
        }
      }
    });

    prevPressedRef.current = new Set(current);
  }, [ws.pressedButtons, onPress, onRelease]);

  // Stop all audio when recording ends
  useEffect(() => {
    if (ws.rosState !== 'recording') stopAll();
  }, [ws.rosState, stopAll]);

  const remaining = TOTAL_SECONDS - elapsed;
  const isRecording = ws.rosState === 'recording';

  return (
    <div className="game-page">
      {/* Header with timer */}
      <div className="game-header">
        <span className="game-title">AI DJ — Recording</span>
        <span className="timer-display" style={{ color: remaining <= 10 ? 'var(--red)' : 'var(--amber)' }}>
          {isRecording ? formatTime(remaining) : '--:--'}
        </span>
      </div>

      {/* Timer progress bar */}
      <div className="timer-bar-track">
        <div className="timer-bar-fill" ref={timerBarRef} />
      </div>

      {/* Waveform track — LED matrix behind, WaveSurfer clips on top */}
      <div className="track" ref={trackRef}>
        <LedMatrix matrix={ws.ledMatrix} />
      </div>

      {/* Sound buttons */}
      <div className="buttons-row">
        {BUTTON_KEYS.map((key, i) => {
          const btnNum = String(i + 1);
          const active = ws.pressedButtons.has(btnNum);
          return (
            <button
              key={key}
              className={`sound-btn${active ? ' active' : ''}`}
              onPointerDown={() => onPress(key, i)}
              onPointerUp={() => onRelease(key)}
              onPointerLeave={() => onRelease(key)}
            >
              {BUTTON_LABELS[i]}
            </button>
          );
        })}
      </div>

      {/* Accept / redo overlay after recording ends */}
      {ws.rosState === 'recording_complete' && (
        <GamePrompt
          lastNav={ws.lastNav}
          onClassify={ws.postClassify}
          onRedo={ws.postRedo}
        />
      )}
    </div>
  );
}
