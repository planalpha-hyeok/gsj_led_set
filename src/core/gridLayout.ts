import { findModule } from '../data/modules';
import type { Cell, Project } from '../types';

/**
 * 화면 격자 (열마다 폭, 행마다 높이가 다를 수 있음)
 * - 같은 열의 캐비닛은 폭이 같고, 같은 행의 캐비닛은 높이가 같다
 * - 열/행 크기는 모듈 장수로 바꾼다 (모듈 조합 캐비닛일 때만)
 * - 두 줄이 만나는 칸은 "바뀐 폭 × 바뀐 높이" 캐비닛이 된다
 */
export interface Track {
  mm: number;
  px: number;
  /** 모듈 장수 (모듈 조합 캐비닛일 때) */
  modules?: number;
  /** 기본 크기와 다른 줄인지 */
  changed: boolean;
}

export interface GridLayout {
  cols: Track[];
  rows: Track[];
  /** 각 열/행의 시작 위치 (mm, px) */
  colStartMm: number[];
  rowStartMm: number[];
  colStartPx: number[];
  rowStartPx: number[];
  widthMm: number;
  heightMm: number;
  widthPx: number;
  heightPx: number;
}

export interface CellSize {
  widthMm: number;
  heightMm: number;
  widthPx: number;
  heightPx: number;
}

function prefix(values: number[]): number[] {
  const out: number[] = [];
  let sum = 0;
  for (const v of values) {
    out.push(sum);
    sum += v;
  }
  return out;
}

/** 열 c 또는 행 r의 변경된 모듈 장수 (없거나 잘못된 값이면 undefined) */
function override(map: Record<string, number> | undefined, i: number): number | undefined {
  const v = map?.[String(i)];
  return typeof v === 'number' && v >= 1 ? Math.round(v) : undefined;
}

export function gridLayout(p: Project): GridLayout {
  const cab = p.cabinet;
  const mod = findModule(cab.moduleId);

  const cols: Track[] = Array.from({ length: p.cols }, (_, c) => {
    const n = mod ? override(p.colModules, c) : undefined;
    if (mod && n !== undefined && n !== cab.moduleCols) {
      return { mm: mod.widthMm * n, px: mod.widthPx * n, modules: n, changed: true };
    }
    return { mm: cab.widthMm, px: cab.widthPx, modules: mod ? cab.moduleCols : undefined, changed: false };
  });
  const rows: Track[] = Array.from({ length: p.rows }, (_, r) => {
    const n = mod ? override(p.rowModules, r) : undefined;
    if (mod && n !== undefined && n !== cab.moduleRows) {
      return { mm: mod.heightMm * n, px: mod.heightPx * n, modules: n, changed: true };
    }
    return { mm: cab.heightMm, px: cab.heightPx, modules: mod ? cab.moduleRows : undefined, changed: false };
  });

  return {
    cols,
    rows,
    colStartMm: prefix(cols.map((t) => t.mm)),
    rowStartMm: prefix(rows.map((t) => t.mm)),
    colStartPx: prefix(cols.map((t) => t.px)),
    rowStartPx: prefix(rows.map((t) => t.px)),
    widthMm: cols.reduce((s, t) => s + t.mm, 0),
    heightMm: rows.reduce((s, t) => s + t.mm, 0),
    widthPx: cols.reduce((s, t) => s + t.px, 0),
    heightPx: rows.reduce((s, t) => s + t.px, 0),
  };
}

export function cellSize(g: GridLayout, cell: Cell): CellSize {
  const col = g.cols[cell.c];
  const row = g.rows[cell.r];
  return { widthMm: col.mm, heightMm: row.mm, widthPx: col.px, heightPx: row.px };
}

export function cellPixels(g: GridLayout, cell: Cell): number {
  const s = cellSize(g, cell);
  return s.widthPx * s.heightPx;
}

/** 크기별 캐비닛 수량 (자재표용). 많은 순 */
export function cabinetSizeCounts(g: GridLayout): { widthMm: number; heightMm: number; widthPx: number; heightPx: number; count: number }[] {
  const map = new Map<string, { widthMm: number; heightMm: number; widthPx: number; heightPx: number; count: number }>();
  for (const col of g.cols) {
    for (const row of g.rows) {
      const key = `${col.mm}x${row.mm}`;
      const e = map.get(key) ?? { widthMm: col.mm, heightMm: row.mm, widthPx: col.px, heightPx: row.px, count: 0 };
      e.count += 1;
      map.set(key, e);
    }
  }
  return [...map.values()].sort((a, b) => b.count - a.count);
}
