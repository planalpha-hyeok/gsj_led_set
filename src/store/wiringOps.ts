import type { Cell } from '../types';

/** 수동 배선 편집 (데이터 포트·전원 회로 공용 순수 함수) */
interface Run {
  id: string;
  cells: Cell[];
}

const same = (a: Cell, b: Cell) => a.c === b.c && a.r === b.r;

/** 캐비닛이 어느 배선에 들어 있는지 찾기 */
export function findOwner<T extends Run>(runs: T[], cell: Cell): { run: T; pos: number } | null {
  for (const run of runs) {
    const pos = run.cells.findIndex((c) => same(c, cell));
    if (pos >= 0) return { run, pos };
  }
  return null;
}

export function mapRun<T extends Run>(runs: T[], id: string, fn: (cells: Cell[]) => Cell[]): T[] {
  return runs.map((r) => (r.id === id ? { ...r, cells: fn(r.cells) } : r));
}

/** 끝에 캐비닛 추가 (다른 배선에 이미 있으면 그대로) */
export function appendCell<T extends Run>(runs: T[], id: string, cell: Cell): T[] {
  if (findOwner(runs, cell)) return runs;
  return mapRun(runs, id, (cells) => [...cells, cell]);
}

/** pos 위치까지만 남기고 뒤를 잘라냄 (pos 포함) */
export function truncateAfter<T extends Run>(runs: T[], id: string, pos: number): T[] {
  return mapRun(runs, id, (cells) => cells.slice(0, pos + 1));
}

export function removeLast<T extends Run>(runs: T[], id: string): T[] {
  return mapRun(runs, id, (cells) => cells.slice(0, -1));
}

/** 화면 크기가 줄어들면 범위를 벗어난 캐비닛을 배선에서 제거 */
export function pruneRuns<T extends Run>(runs: T[], cols: number, rows: number): T[] {
  return runs.map((r) => ({ ...r, cells: r.cells.filter((c) => c.c < cols && c.r < rows) }));
}
