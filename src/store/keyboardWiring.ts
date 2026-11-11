import type { Cell } from '../types';
import { appendCell, findOwner, mapRun, removeLast, truncateAfter } from './wiringOps';

/**
 * 키보드 배선 (데이터 포트·전원 회로 공용 순수 함수)
 *
 * 스페이스
 *  - 배선 중: 끝
 *  - 빈 캐비닛: 시작 (선택된 배선이 비어 있으면 거기에, 아니면 새 배선을 만들어서)
 *  - 배선의 마지막 캐비닛: 이어서 그리기
 *  - 배선의 시작 캐비닛: 그 배선의 캐비닛 전체 삭제 (배선 자체는 남김)
 *  - 배선의 중간 캐비닛: 그 캐비닛부터 끝까지 삭제
 * 방향키: 커서 이동. 배선 중이면 이동하면서 연결, 바로 전 캐비닛으로 돌아가면 마지막 연결 취소
 * Shift+방향키: 같은 동작을 더 갈 수 없을 때까지 반복
 * Backspace: 마지막 캐비닛 1개 연결 취소
 */
interface Run {
  id: string;
  cells: Cell[];
}

export interface KbState {
  cursor: Cell;
  drawing: boolean;
  activeId?: string;
}

export interface KbResult<T extends Run> {
  runs: T[];
  state: KbState;
}

const same = (a: Cell | undefined, b: Cell) => !!a && a.c === b.c && a.r === b.r;

export function pressSpace<T extends Run>(runs: T[], s: KbState, makeRun: (runs: T[]) => T): KbResult<T> {
  if (s.drawing) return { runs, state: { ...s, drawing: false } };

  const owner = findOwner(runs, s.cursor);
  if (owner) {
    const { run, pos } = owner;
    const last = run.cells.length - 1;
    if (pos === 0) {
      return { runs: mapRun(runs, run.id, () => []), state: { ...s, activeId: run.id, drawing: false } };
    }
    if (pos === last) {
      return { runs, state: { ...s, activeId: run.id, drawing: true } };
    }
    return { runs: truncateAfter(runs, run.id, pos - 1), state: { ...s, activeId: run.id, drawing: false } };
  }

  let next = runs;
  let active = runs.find((r) => r.id === s.activeId);
  if (!active || active.cells.length > 0) {
    active = makeRun(runs);
    next = [...runs, active];
  }
  return { runs: appendCell(next, active.id, s.cursor), state: { ...s, activeId: active.id, drawing: true } };
}

export function pressArrow<T extends Run>(
  runs: T[], s: KbState, dc: number, dr: number, toEdge: boolean, cols: number, rows: number,
): KbResult<T> {
  let cur = runs;
  let state = s;
  for (;;) {
    const target = { c: state.cursor.c + dc, r: state.cursor.r + dr };
    if (target.c < 0 || target.r < 0 || target.c >= cols || target.r >= rows) break;

    if (!state.drawing) {
      state = { ...state, cursor: target };
    } else {
      const run = cur.find((r) => r.id === state.activeId);
      if (!run) break;
      if (same(run.cells[run.cells.length - 2], target)) {
        cur = removeLast(cur, run.id);
      } else if (!findOwner(cur, target)) {
        cur = appendCell(cur, run.id, target);
      } else {
        break; // 이미 연결된 캐비닛 → 멈춤
      }
      state = { ...state, cursor: target };
    }
    if (!toEdge) break;
  }
  return { runs: cur, state };
}

export function pressBackspace<T extends Run>(runs: T[], s: KbState): KbResult<T> {
  const run = runs.find((r) => r.id === s.activeId);
  if (!run || run.cells.length === 0) return { runs, state: s };
  const next = removeLast(runs, run.id);
  const remain = run.cells.slice(0, -1);
  return {
    runs: next,
    state: { ...s, cursor: remain[remain.length - 1] ?? s.cursor, drawing: remain.length > 0 && s.drawing },
  };
}
