import { newId } from '../store/defaults';
import { normalizeProject } from '../store/persistence';
import type { Project } from '../types';
import { downloadBlob, safeName } from './download';

/** 프로젝트를 JSON 파일로 저장 */
export function exportProjectJson(p: Project): void {
  const blob = new Blob([JSON.stringify(p, null, 2)], { type: 'application/json' });
  downloadBlob(blob, `${safeName(p.name)}.ledproj.json`);
}

/** JSON 파일에서 프로젝트 불러오기 (새 id 부여 → 기존 프로젝트를 덮어쓰지 않음) */
export async function importProjectJson(file: File): Promise<Project> {
  const text = await file.text();
  let raw: unknown;
  try {
    raw = JSON.parse(text);
  } catch {
    throw new Error('JSON 형식이 아닙니다');
  }
  if (!raw || typeof raw !== 'object' || !('cabinet' in raw) || !('cols' in raw)) {
    throw new Error('LED 프로젝트 파일이 아닙니다');
  }
  return { ...normalizeProject(raw as Partial<Project>), id: newId(), updatedAt: Date.now() };
}
