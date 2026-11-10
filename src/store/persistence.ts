import type { Project } from '../types';
import { defaultProject } from './defaults';

/**
 * 브라우저 내부 저장 (localStorage)
 * 브라우저 데이터를 지우면 사라지므로, 중요한 프로젝트는 JSON 파일로 내보내서 보관
 */
const KEY_PROJECTS = 'gsj-led:projects';
const KEY_CURRENT = 'gsj-led:current';

function readAll(): Record<string, Project> {
  try {
    const raw = localStorage.getItem(KEY_PROJECTS);
    return raw ? (JSON.parse(raw) as Record<string, Project>) : {};
  } catch {
    return {};
  }
}

function writeAll(all: Record<string, Project>): void {
  try {
    localStorage.setItem(KEY_PROJECTS, JSON.stringify(all));
  } catch {
    /* 저장 불가 (사생활 보호 모드 등) → 무시 */
  }
}

export function listProjects(): Project[] {
  return Object.values(readAll()).sort((a, b) => b.updatedAt - a.updatedAt);
}

export function saveProject(p: Project): void {
  const all = readAll();
  all[p.id] = p;
  writeAll(all);
  try {
    localStorage.setItem(KEY_CURRENT, p.id);
  } catch {
    /* 무시 */
  }
}

export function deleteProject(id: string): void {
  const all = readAll();
  delete all[id];
  writeAll(all);
}

export function loadInitialProject(): Project {
  try {
    const id = localStorage.getItem(KEY_CURRENT);
    const all = readAll();
    if (id && all[id]) return normalizeProject(all[id]);
  } catch {
    /* 무시 */
  }
  return defaultProject();
}

/**
 * 불러온 데이터에 빠진 항목이 있으면 기본값으로 채움 (이전 버전 파일 호환)
 * 버전 1(자동 경로) 파일은 배선 정보가 없으므로 빈 포트/회로 1개로 시작
 */
export function normalizeProject(raw: Partial<Project>): Project {
  const d = defaultProject();
  const power = { ...d.power, ...raw.power };
  delete (power as { route?: unknown }).route;
  const p: Project = {
    ...d,
    ...raw,
    cabinet: { ...d.cabinet, ...raw.cabinet },
    power,
    dataRuns: Array.isArray(raw.dataRuns) ? raw.dataRuns : d.dataRuns,
    powerRuns: Array.isArray(raw.powerRuns) ? raw.powerRuns : d.powerRuns,
    customControllers: raw.customControllers ?? [],
    customCards: raw.customCards ?? [],
    version: 2,
  };
  delete (p as { mapping?: unknown }).mapping;
  return p;
}
