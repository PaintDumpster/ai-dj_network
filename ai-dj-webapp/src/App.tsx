import { useEffect, useState } from 'react';
import { useWebSocket } from './hooks/useWebSocket';
import Welcome from './pages/welcome';
import Game from './pages/game';
import Classification from './pages/classification';
import ConfirmPrompt from './components/ConfirmPrompt';
import './index.css';

const PAGE_MAP: Record<string, 'welcome' | 'game' | 'classification'> = {
  welcome:            'welcome',
  countdown:          'welcome',
  recording:          'game',
  recording_complete: 'game',
  analyzing:          'classification',
  results:            'classification',
};

export default function App() {
  const ws = useWebSocket();
  const page = PAGE_MAP[ws.rosState] ?? 'welcome';
  const [restartOpen, setRestartOpen] = useState(false);

  // # (BACK) opens the restart-confirmation prompt from anywhere — its only job.
  useEffect(() => {
    if (ws.lastNav === 'BACK') setRestartOpen(true);
  }, [ws.lastNav]);

  const handleRestartYes = () => {
    setRestartOpen(false);
    ws.postRestart();
  };
  const handleRestartNo = () => setRestartOpen(false);

  // While the restart prompt is open, mask lastNav for the underlying page so its
  // own nav handlers don't also react to the button press meant for this prompt.
  const childWs = restartOpen ? { ...ws, lastNav: null } : ws;

  return (
    <div className="App">
      {page === 'welcome'        && <Welcome ws={childWs} locked={restartOpen} />}
      {page === 'game'           && <Game ws={childWs} locked={restartOpen} />}
      {page === 'classification' && <Classification ws={childWs} locked={restartOpen} />}
      {restartOpen && (
        <ConfirmPrompt
          question="Restart experience?"
          lastNav={ws.lastNav}
          onYes={handleRestartYes}
          onNo={handleRestartNo}
        />
      )}
    </div>
  );
}
