import { useEffect, useRef, useState } from 'react';
import { nextFreePort } from '../../core/dataMapping';
import { suggestPhase } from '../../core/threePhase';
import { newDataRun, newPowerRun } from '../../store/defaults';
import { pressArrow, pressBackspace, pressSpace, type KbResult, type KbState } from '../../store/keyboardWiring';
import { useProject } from '../../store/ProjectContext';
import type { Cell, DataRun, PowerRun } from '../../types';
import type { WiringKind } from './useWiringEditor';

type AnyRun = DataRun | PowerRun;

export interface KeyboardWiring {
  cursor: Cell;
  drawing: boolean;
  /** 마우스로 캐비닛을 누르면 커서도 그 위치로 (배선 중이면 끝냄) */
  setCursor: (cell: Cell) => void;
}

const ARROWS: Record<string, [number, number]> = {
  ArrowUp: [0, -1], ArrowDown: [0, 1], ArrowLeft: [-1, 0], ArrowRight: [1, 0],
};

/**
 * 키보드 배선 (화면 연결부)
 * 스페이스: 시작/끝/지우기 · 방향키: 이동/연결 · Shift+방향키: 끝까지
 * Backspace: 하나 취소 · Esc: 끝 · Tab/Shift+Tab: 배선 선택 · N: 새 배선
 */
export function useKeyboardWiring(
  kind: WiringKind, enabled: boolean, activeId: string | undefined, setActiveId: (id: string) => void,
): KeyboardWiring {
  const { project: p, analysis: a, updateDataRuns, updatePowerRuns } = useProject();
  const runs: AnyRun[] = kind === 'data' ? p.dataRuns : p.powerRuns;
  const [kb, setKb] = useState<{ cursor: Cell; drawing: boolean }>({ cursor: { c: 0, r: 0 }, drawing: false });

  // 화면 크기가 줄면 커서를 범위 안으로
  const cursor = { c: Math.min(kb.cursor.c, p.cols - 1), r: Math.min(kb.cursor.r, p.rows - 1) };
  const activeExists = runs.some((r) => r.id === activeId);
  const drawing = kb.drawing && activeExists;

  // 키 입력이 빠르게 이어져도 최신 값으로 계산하도록 ref에 보관
  const latest = useRef({ runs, state: { cursor, drawing, activeId } as KbState });
  latest.current = { runs, state: { cursor, drawing, activeId } };
  const ctx = useRef({ p, a, setActiveId });
  ctx.current = { p, a, setActiveId };

  const commit = (res: KbResult<AnyRun>) => {
    const prev = latest.current;
    if (res.runs !== prev.runs) {
      const next = res.runs;
      if (kind === 'data') updateDataRuns(() => next as DataRun[]);
      else updatePowerRuns(() => next as PowerRun[]);
    }
    latest.current = { runs: res.runs, state: res.state };
    setKb({ cursor: res.state.cursor, drawing: res.state.drawing });
    if (res.state.activeId && res.state.activeId !== prev.state.activeId) ctx.current.setActiveId(res.state.activeId);
  };

  const makeRun = (cur: AnyRun[]): AnyRun => {
    const { p: proj, a: an } = ctx.current;
    if (kind === 'data') {
      const { unit, port } = nextFreePort({ ...proj, dataRuns: cur as DataRun[] }, an.controller?.ports ?? 1);
      return newDataRun(unit, port);
    }
    return newPowerRun(suggestPhase(an.phases.phases));
  };

  useEffect(() => {
    if (!enabled) return;
    const onKey = (e: KeyboardEvent) => {
      const el = e.target as HTMLElement;
      if (el.closest('input, select, textarea') || document.querySelector('.modal-backdrop')) return;
      if (e.ctrlKey || e.metaKey || e.altKey) return;
      const { runs: cur, state: s } = latest.current;
      const { cols, rows } = ctx.current.p;
      let res: KbResult<AnyRun> | null = null;

      if (e.code === 'Space') res = pressSpace(cur, s, makeRun);
      else if (ARROWS[e.key]) res = pressArrow(cur, s, ARROWS[e.key][0], ARROWS[e.key][1], e.shiftKey, cols, rows);
      else if (e.key === 'Backspace') res = pressBackspace(cur, s);
      else if (e.key === 'Escape') res = { runs: cur, state: { ...s, drawing: false } };
      else if (e.key === 'Tab' && cur.length > 0) {
        const idx = cur.findIndex((r) => r.id === s.activeId);
        const next = cur[(idx + (e.shiftKey ? -1 : 1) + cur.length) % cur.length];
        const lastCell = next.cells[next.cells.length - 1];
        res = { runs: cur, state: { cursor: lastCell ?? s.cursor, drawing: false, activeId: next.id } };
      } else if (e.code === 'KeyN' && !e.shiftKey) {
        const run = makeRun(cur);
        res = { runs: [...cur, run], state: { ...s, drawing: false, activeId: run.id } };
      }
      if (!res) return;

      e.preventDefault();
      // 버튼에 포커스가 있으면 스페이스가 버튼을 누르지 않도록 해제
      if (el instanceof HTMLButtonElement) el.blur();
      commit(res);
    };
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  }, [enabled, kind]);

  return {
    cursor,
    drawing,
    setCursor: (cell) => setKb({ cursor: cell, drawing: false }),
  };
}
