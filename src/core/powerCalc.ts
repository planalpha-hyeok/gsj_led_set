import type { Project } from '../types';

export interface PowerTotals {
  /** 계산 기준 캐비닛 1개 전력 (W) */
  cabinetW: number;
  /** 캐비닛 1개 전류 (A, 상전압 기준) */
  cabinetA: number;
  totalMaxW: number;
  totalAvgW: number;
  /** 계산 기준 총 전력 (W) */
  totalW: number;
  /** 피상전력 (kVA) */
  totalKVA: number;
  /** 단상 총 전류 (A) */
  singlePhaseA: number;
  /** 3상 평형 시 선전류 (A) = P / (√3 × V선간 × 역률) */
  threePhaseLineA: number;
}

/** 전력·전류 계산 */
export function computePower(p: Project): PowerTotals {
  const { cabinet: cab, power: pw } = p;
  const n = p.cols * p.rows;
  const cabinetW = pw.basis === 'max' ? cab.powerMaxW : cab.powerAvgW;
  const pf = pw.powerFactor > 0 ? pw.powerFactor : 1;
  const totalW = cabinetW * n;
  return {
    cabinetW,
    cabinetA: cabinetW / (pw.phaseVoltage * pf),
    totalMaxW: cab.powerMaxW * n,
    totalAvgW: cab.powerAvgW * n,
    totalW,
    totalKVA: totalW / pf / 1000,
    singlePhaseA: totalW / (pw.phaseVoltage * pf),
    threePhaseLineA: totalW / (Math.sqrt(3) * pw.lineVoltage * pf),
  };
}

/** 표준 차단기 정격 (A) 중 필요 전류 ÷ 사용률 이상인 최소값 */
const STANDARD_BREAKERS = [16, 20, 30, 32, 40, 50, 60, 63, 75, 100, 125, 150, 175, 200, 225, 250, 300, 400, 500, 630];

export function recommendBreaker(currentA: number, derate: number): number | null {
  const need = currentA / (derate > 0 ? derate : 1);
  return STANDARD_BREAKERS.find((b) => b >= need) ?? null;
}
