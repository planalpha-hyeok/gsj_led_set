import type { Cell, Issue, Project } from '../types';
import { generateLines, splitIntoGroups } from './routePath';

export type Phase = 'L1' | 'L2' | 'L3';

export interface PowerCircuit {
  index: number;
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
  issues: Issue[];
}

/** 전원 회로 분배: 회로(= 전원 케이블 1라인)마다 연결할 캐비닛 경로 계산 */
export function buildCircuits(p: Project, cabinetW: number, cabinetA: number): CircuitResult {
  const pw = p.power;
  const issues: Issue[] = [];
  const breakerLimit = pw.breakerA * pw.derate;
  const circuitLimitA = Math.min(breakerLimit, pw.chainLimitA);
  const cabinetsPerCircuit = cabinetA > 0 ? Math.floor(circuitLimitA / cabinetA) : 0;

  if (cabinetsPerCircuit < 1) {
    issues.push({ severity: 'error', message: `캐비닛 1개 전류(${cabinetA.toFixed(2)}A)가 회로 허용 전류(${circuitLimitA.toFixed(1)}A)보다 큽니다` });
    return { circuitLimitA, cabinetsPerCircuit, circuits: [], issues };
  }

  if (pw.chainLimitA < breakerLimit) {
    issues.push({ severity: 'info', message: `회로 용량은 케이블 허용 전류(${pw.chainLimitA}A) 기준으로 제한됨` });
  }

  const lines = generateLines(p.cols, p.rows, pw.route);
  const groups = splitIntoGroups(lines, cabinetsPerCircuit, pw.route.split);
  const circuits: PowerCircuit[] = groups.map((cells, index) => ({
    index,
    cells,
    currentA: cells.length * cabinetA,
    watts: cells.length * cabinetW,
    load: (cells.length * cabinetA) / circuitLimitA,
    phase: 'L1',
  }));

  if (pw.basis === 'avg') {
    issues.push({ severity: 'warning', message: '평균 전력 기준으로 회로를 나눔 → 전체 흰색 화면 등 최대 부하 시 차단기가 떨어질 수 있음' });
  }

  return { circuitLimitA, cabinetsPerCircuit, circuits, issues };
}
