import { useEffect, useRef } from 'react';
import { gsap } from 'gsap';

type ModelName = 'surveillance' | 'natural' | 'cultural';

interface ModelMeta {
  description: string;
  labels: string[];
  colorVar: string;
}

const MODEL_META: Record<ModelName, ModelMeta> = {
  surveillance: {
    description: 'Trained to identify security-relevant audio events in urban environments.',
    labels: ['Gunshot / gunfire','Screaming','Shatter','Siren','Helicopter','Crowd','Shout','Explosion','Bark'],
    colorVar: 'var(--surveillance)',
  },
  natural: {
    description: 'Trained to identify sounds from the natural world and environment.',
    labels: ['Bird','Wind','Rustle','Water','Ocean','Insect','Rain','Thunder','Animal'],
    colorVar: 'var(--natural)',
  },
  cultural: {
    description: 'Trained to identify cultural and social audio events.',
    labels: ['Clapping','Street Vendors','Group Singing','Fireworks','Cheering','Live Music','Drummers','Chatting','Sport Sounds','Laughing'],
    colorVar: 'var(--cultural)',
  },
};

/** Parse "1. Label: 0.8745\n2. ..." into [{label, score}] */
function parseResults(raw: string): { label: string; score: number }[] {
  return raw
    .split('\n')
    .map(line => line.match(/\d+\.\s+(.+?):\s+([\d.]+)/))
    .filter(Boolean)
    .map(m => ({ label: m![1].trim(), score: parseFloat(m![2]) }));
}

interface Props {
  model: ModelName;
  results: string | null;
  llmResult: string | null;
  focused: boolean;
  expanded: boolean;
  animDelay: number;
}

export default function ClassificationCard({ model, results, llmResult, focused, expanded, animDelay }: Props) {
  const cardRef = useRef<HTMLDivElement>(null);
  const bodyRef = useRef<HTMLDivElement>(null);
  const meta    = MODEL_META[model];
  const parsed  = results ? parseResults(results) : [];
  const top3    = parsed.slice(0, 3);
  const top5    = parsed.slice(0, 5);

  // Entrance animation
  useEffect(() => {
    gsap.to(cardRef.current, {
      opacity: 1, y: 0, duration: 0.6, ease: 'power3.out', delay: animDelay,
    });
  }, [animDelay]);

  const modelLabel = model.charAt(0).toUpperCase() + model.slice(1);

  return (
    <div
      ref={cardRef}
      className={`classification-card card-${model}${focused ? ' focused' : ''}`}
    >
      <div className="card-header">
        <div className="card-color-dot" />
        <span className="card-model-name">{modelLabel}</span>
        {focused && <span className="card-expand-hint">{expanded ? '▲ collapse' : '▼ expand'}</span>}
      </div>

      <div className="card-body" ref={bodyRef}>
        {!results ? (
          <p style={{ color: 'var(--muted)', fontSize: '0.75rem' }}>waiting for results…</p>
        ) : (
          <>
            {/* Collapsed: top-3 bars */}
            {top3.map((r, i) => (
              <div className="result-row" key={i}>
                <span className="result-rank">{i + 1}.</span>
                <span className="result-label">{r.label}</span>
                <div className="result-bar-track">
                  <div
                    className="result-bar-fill"
                    style={{ width: `${Math.round(r.score * 100)}%` }}
                  />
                </div>
                <span className="result-score">{(r.score * 100).toFixed(0)}%</span>
              </div>
            ))}

            {/* Expanded extras */}
            {expanded && (
              <>
                <div className="card-divider" />
                <p className="card-description">{meta.description}</p>

                <p className="card-labels-title">Model labels</p>
                <div className="card-labels">
                  {meta.labels.map(l => (
                    <span key={l} className="card-label-chip">{l}</span>
                  ))}
                </div>

                <div className="card-divider" />
                <p className="card-labels-title">Full top-5</p>
                {top5.map((r, i) => (
                  <div className="result-row" key={i}>
                    <span className="result-rank">{i + 1}.</span>
                    <span className="result-label">{r.label}</span>
                    <div className="result-bar-track">
                      <div
                        className="result-bar-fill"
                        style={{ width: `${Math.round(r.score * 100)}%` }}
                      />
                    </div>
                    <span className="result-score">{(r.score * 100).toFixed(0)}%</span>
                  </div>
                ))}

                {llmResult && (
                  <>
                    <div className="card-divider" />
                    <p className="card-llm">{llmResult}</p>
                  </>
                )}
              </>
            )}
          </>
        )}
      </div>
    </div>
  );
}

export { type ModelName };
