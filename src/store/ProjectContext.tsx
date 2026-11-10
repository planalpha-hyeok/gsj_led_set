import { createContext, useCallback, useContext, useEffect, useMemo, useReducer, type ReactNode } from 'react';
import { analyze, type Analysis } from '../core/analyze';
import type { CabinetSpec, DataRun, PowerRun, PowerSettings, Project } from '../types';
import { loadInitialProject, saveProject } from './persistence';
import { pruneRuns } from './wiringOps';

/**
 * 프로젝트 상태 + 실행 취소/다시 실행
 * coalesceKey가 같은 연속 변경(예: 드래그 한 번, 이름 입력)은 1.5초 안이면 한 단계로 묶는다.
 */
const HISTORY_LIMIT = 100;
const COALESCE_MS = 1500;

interface State {
  project: Project;
  past: Project[];
  future: Project[];
  lastKey: string | null;
  lastTime: number;
}

type Action =
  | { type: 'apply'; fn: (p: Project) => Project; key?: string }
  | { type: 'load'; project: Project }
  | { type: 'undo' }
  | { type: 'redo' };

function reducer(s: State, a: Action): State {
  switch (a.type) {
    case 'apply': {
      const next = a.fn(s.project);
      if (next === s.project) return s;
      const now = Date.now();
      const merge = a.key !== undefined && a.key === s.lastKey && now - s.lastTime < COALESCE_MS;
      return {
        project: { ...next, updatedAt: now },
        past: merge ? s.past : [...s.past, s.project].slice(-HISTORY_LIMIT),
        future: [],
        lastKey: a.key ?? null,
        lastTime: now,
      };
    }
    case 'load':
      return { project: a.project, past: [], future: [], lastKey: null, lastTime: 0 };
    case 'undo': {
      if (!s.past.length) return s;
      return { project: s.past[s.past.length - 1], past: s.past.slice(0, -1), future: [s.project, ...s.future], lastKey: null, lastTime: 0 };
    }
    case 'redo': {
      if (!s.future.length) return s;
      return { project: s.future[0], past: [...s.past, s.project], future: s.future.slice(1), lastKey: null, lastTime: 0 };
    }
  }
}

interface ProjectStore {
  project: Project;
  analysis: Analysis;
  canUndo: boolean;
  canRedo: boolean;
  undo: () => void;
  redo: () => void;
  setProject: (p: Project) => void;
  update: (patch: Partial<Project>, key?: string) => void;
  updateCabinet: (patch: Partial<CabinetSpec>) => void;
  updatePower: (patch: Partial<PowerSettings>) => void;
  updateDataRuns: (fn: (runs: DataRun[]) => DataRun[], key?: string) => void;
  updatePowerRuns: (fn: (runs: PowerRun[]) => PowerRun[], key?: string) => void;
}

const Ctx = createContext<ProjectStore | null>(null);

export function ProjectProvider({ children }: { children: ReactNode }) {
  const [state, dispatch] = useReducer(reducer, undefined, () => ({
    project: loadInitialProject(), past: [], future: [], lastKey: null, lastTime: 0,
  }));
  const { project } = state;

  // 변경 후 0.5초 뒤 자동 저장
  useEffect(() => {
    const t = setTimeout(() => saveProject(project), 500);
    return () => clearTimeout(t);
  }, [project]);

  const apply = useCallback((fn: (p: Project) => Project, key?: string) => dispatch({ type: 'apply', fn, key }), []);

  // Ctrl+Z / Ctrl+Shift+Z / Ctrl+Y (입력 칸에서는 브라우저 기본 동작 유지)
  useEffect(() => {
    const onKey = (e: KeyboardEvent) => {
      const el = e.target as HTMLElement;
      if (el.closest('input, textarea, select')) return;
      if (!(e.ctrlKey || e.metaKey)) return;
      const k = e.key.toLowerCase();
      if (k === 'z' && !e.shiftKey) { e.preventDefault(); dispatch({ type: 'undo' }); }
      else if ((k === 'z' && e.shiftKey) || k === 'y') { e.preventDefault(); dispatch({ type: 'redo' }); }
    };
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  }, []);

  const analysis = useMemo(() => analyze(project), [project]);

  const store = useMemo<ProjectStore>(() => ({
    project,
    analysis,
    canUndo: state.past.length > 0,
    canRedo: state.future.length > 0,
    undo: () => dispatch({ type: 'undo' }),
    redo: () => dispatch({ type: 'redo' }),
    setProject: (p) => dispatch({ type: 'load', project: p }),
    update: (patch, key) => apply((p) => {
      const next = { ...p, ...patch };
      if (patch.cols !== undefined || patch.rows !== undefined) {
        next.dataRuns = pruneRuns(next.dataRuns, next.cols, next.rows);
        next.powerRuns = pruneRuns(next.powerRuns, next.cols, next.rows);
      }
      return next;
    }, key),
    updateCabinet: (patch) => apply((p) => ({ ...p, cabinet: { ...p.cabinet, ...patch } })),
    updatePower: (patch) => apply((p) => ({ ...p, power: { ...p.power, ...patch } })),
    updateDataRuns: (fn, key) => apply((p) => {
      const runs = fn(p.dataRuns);
      return runs === p.dataRuns ? p : { ...p, dataRuns: runs };
    }, key),
    updatePowerRuns: (fn, key) => apply((p) => {
      const runs = fn(p.powerRuns);
      return runs === p.powerRuns ? p : { ...p, powerRuns: runs };
    }, key),
  }), [project, analysis, state.past.length, state.future.length, apply]);

  return <Ctx.Provider value={store}>{children}</Ctx.Provider>;
}

export function useProject(): ProjectStore {
  const v = useContext(Ctx);
  if (!v) throw new Error('ProjectProvider 밖에서 useProject 사용');
  return v;
}
