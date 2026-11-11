import type { Issue, Phase } from '../types';
import type { PowerCircuit } from './powerCircuit';

export interface PhaseLoad {
  phase: Phase;
  currentA: number;
  watts: number;
  circuits: number;
}

export interface PhaseResult {
  phases: PhaseLoad[];
  /** 불평형률 (%) = (최대 - 최소) / 평균 × 100 */
  imbalancePct: number;
  issues: Issue[];
}

export const PHASES: Phase[] = ['L1', 'L2', 'L3'];

/** 상별 부하 합계와 불평형률 (상 배정은 사용자가 회로마다 직접 선택) */
export function computePhaseLoads(circuits: PowerCircuit[], threePhase: boolean): PhaseResult {
  const issues: Issue[] = [];
  const loads = new Map<Phase, PhaseLoad>(PHASES.map((ph) => [ph, { phase: ph, currentA: 0, watts: 0, circuits: 0 }]));
  for (const c of circuits) {
    const l = loads.get(threePhase ? c.phase : 'L1')!;
    l.currentA += c.currentA;
    l.watts += c.watts;
    l.circuits += 1;
  }

  const phases = threePhase ? PHASES.map((ph) => loads.get(ph)!) : [loads.get('L1')!];
  let imbalancePct = 0;
  if (threePhase) {
    const cur = phases.map((x) => x.currentA);
    const avg = cur.reduce((a, b) => a + b, 0) / 3;
    imbalancePct = avg > 0 ? ((Math.max(...cur) - Math.min(...cur)) / avg) * 100 : 0;
    if (imbalancePct > 15) {
      issues.push({ severity: 'warning', message: `3상 불평형률 ${imbalancePct.toFixed(1)}% (권장 15% 이하) → 회로의 상 배정 조정 필요` });
    }
  }
  return { phases, imbalancePct, issues };
}

/** 새 회로에 추천할 상: 전류가 가장 적은 상 → 회로 수가 적은 상 → L1·L2·L3 순 */
export function suggestPhase(phases: PhaseLoad[]): Phase {
  if (phases.length < 3) return 'L1';
  return phases.reduce((best, x) => {
    if (x.currentA < best.currentA - 1e-9) return x;
    if (Math.abs(x.currentA - best.currentA) <= 1e-9 && x.circuits < best.circuits) return x;
    return best;
  }).phase;
}
