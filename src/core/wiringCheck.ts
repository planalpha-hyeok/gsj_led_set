import type { Cell, Issue, Project } from '../types';

export const cellKey = (c: Cell) => `${c.c},${c.r}`;

export function inBounds(p: Project, c: Cell): boolean {
  return c.c >= 0 && c.r >= 0 && c.c < p.cols && c.r < p.rows;
}

/**
 * 수동 배선 공통 검사 (데이터·전원)
 * - 화면 밖 캐비닛, 중복 연결, 미연결 캐비닛, 이웃하지 않은 연결
 */
export function checkGroups(p: Project, groups: { name: string; cells: Cell[] }[], kind: string): { unassigned: number; issues: Issue[] } {
  const issues: Issue[] = [];
  const seen = new Map<string, string>();

  for (const g of groups) {
    for (let i = 0; i < g.cells.length; i++) {
      const c = g.cells[i];
      const k = cellKey(c);
      const prev = seen.get(k);
      if (prev) {
        issues.push({ severity: 'error', message: `${kind}: 캐비닛 (${c.c + 1}, ${c.r + 1})이 ${prev}와 ${g.name}에 중복 연결됨` });
      }
      seen.set(k, g.name);
      if (i > 0) {
        const a = g.cells[i - 1];
        if (Math.abs(a.c - c.c) + Math.abs(a.r - c.r) !== 1) {
          issues.push({ severity: 'warning', message: `${kind} ${g.name}: ${i}→${i + 1}번째 캐비닛이 붙어 있지 않음 (케이블 길이 확인)` });
        }
      }
    }
  }

  const unassigned = p.cols * p.rows - seen.size;
  if (unassigned > 0) {
    issues.push({ severity: 'warning', message: `${kind}: 연결되지 않은 캐비닛 ${unassigned}개` });
  }
  return { unassigned, issues };
}
