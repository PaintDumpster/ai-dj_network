import { useEffect, useRef } from 'react';
import type { LedMatrix as LedMatrixData } from '../hooks/useWebSocket';

interface Props { matrix: LedMatrixData | null; }

export default function LedMatrix({ matrix }: Props) {
  const canvasRef = useRef<HTMLCanvasElement>(null);

  useEffect(() => {
    if (!matrix || !canvasRef.current) return;
    const { width, height, data } = matrix;
    const canvas = canvasRef.current;
    canvas.width  = width;
    canvas.height = height;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    const imgData = ctx.createImageData(width, height);
    for (let i = 0; i < data.length; i++) {
      const v = data[i];
      const px = i * 4;
      imgData.data[px]     = v;
      imgData.data[px + 1] = v;
      imgData.data[px + 2] = v;
      imgData.data[px + 3] = 255;
    }
    ctx.putImageData(imgData, 0, 0);
  }, [matrix]);

  return (
    <canvas
      ref={canvasRef}
      className="led-matrix-canvas"
      style={{ imageRendering: 'pixelated' }}
    />
  );
}
