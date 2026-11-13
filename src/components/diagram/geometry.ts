import { gridLayout } from '../../core/gridLayout';
import type { Cell, Project } from '../../types';

/** 배선도 좌표 계산 (실제 캐비닛 가로세로 비율 유지) */
export interface Geometry {
  cellW: number;
  cellH: number;
  padL: number;
  padT: number;
  padR: number;
  padB: number;
  width: number;
  height: number;
  gridW: number;
  gridH: number;
  font: number;
  /** 열/행별 위치와 크기 (그림 좌표). 크기가 섞인 화면은 실제 비율대로 */
  colX: number[];
  colW: number[];
  rowY: number[];
  rowH: number[];
}

/** 좌우 최소 여백, 그림 최소 폭 (제목·범례가 들어갈 자리) */
export const SIDE_MARGIN = 60;
export const MIN_WIDTH = 680;

/**
 * 격자는 항상 가로 가운데에 놓는다 (좌우 여백 동일)
 * extraBottom: 범례 높이 (없으면 0)
 */
export function computeGeometry(p: Project, extraBottom = 0): Geometry {
  // 기본 캐비닛 폭을 100으로 두고 실제 mm 비율로 그림
  const cellW = 100;
  const k = p.cabinet.widthMm > 0 ? cellW / p.cabinet.widthMm : 1;
  const cellH = Math.max(30, p.cabinet.heightMm * k);
  const lay = gridLayout(p);
  const colW = lay.cols.map((t) => t.mm * k);
  const rowH = lay.rows.map((t) => Math.max(20, t.mm * k));
  const gridW = colW.reduce((s, v) => s + v, 0);
  const gridH = rowH.reduce((s, v) => s + v, 0);
  const width = Math.max(gridW + SIDE_MARGIN * 2, MIN_WIDTH);
  const padL = (width - gridW) / 2;
  const padR = width - padL - gridW;
  const padT = 96;
  const padB = 48 + extraBottom;
  return {
    cellW,
    cellH,
    padL,
    padT,
    padR,
    padB,
    width,
    height: padT + gridH + padB,
    gridW,
    gridH,
    font: Math.min(cellW, cellH) * 0.17,
    colX: prefix(colW).map((v) => v + padL),
    colW,
    rowY: prefix(rowH).map((v) => v + padT),
    rowH,
  };
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

/** 캐비닛 칸의 위치와 크기 (그림 좌표) */
export function cellRect(g: Geometry, cell: Cell): { x: number; y: number; w: number; h: number } {
  return { x: g.colX[cell.c], y: g.rowY[cell.r], w: g.colW[cell.c], h: g.rowH[cell.r] };
}

export function cellOrigin(g: Geometry, cell: Cell): { x: number; y: number } {
  return { x: g.colX[cell.c], y: g.rowY[cell.r] };
}

export function cellCenter(g: Geometry, cell: Cell): { x: number; y: number } {
  const o = cellRect(g, cell);
  return { x: o.x + o.w / 2, y: o.y + o.h / 2 };
}

/** 열 이름: A, B, ..., Z, AA ... */
export function colName(i: number): string {
  let s = '';
  let n = i + 1;
  while (n > 0) {
    const m = (n - 1) % 26;
    s = String.fromCharCode(65 + m) + s;
    n = Math.floor((n - 1) / 26);
  }
  return s;
}

export function cellName(cell: Cell): string {
  return `${colName(cell.c)}${cell.r + 1}`;
}
