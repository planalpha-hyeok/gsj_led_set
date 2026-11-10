import { createContext, useCallback, useContext, useEffect, useMemo, useState, type ReactNode } from 'react';
import { analyze, type Analysis } from '../core/analyze';
import type { CabinetSpec, PowerSettings, Project, RouteSettings } from '../types';
import { loadInitialProject, saveProject } from './persistence';

interface ProjectStore {
  project: Project;
  analysis: Analysis;
  setProject: (p: Project) => void;
  update: (patch: Partial<Project>) => void;
  updateCabinet: (patch: Partial<CabinetSpec>) => void;
  updateMapping: (patch: Partial<RouteSettings>) => void;
  updatePower: (patch: Partial<PowerSettings>) => void;
  updatePowerRoute: (patch: Partial<RouteSettings>) => void;
}

const Ctx = createContext<ProjectStore | null>(null);

export function ProjectProvider({ children }: { children: ReactNode }) {
  const [project, setProjectState] = useState<Project>(loadInitialProject);

  // 변경 후 0.5초 뒤 자동 저장
  useEffect(() => {
    const t = setTimeout(() => saveProject(project), 500);
    return () => clearTimeout(t);
  }, [project]);

  const apply = useCallback((fn: (p: Project) => Project) => {
    setProjectState((p) => ({ ...fn(p), updatedAt: Date.now() }));
  }, []);

  const store = useMemo<ProjectStore>(() => ({
    project,
    analysis: analyze(project),
    setProject: (p) => setProjectState(p),
    update: (patch) => apply((p) => ({ ...p, ...patch })),
    updateCabinet: (patch) => apply((p) => ({ ...p, cabinet: { ...p.cabinet, ...patch } })),
    updateMapping: (patch) => apply((p) => ({ ...p, mapping: { ...p.mapping, ...patch } })),
    updatePower: (patch) => apply((p) => ({ ...p, power: { ...p.power, ...patch } })),
    updatePowerRoute: (patch) => apply((p) => ({ ...p, power: { ...p.power, route: { ...p.power.route, ...patch } } })),
  }), [project, apply]);

  return <Ctx.Provider value={store}>{children}</Ctx.Provider>;
}

export function useProject(): ProjectStore {
  const v = useContext(Ctx);
  if (!v) throw new Error('ProjectProvider 밖에서 useProject 사용');
  return v;
}
