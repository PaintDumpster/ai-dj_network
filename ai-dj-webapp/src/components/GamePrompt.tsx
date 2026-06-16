import { useEffect, useRef, useState } from 'react';
import { gsap } from 'gsap';

interface Props {
  lastNav: string | null;
  onClassify: () => void;
  onRedo: () => void;
}

export default function GamePrompt({ lastNav, onClassify, onRedo }: Props) {
  const [selected, setSelected] = useState<'yes' | 'no'>('yes');
  const boxRef = useRef<HTMLDivElement>(null);

  // Slide in on mount
  useEffect(() => {
    gsap.to(boxRef.current, { opacity: 1, y: 0, duration: 0.4, ease: 'power3.out' });
  }, []);

  // Handle nav events (hardware) and arrow keys (keyboard fallback)
  useEffect(() => {
    if (!lastNav) return;
    if (lastNav === 'NAV_C') setSelected('yes');
    if (lastNav === 'NAV_B') setSelected('no');
    if (lastNav === 'SELECT') {
      selected === 'yes' ? onClassify() : onRedo();
    }
  }, [lastNav, selected, onClassify, onRedo]);

  useEffect(() => {
    const onKey = (e: KeyboardEvent) => {
      if (e.code === 'ArrowRight') setSelected('yes');
      if (e.code === 'ArrowLeft')  setSelected('no');
      if (e.code === 'Enter') {
        setSelected(s => { s === 'yes' ? onClassify() : onRedo(); return s; });
      }
    };
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  }, [onClassify, onRedo]);

  return (
    <div className="game-prompt-overlay">
      <div className="game-prompt-box" ref={boxRef}>
        <p className="game-prompt-question">Submit your audio mix?</p>

        <div className="game-prompt-choices">
          <button
            className={`prompt-choice${selected === 'no' ? ' selected-no' : ''}`}
            onClick={onRedo}
          >
            ← No
          </button>
          <button
            className={`prompt-choice${selected === 'yes' ? ' selected-yes' : ''}`}
            onClick={onClassify}
          >
            Yes →
          </button>
        </div>

        <p className="game-prompt-hint">
          <span className="nav-key">B</span> No &nbsp;·&nbsp;
          <span className="nav-key">C</span> Yes &nbsp;·&nbsp;
          <span className="nav-key">SELECT</span> Confirm
        </p>
      </div>
    </div>
  );
}
