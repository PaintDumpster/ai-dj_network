import { useEffect, useRef } from 'react';
import { gsap } from 'gsap';

type ModelName = 'surveillance' | 'natural' | 'cultural';

interface AgentMeta {
  agentName: string;
  icon: string;
  description: string;
  labels: string[];
}

const AGENT_META: Record<ModelName, AgentMeta> = {
  surveillance: {
    agentName: 'Vigil',
    icon: '/svgs/avatar_vigil.svg',
    description: 'Trained to identify security-relevant audio events in urban environments.',
    labels: ['Gunshot / gunfire','Screaming','Shatter','Siren','Helicopter','Crowd','Shout','Explosion','Bark'],
  },
  natural: {
    agentName: 'Flora',
    icon: '/svgs/avatar_flora.svg',
    description: 'Trained to identify sounds from the natural world and environment.',
    labels: ['Bird','Wind','Rustle','Water','Ocean','Insect','Rain','Thunder','Animal'],
  },
  cultural: {
    agentName: 'Ludo',
    icon: '/svgs/avatar_ludo.svg',
    description: 'Trained to identify cultural and social audio events.',
    labels: ['Clapping','Street Vendors','Group Singing','Fireworks','Cheering','Live Music','Drummers','Chatting','Sport Sounds','Laughing'],
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
  const meta    = AGENT_META[model];
  const parsed  = results ? parseResults(results) : [];
  const top5    = parsed.slice(0, 5);

  // Entrance animation
  useEffect(() => {
    gsap.to(cardRef.current, {
      opacity: 1, y: 0, duration: 0.6, ease: 'power3.out', delay: animDelay,
    });
  }, [animDelay]);

  return (
    <div
      ref={cardRef}
      className={`classification-card card-${model}${focused ? ' focused' : ''}`}
    >
      <div className="card-icon-wrap">
        <object className="card-icon" data={meta.icon} type="image/svg+xml" aria-label={meta.agentName} />
      </div>

      <div className="card-body">
        <p className="card-generated-text">{llmResult ?? 'listening…'}</p>

        {expanded && (
          <>
            <div className="card-divider" />
            <p className="card-agent-name">{meta.agentName}</p>
            <p className="card-description">{meta.description}</p>

            <div className="card-divider" />
            <p className="card-labels-title">Detected signals</p>
            {!results ? (
              <p className="card-waiting">waiting for results…</p>
            ) : (
              top5.map((r, i) => (
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
              ))
            )}

            <div className="card-labels">
              {meta.labels.map(l => (
                <span key={l} className="card-label-chip">{l}</span>
              ))}
            </div>
          </>
        )}
      </div>
    </div>
  );
}

export { type ModelName };
