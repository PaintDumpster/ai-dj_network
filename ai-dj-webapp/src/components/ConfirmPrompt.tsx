import { useEffect, useRef, useState } from 'react';
import { gsap } from 'gsap';

interface Props {
  question: string;
  yesLabel?: string;
  noLabel?: string;
  lastNav: string | null;
  onYes: () => void;
  onNo: () => void;
  locked?: boolean;
}

export default function ConfirmPrompt({ question, yesLabel = 'Yes', noLabel = 'No', lastNav, onYes, onNo, locked }: Props) {
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
      selected === 'yes' ? onYes() : onNo();
    }
  }, [lastNav, selected, onYes, onNo]);

  useEffect(() => {
    const onKey = (e: KeyboardEvent) => {
      if (locked) return;
      if (e.code === 'ArrowRight') setSelected('yes');
      if (e.code === 'ArrowLeft')  setSelected('no');
      if (e.code === 'Enter') {
        setSelected(s => { s === 'yes' ? onYes() : onNo(); return s; });
      }
    };
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  }, [locked, onYes, onNo]);

  return (
    <div className="confirm-prompt-overlay">
      <div className="confirm-prompt-box" ref={boxRef}>
        <p className="confirm-prompt-question">{question}</p>

        <div className="confirm-prompt-choices">
          <button
            className={`prompt-choice${selected === 'no' ? ' selected-no' : ''}`}
            onClick={onNo}
          >
            ← {noLabel}
          </button>
          <button
            className={`prompt-choice${selected === 'yes' ? ' selected-yes' : ''}`}
            onClick={onYes}
          >
            {yesLabel} →
          </button>
        </div>

        <p className="confirm-prompt-hint">
          <span className="nav-key">B</span> {noLabel} &nbsp;·&nbsp;
          <span className="nav-key">C</span> {yesLabel} &nbsp;·&nbsp;
          <span className="nav-key">*</span> Confirm
        </p>
      </div>
    </div>
  );
}
