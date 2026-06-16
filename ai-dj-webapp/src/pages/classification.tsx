import { useEffect, useRef, useState } from 'react';
import { gsap } from 'gsap';
import type { useWebSocket } from '../hooks/useWebSocket';
import ClassificationCard, { type ModelName } from '../components/ClassificationCard';

interface Props { ws: ReturnType<typeof useWebSocket>; locked?: boolean; }

const MODELS: ModelName[] = ['surveillance', 'natural', 'cultural'];

export default function Classification({ ws, locked }: Props) {
  const [phase, setPhase]       = useState<'classifying' | 'generating' | 'results'>('classifying');
  const [focusIdx, setFocusIdx]       = useState(0);
  const [expandedSet, setExpandedSet] = useState<Set<number>>(new Set());
  const labelRef  = useRef<HTMLParagraphElement>(null);
  const label2Ref = useRef<HTMLParagraphElement>(null);

  const { rosState, classification, llmResult, lastNav, postColorize } = ws;
  const anyResult = Object.values(classification).some(r => r !== null);
  const allResults = MODELS.every(m => classification[m] !== null);

  // Phase transitions
  useEffect(() => {
    if (rosState === 'results' || allResults) {
      setPhase('results');
    } else if (anyResult) {
      setPhase('generating');
    } else {
      setPhase('classifying');
    }
  }, [rosState, anyResult, allResults]);

  // Animate the loading label in
  useEffect(() => {
    if (phase === 'classifying') {
      gsap.to(labelRef.current, { opacity: 1, duration: 0.5 });
      // Pulsing dots
      gsap.to('.loading-dot', {
        opacity: 0.2, duration: 0.5, repeat: -1, yoyo: true,
        stagger: { each: 0.2, repeat: -1 },
      });
    }
    if (phase === 'generating') {
      gsap.to(labelRef.current,  { opacity: 0, duration: 0.3 });
      gsap.to(label2Ref.current, { opacity: 1, duration: 0.5, delay: 0.3 });
    }
  }, [phase]);

  // Navigation (card focus + expand) — hardware nav events
  useEffect(() => {
    if (!lastNav) return;
    if (phase !== 'results') return;

    if (lastNav === 'NAV_C' || lastNav === 'NAV_A') {
      setFocusIdx(i => (i + 1) % MODELS.length);
      setExpandedSet(new Set());
    }
    if (lastNav === 'NAV_B' || lastNav === 'NAV_D') {
      setFocusIdx(i => (i - 1 + MODELS.length) % MODELS.length);
      setExpandedSet(new Set());
    }
    if (lastNav === 'SELECT') {
      setExpandedSet(prev => {
        const next = new Set(prev);
        next.has(focusIdx) ? next.delete(focusIdx) : next.add(focusIdx);
        return next;
      });
    }
  }, [lastNav, phase, focusIdx]);

  // Keyboard arrow fallback
  useEffect(() => {
    if (phase !== 'results') return;
    const onKey = (e: KeyboardEvent) => {
      if (locked) return;
      if (e.code === 'ArrowRight') { setFocusIdx(i => (i + 1) % MODELS.length); setExpandedSet(new Set()); }
      if (e.code === 'ArrowLeft')  { setFocusIdx(i => (i - 1 + MODELS.length) % MODELS.length); setExpandedSet(new Set()); }
      if (e.code === 'Enter') {
        setExpandedSet(prev => {
          const next = new Set(prev);
          next.has(focusIdx) ? next.delete(focusIdx) : next.add(focusIdx);
          return next;
        });
      }
    };
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  }, [phase, focusIdx, locked]);

  // Colorize the physical LED waveform to match whichever card is expanded;
  // revert to white when collapsed, switching cards, or leaving this page.
  useEffect(() => {
    if (phase !== 'results') return;
    const model = expandedSet.has(focusIdx) ? MODELS[focusIdx] : '';
    postColorize(model);
    return () => { postColorize(''); };
  }, [expandedSet, focusIdx, phase, postColorize]);

  return (
    <div className="classification-page">
      <div className="classification-header">
        <h1>AI DJ — Audio Bias Pavilion</h1>
      </div>

      {phase !== 'results' ? (
        <div className="loading-container">
          <p className="loading-label" ref={labelRef}>classifying…</p>
          <p className="loading-label" ref={label2Ref} style={{ position: 'absolute', opacity: 0 }}>
            generating agent response…
          </p>
          <div className="loading-dots">
            <div className="loading-dot" />
            <div className="loading-dot" />
            <div className="loading-dot" />
          </div>
        </div>
      ) : (
        <>
          <div className="cards-container">
            {MODELS.map((model, i) => (
              <ClassificationCard
                key={model}
                model={model}
                results={classification[model]}
                llmResult={llmResult[model]}
                focused={focusIdx === i}
                expanded={expandedSet.has(i)}
                animDelay={i * 0.15}
              />
            ))}
          </div>

          <div className="nav-hint-bar">
            <span className="nav-hint"><span className="nav-key">B</span><span className="nav-key">C</span> Navigate cards</span>
            <span className="nav-hint"><span className="nav-key">*</span> Expand / collapse</span>
          </div>
        </>
      )}
    </div>
  );
}
