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
}

export function computeGeometry(p: Project, extraBottom = 0): Geometry {
  const cellW = 100;
  const ratio = p.cabinet.widthMm > 0 ? p.cabinet.heightMm / p.cabinet.widthMm : 1;
  const cellH = Math.max(30, Math.min(300, cellW * ratio));
  const padL = 70;
  const padT = 70;
  const padR = 40;
  const padB = 60 + extraBottom;
  const gridW = cellW * p.cols;
  const gridH = cellH * p.rows;
  return {
    cellW,
    cellH,
    padL,
    padT,
    padR,
    padB,
    width: padL + gridW + padR,
    height: padT + gridH + padB,
    gridW,
    gridH,
    font: Math.min(cellW, cellH) * 0.17,
  };
}

export function cellOrigin(g: Geometry, cell: Cell): { x: number; y: number } {
  return { x: g.padL + cell.c * g.cellW, y: g.padT + cell.r * g.cellH };
}

export function cellCenter(g: Geometry, cell: Cell): { x: number; y: number } {
  const o = cellOrigin(g, cell);
  return { x: o.x + g.cellW / 2, y: o.y + g.cellH / 2 };
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
