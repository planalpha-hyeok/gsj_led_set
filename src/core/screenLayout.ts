import type { Project } from '../types';
import { gridLayout } from './gridLayout';

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
  const g = gridLayout(p);
  const { widthPx, heightPx, widthMm, heightMm } = g;
  // 크기가 다른 캐비닛의 무게는 기본 캐비닛 대비 면적 비례로 추정
  const baseArea = cab.widthMm * cab.heightMm;
  const weightKg = baseArea > 0
    ? g.cols.reduce((s, col) => s + g.rows.reduce((t, row) => t + cab.weightKg * (col.mm * row.mm) / baseArea, 0), 0)
    : 0;
  return {
    cabinetCount: cols * rows,
    cabinetPixels: cab.widthPx * cab.heightPx,
    widthPx,
    heightPx,
    totalPixels: widthPx * heightPx,
    widthMm,
    heightMm,
    areaM2: (widthMm * heightMm) / 1_000_000,
    weightKg,
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
