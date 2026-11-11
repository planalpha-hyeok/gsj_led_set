import type { Analysis } from '../../core/analyze';
import type { Project } from '../../types';
import { computeGeometry } from './geometry';
import { legendHeight } from './Legend';

export type ViewKind = 'screen' | 'mapping' | 'power';

/** 배선도 SVG 크기 (각 Diagram 컴포넌트의 계산과 동일) */
export function diagramSize(kind: ViewKind, p: Project, a: Analysis): { width: number; height: number } {
  const g0 = computeGeometry(p);
  if (kind === 'screen') return { width: g0.width, height: g0.height };
  const count = kind === 'mapping' ? (a.data?.ports.length ?? 0) : a.circuits.circuits.length;
  const g = computeGeometry(p, legendHeight(count, g0.width));
  return { width: g.width, height: g.height };
}
