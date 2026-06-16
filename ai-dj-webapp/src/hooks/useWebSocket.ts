import { useEffect, useRef, useState, useCallback } from 'react';

export type RosState =
  | 'welcome'
  | 'countdown'
  | 'recording'
  | 'recording_complete'
  | 'analyzing'
  | 'results';

export interface LedMatrix {
  width: number;
  height: number;
  data: number[];
}

export type ModelName = 'surveillance' | 'natural' | 'cultural';

export interface WSState {
  connected: boolean;
  rosState: RosState;
  pressedButtons: Set<string>;
  lastNav: string | null;
  classification: Record<ModelName, string | null>;
  llmResult: Record<ModelName, string | null>;
  ledMatrix: LedMatrix | null;
  recordingStartTs: number | null;
}

const WS_URL = '/ws';

const initialState: WSState = {
  connected: false,
  rosState: 'welcome',
  pressedButtons: new Set(),
  lastNav: null,
  classification: { surveillance: null, natural: null, cultural: null },
  llmResult:      { surveillance: null, natural: null, cultural: null },
  ledMatrix: null,
  recordingStartTs: null,
};

export function useWebSocket() {
  const [state, setState] = useState<WSState>(initialState);
  const wsRef = useRef<WebSocket | null>(null);
  const navClearRef = useRef<ReturnType<typeof setTimeout> | null>(null);
  const reconnectRef = useRef<ReturnType<typeof setTimeout> | null>(null);

  const connect = useCallback(() => {
    if (wsRef.current?.readyState === WebSocket.OPEN) return;

    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    const ws = new WebSocket(`${protocol}//${window.location.host}${WS_URL}`);
    wsRef.current = ws;

    ws.onopen = () => {
      setState(s => ({ ...s, connected: true }));
    };

    ws.onclose = () => {
      setState(s => ({ ...s, connected: false }));
      // Reconnect after 2s
      reconnectRef.current = setTimeout(connect, 2000);
    };

    ws.onerror = () => {
      ws.close();
    };

    ws.onmessage = (event) => {
      let msg: Record<string, unknown>;
      try {
        msg = JSON.parse(event.data as string);
      } catch {
        return;
      }

      const type = msg.type as string;

      if (type === 'state_change') {
        const newState = msg.state as RosState;
        setState(s => ({
          ...s,
          rosState: newState,
          // Reset classification results when starting a new session
          ...(newState === 'countdown' ? {
            classification: { surveillance: null, natural: null, cultural: null },
            llmResult:      { surveillance: null, natural: null, cultural: null },
            pressedButtons: new Set<string>(),
          } : {}),
          recordingStartTs: newState === 'recording' ? (msg.timestamp as number) : s.recordingStartTs,
        }));
        return;
      }

      if (type === 'arduino') {
        const token = msg.button as string;
        if (token.startsWith('PRESS_')) {
          const btn = token.slice('PRESS_'.length);
          setState(s => {
            const next = new Set(s.pressedButtons);
            next.add(btn);
            return { ...s, pressedButtons: next };
          });
        } else if (token.startsWith('RELEASE_')) {
          const btn = token.slice('RELEASE_'.length);
          setState(s => {
            const next = new Set(s.pressedButtons);
            next.delete(btn);
            return { ...s, pressedButtons: next };
          });
        }
        return;
      }

      if (type === 'nav') {
        const btn = msg.button as string;
        if (navClearRef.current) clearTimeout(navClearRef.current);
        setState(s => ({ ...s, lastNav: btn }));
        navClearRef.current = setTimeout(() => {
          setState(s => ({ ...s, lastNav: null }));
        }, 150);
        return;
      }

      if (type === 'classification') {
        const model = msg.model as 'surveillance' | 'natural' | 'cultural';
        const results = msg.results as string;
        setState(s => ({
          ...s,
          classification: { ...s.classification, [model]: results },
        }));
        return;
      }

      if (type === 'llm_result') {
        try {
          const parsed   = JSON.parse(msg.data as string) as { model: ModelName; sentence: string };
          const { model: m, sentence } = parsed;
          if (m && sentence) {
            setState(s => ({ ...s, llmResult: { ...s.llmResult, [m]: sentence } }));
          }
        } catch { /* malformed */ }
        return;
      }

      if (type === 'led_matrix') {
        setState(s => ({
          ...s,
          ledMatrix: {
            width: msg.width as number,
            height: msg.height as number,
            data: msg.data as number[],
          },
        }));
        return;
      }
    };
  }, []);

  useEffect(() => {
    connect();
    return () => {
      if (reconnectRef.current) clearTimeout(reconnectRef.current);
      if (navClearRef.current) clearTimeout(navClearRef.current);
      wsRef.current?.close();
    };
  }, [connect]);

  const postStart = useCallback(async () => {
    await fetch('/api/start', { method: 'POST' });
  }, []);

  const postClassify = useCallback(async () => {
    await fetch('/api/classify', { method: 'POST' });
  }, []);

  const postRedo = useCallback(async () => {
    await fetch('/api/redo', { method: 'POST' });
  }, []);

  const postRestart = useCallback(async () => {
    await fetch('/api/restart', { method: 'POST' });
  }, []);

  const postColorize = useCallback(async (model: string) => {
    await fetch('/api/colorize', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ model }),
    });
  }, []);

  return { ...state, postStart, postClassify, postRedo, postRestart, postColorize };
}
