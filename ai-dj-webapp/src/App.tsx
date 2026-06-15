import { useWebSocket } from './hooks/useWebSocket';
import Welcome from './pages/welcome';
import Game from './pages/game';
import Classification from './pages/classification';
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

  return (
    <div className="App">
      {page === 'welcome'        && <Welcome ws={ws} />}
      {page === 'game'           && <Game ws={ws} />}
      {page === 'classification' && <Classification ws={ws} />}
    </div>
  );
}
