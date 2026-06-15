import { useEffect, useRef } from 'react';
import { gsap } from 'gsap';

export default function Countdown() {
  const refs = [
    useRef<HTMLDivElement>(null),
    useRef<HTMLDivElement>(null),
    useRef<HTMLDivElement>(null),
    useRef<HTMLDivElement>(null),
  ];

  useEffect(() => {
    const tl = gsap.timeline();
    const labels = ['3', '2', '1', 'GO'];

    labels.forEach((_, i) => {
      tl.to(refs[i].current, { opacity: 1, scale: 1, duration: 0.25, ease: 'back.out(1.4)' })
        .to(refs[i].current, { opacity: 0, scale: 0.7, duration: 0.5, ease: 'power2.in' }, '+=0.4');
    });

    return () => { tl.kill(); };
  // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  return (
    <div className="countdown-overlay">
      {['3', '2', '1', 'GO'].map((label, i) => (
        <div key={label} className="countdown-digit" ref={refs[i]}>
          {label}
        </div>
      ))}
    </div>
  );
}
