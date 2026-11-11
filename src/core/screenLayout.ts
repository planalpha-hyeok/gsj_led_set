import type { Project } from '../types';

export interface ScreenInfo {
  cabinetCount: number;
  cabinetPixels: number;
  widthPx: number;
  heightPx: number;
  totalPixels: number;
  widthMm: number;
  heightMm: number;
  areaM2: number;
  weightKg: number;
  pitchMm: number;
  aspect: string;
}

/** 화면 전체 크기·해상도 계산 */
export function computeScreen(p: Project): ScreenInfo {
  const { cabinet: cab, cols, rows } = p;
  const widthPx = cab.widthPx * cols;
  const heightPx = cab.heightPx * rows;
  const widthMm = cab.widthMm * cols;
  const heightMm = cab.heightMm * rows;
  return {
    cabinetCount: cols * rows,
    cabinetPixels: cab.widthPx * cab.heightPx,
    widthPx,
    heightPx,
    totalPixels: widthPx * heightPx,
    widthMm,
    heightMm,
    areaM2: (widthMm * heightMm) / 1_000_000,
    weightKg: cab.weightKg * cols * rows,
    pitchMm: cab.widthPx > 0 ? cab.widthMm / cab.widthPx : 0,
    aspect: aspectText(widthPx, heightPx),
  };
}

function aspectText(w: number, h: number): string {
  if (w <= 0 || h <= 0) return '-';
  const ratio = w / h;
  const known: [number, string][] = [[16 / 9, '16:9'], [4 / 3, '4:3'], [21 / 9, '21:9'], [1, '1:1'], [9 / 16, '9:16'], [32 / 9, '32:9']];
  for (const [v, t] of known) if (Math.abs(ratio - v) < 0.01) return t;
  return `${ratio.toFixed(2)}:1`;
}
