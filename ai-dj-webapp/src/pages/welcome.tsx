import { useEffect, useRef, useCallback } from 'react';
import { gsap } from 'gsap';
import type { useWebSocket } from '../hooks/useWebSocket';
import Countdown from '../components/Countdown';

interface Props { ws: ReturnType<typeof useWebSocket>; }

export default function Welcome({ ws }: Props) {
  const titleRef    = useRef<HTMLHeadingElement>(null);
  const subtitleRef = useRef<HTMLParagraphElement>(null);

  useEffect(() => {
    const tl = gsap.timeline();
    tl.to(titleRef.current, { opacity: 1, y: 0, duration: 1.2, ease: 'power3.out', delay: 0.3 })
      .to(subtitleRef.current, { opacity: 1, duration: 0.8, ease: 'power2.out' }, '-=0.4');
    return () => { tl.kill(); };
  }, []);

  useEffect(() => {
    if (!subtitleRef.current) return;
    const tween = gsap.to(subtitleRef.current, {
      opacity: 0.2,
      duration: 0.9,
      repeat: -1,
      yoyo: true,
      ease: 'sine.inOut',
      delay: 1.6,
    });
    return () => { tween.kill(); };
  }, []);

  const handleStart = useCallback(() => {
    if (ws.connected && ws.rosState === 'welcome') {
      ws.postStart();
    }
  }, [ws]);

  useEffect(() => {
    const onKey = (e: KeyboardEvent) => {
      if (e.code === 'Space' || e.code === 'Enter') handleStart();
    };
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  }, [handleStart]);

  return (
    <div className="welcome-page" onClick={handleStart} style={{ cursor: 'pointer' }}>
      <div className="welcome-status">
        <div className={`connection-dot${ws.connected ? ' connected' : ''}`} />
        {ws.connected ? 'connected' : 'connecting…'}
      </div>

      <h1 className="welcome-title" ref={titleRef} style={{ transform: 'translateY(20px)' }}>
        AI DJ: Audio Bias Pavilion
      </h1>
      <p className="welcome-subtitle" ref={subtitleRef}>
        press select or click to start
      </p>

      {ws.rosState === 'countdown' && <Countdown />}
    </div>
  );
}
