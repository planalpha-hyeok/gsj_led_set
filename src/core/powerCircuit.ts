import type { Cell, Issue, Phase, Project } from '../types';
import { checkGroups, inBounds } from './wiringCheck';
import { gridLayout } from './gridLayout';
import { cellPowerW } from './powerCalc';

export interface PowerCircuit {
  index: number;
  id: string;
  label: string;
  cells: Cell[];
  currentA: number;
  watts: number;
  /** 회로 허용 전류 대비 사용률 (0~1) */
  load: number;
  phase: Phase;
}

export type LimitBy = 'breaker' | 'cable' | 'lineW';

export interface CircuitResult {
  /** 회로 1개 허용 전류 = min(차단기 × 사용률, 케이블 허용 전류, 한 라인 최대 W를 전류로 환산) */
  circuitLimitA: number;
  /** 같은 한도를 전력(W)으로 표시 = 허용 전류 × 상전압 × 역률 */
  circuitLimitW: number;
  /** 가장 엄격해서 실제로 적용된 기준 */
  limitBy: LimitBy;
  cabinetsPerCircuit: number;
  circuits: PowerCircuit[];
  unassigned: number;
  issues: Issue[];
}

export const limitByLabel: Record<LimitBy, string> = {
  breaker: '차단기',
  cable: '케이블',
  lineW: '한 라인 최대 W',
};

/**
 * 수동 전원 배선 검사: 회로별 전류와 허용 전류 비교
 * 허용 한도는 세 기준 중 가장 엄격한 값
 * - 차단기 정격 × 사용률 (A)
 * - 케이블·커넥터 허용 전류 (A)
 * - 한 라인 최대 전력 (W) → 전류로 환산: W ÷ (상전압 × 역률)
 */
export function buildCircuits(p: Project, cabinetW: number, cabinetA: number): CircuitResult {
  const pw = p.power;
  const issues: Issue[] = [];
  const vpf = pw.phaseVoltage * (pw.powerFactor > 0 ? pw.powerFactor : 1);
  const limits: { by: LimitBy; a: number }[] = [
    { by: 'breaker', a: pw.breakerA * pw.derate },
    { by: 'cable', a: pw.chainLimitA },
  ];
  if (pw.lineLimitW > 0 && vpf > 0) limits.push({ by: 'lineW', a: pw.lineLimitW / vpf });
  const strict = limits.reduce((m, x) => (x.a < m.a ? x : m));
  const circuitLimitA = strict.a;
  const circuitLimitW = circuitLimitA * vpf;
  const limitBy = strict.by;
  const cabinetsPerCircuit = cabinetA > 0 ? Math.floor(circuitLimitA / cabinetA) : 0;
  const three = pw.system === 'three';
  const grid = gridLayout(p);

  if (cabinetsPerCircuit < 1) {
    issues.push({ severity: 'error', message: `캐비닛 1개 전류(${cabinetA.toFixed(2)}A)가 회로 허용 전류(${circuitLimitA.toFixed(1)}A)보다 큽니다` });
  }
  if (limitBy !== 'breaker') {
    issues.push({ severity: 'info', message: `회로 용량은 ${limitByLabel[limitBy]} 기준으로 제한됨 (${circuitLimitA.toFixed(1)}A ≈ ${Math.round(circuitLimitW).toLocaleString()}W)` });
  }

  const circuits: PowerCircuit[] = p.powerRuns.map((r, index) => {
    const cells = r.cells.filter((c) => inBounds(p, c));
    const watts = cells.reduce((s, c) => s + cellPowerW(p, grid, c, cabinetW), 0);
    const currentA = cabinetW > 0 ? watts * (cabinetA / cabinetW) : 0;
    const label = `C${index + 1}`;
    if (circuitLimitA > 0 && currentA > circuitLimitA) {
      issues.push({ severity: 'error', message: `${label}: 회로 과부하 ${Math.round(watts).toLocaleString()}W · ${currentA.toFixed(1)}A / 한도 ${Math.round(circuitLimitW).toLocaleString()}W · ${circuitLimitA.toFixed(1)}A (${limitByLabel[limitBy]} 기준, 최대 캐비닛 ${cabinetsPerCircuit}개)` });
    }
    return {
      index, id: r.id, label, cells, currentA,
      watts,
      load: circuitLimitA > 0 ? currentA / circuitLimitA : 0,
      phase: three ? r.phase : 'L1',
    };
  });

  const check = checkGroups(p, circuits.map((c) => ({ name: c.label, cells: c.cells })), '전원');
  issues.push(...check.issues);

  return { circuitLimitA, circuitLimitW, limitBy, cabinetsPerCircuit, circuits, unassigned: check.unassigned, issues };
}
