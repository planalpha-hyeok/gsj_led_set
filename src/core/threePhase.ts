import type { Issue } from '../types';
import type { Phase, PowerCircuit } from './powerCircuit';

export interface PhaseLoad {
  phase: Phase;
  currentA: number;
  watts: number;
  circuits: number;
}

export interface PhaseResult {
  circuits: PowerCircuit[];
  phases: PhaseLoad[];
  /** 불평형률 (%) = (최대 - 최소) / 평균 × 100 */
  imbalancePct: number;
  issues: Issue[];
}

const PHASES: Phase[] = ['L1', 'L2', 'L3'];

/**
 * 3상 분배: 회로를 R(L1)/S(L2)/T(L3)에 배정
 * 회로 순서대로, 현재 전류가 가장 적은 상에 배정한다 (동률이면 L1→L2→L3 순).
 */
export function assignPhases(circuits: PowerCircuit[], threePhase: boolean): PhaseResult {
  const issues: Issue[] = [];
  const loads = new Map<Phase, PhaseLoad>(PHASES.map((ph) => [ph, { phase: ph, currentA: 0, watts: 0, circuits: 0 }]));

  const assigned = circuits.map((c) => {
    let phase: Phase = 'L1';
    if (threePhase) {
      phase = PHASES.reduce((best, ph) => (loads.get(ph)!.currentA < loads.get(best)!.currentA - 1e-9 ? ph : best), 'L1' as Phase);
    }
    const l = loads.get(phase)!;
    l.currentA += c.currentA;
    l.watts += c.watts;
    l.circuits += 1;
    return { ...c, phase };
  });

  const phases = threePhase ? PHASES.map((ph) => loads.get(ph)!) : [loads.get('L1')!];
  let imbalancePct = 0;
  if (threePhase) {
    const cur = phases.map((x) => x.currentA);
    const avg = cur.reduce((a, b) => a + b, 0) / 3;
    imbalancePct = avg > 0 ? ((Math.max(...cur) - Math.min(...cur)) / avg) * 100 : 0;
    if (imbalancePct > 15) {
      issues.push({ severity: 'warning', message: `3상 불평형률 ${imbalancePct.toFixed(1)}% (권장 15% 이하) → 회로 배치 조정 필요` });
    }
    if (circuits.length < 3) {
      issues.push({ severity: 'info', message: `회로가 ${circuits.length}개뿐이라 3상에 고르게 나눌 수 없음` });
    }
  }
  return { circuits: assigned, phases, imbalancePct, issues };
}
