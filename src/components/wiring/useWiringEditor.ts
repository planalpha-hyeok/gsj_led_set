import { useRef } from 'react';
import { useProject } from '../../store/ProjectContext';
import { appendCell, findOwner, removeLast, truncateAfter } from '../../store/wiringOps';
import type { Cell, DataRun, PowerRun } from '../../types';
import type { CellInteraction } from '../diagram/ZoomPan';

export type WiringKind = 'data' | 'power';

/**
 * 수동 배선 마우스 동작
 * - 빈 캐비닛 클릭/드래그: 편집 중인 배선 끝에 차례로 연결
 * - 드래그 중 바로 전 캐비닛으로 되돌아가면: 마지막 연결 취소
 * - 편집 중인 배선의 마지막 캐비닛 클릭: 연결 취소
 * - 편집 중인 배선의 중간 캐비닛 클릭: 그 뒤를 잘라내고 거기서부터 다시 연결
 * - 다른 배선의 캐비닛 클릭: 그 배선을 편집 대상으로 선택
 * 드래그 한 번은 실행 취소 한 단계로 묶인다.
 */
export function useWiringEditor(kind: WiringKind, activeId: string | undefined, setActiveId: (id: string) => void): CellInteraction {
  const { project, updateDataRuns, updatePowerRuns } = useProject();
  const runs: (DataRun | PowerRun)[] = kind === 'data' ? project.dataRuns : project.powerRuns;
  const state = useRef({ mode: 'none' as 'none' | 'add', key: '', stroke: 0, active: activeId });
  state.current.active = activeId;

  const edit = (fn: (r: (DataRun | PowerRun)[]) => (DataRun | PowerRun)[]) => {
    const key = state.current.key;
    if (kind === 'data') updateDataRuns((r) => fn(r) as DataRun[], key);
    else updatePowerRuns((r) => fn(r) as PowerRun[], key);
  };

  return {
    onDown: (cell: Cell) => {
      const s = state.current;
      s.stroke += 1;
      s.key = `${kind}-stroke-${s.stroke}`;
      s.mode = 'none';
      const owner = findOwner(runs, cell);
      const active = runs.find((r) => r.id === s.active);

      if (owner && owner.run.id !== s.active) {
        setActiveId(owner.run.id);
        return true;
      }
      if (!active) return true;
      const id = active.id;

      if (owner) {
        if (owner.pos === active.cells.length - 1) {
          edit((r) => removeLast(r, id));
        } else {
          edit((r) => truncateAfter(r, id, owner.pos));
          s.mode = 'add';
        }
        return true;
      }
      edit((r) => appendCell(r, id, cell));
      s.mode = 'add';
      return true;
    },
    onMove: (cell: Cell) => {
      const s = state.current;
      const id = s.active;
      if (s.mode !== 'add' || !id) return;
      edit((r) => {
        const run = r.find((x) => x.id === id);
        if (!run) return r;
        const prev = run.cells[run.cells.length - 2];
        if (prev && prev.c === cell.c && prev.r === cell.r) return removeLast(r, id);
        return appendCell(r, id, cell);
      });
    },
    onUp: () => {
      state.current.mode = 'none';
    },
  };
}
