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

export interface CircuitResult {
  /** 회로 1개 허용 전류 = min(차단기 × 사용률, 케이블 허용 전류) */
  circuitLimitA: number;
  cabinetsPerCircuit: number;
  circuits: PowerCircuit[];
  unassigned: number;
  issues: Issue[];
}

/** 수동 전원 배선 검사: 회로별 전류와 허용 전류 비교 */
export function buildCircuits(p: Project, cabinetW: number, cabinetA: number): CircuitResult {
  const pw = p.power;
  const issues: Issue[] = [];
  const breakerLimit = pw.breakerA * pw.derate;
  const circuitLimitA = Math.min(breakerLimit, pw.chainLimitA);
  const cabinetsPerCircuit = cabinetA > 0 ? Math.floor(circuitLimitA / cabinetA) : 0;
  const three = pw.system === 'three';
  const grid = gridLayout(p);

  if (cabinetsPerCircuit < 1) {
    issues.push({ severity: 'error', message: `캐비닛 1개 전류(${cabinetA.toFixed(2)}A)가 회로 허용 전류(${circuitLimitA.toFixed(1)}A)보다 큽니다` });
  }
  if (pw.chainLimitA < breakerLimit) {
    issues.push({ severity: 'info', message: `회로 용량은 케이블 허용 전류(${pw.chainLimitA}A) 기준으로 제한됨` });
  }

  const circuits: PowerCircuit[] = p.powerRuns.map((r, index) => {
    const cells = r.cells.filter((c) => inBounds(p, c));
    const watts = cells.reduce((s, c) => s + cellPowerW(p, grid, c, cabinetW), 0);
    const currentA = cabinetW > 0 ? watts * (cabinetA / cabinetW) : 0;
    const label = `C${index + 1}`;
    if (circuitLimitA > 0 && currentA > circuitLimitA) {
      issues.push({ severity: 'error', message: `${label}: 회로 과부하 ${currentA.toFixed(1)}A / ${circuitLimitA.toFixed(1)}A (최대 캐비닛 ${cabinetsPerCircuit}개)` });
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

  return { circuitLimitA, cabinetsPerCircuit, circuits, unassigned: check.unassigned, issues };
}
