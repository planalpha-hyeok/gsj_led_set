import type { Project } from '../types';
import { cabinetAreaM2 } from './cabinetBuild';

export interface PowerTotals {
  /** 캐비닛 1개 SMPS 정격 출력 합계 (W) */
  cabinetSmpsW: number;
  /** 캐비닛 1개 최대 입력 전력 (W) = SMPS 정격 × 개수 ÷ 효율 */
  cabinetW: number;
  /** 캐비닛 1개 전류 (A, 상전압 기준) */
  cabinetA: number;
  /** 최대 입력 전력 합계 (W) */
  totalW: number;
  /** 피상전력 (kVA) */
  totalKVA: number;
  /** 단상 총 전류 (A) */
  singlePhaseA: number;
  /** 3상 평형 시 선전류 (A) = P / (√3 × V선간 × 역률) */
  threePhaseLineA: number;
}

/**
 * 전력·전류 계산 (항상 최대 전력 기준)
 * 캐비닛 입력 전력
 * - SMPS 기준: SMPS 정격 출력 × 개수 ÷ 효율
 * - ㎡당 W 기준: 사양서 소비전력(W/㎡) × 캐비닛 면적 (이미 입력 전력이므로 효율로 나누지 않음)
 */
export function computePower(p: Project): PowerTotals {
  const { cabinet: cab, power: pw } = p;
  const n = p.cols * p.rows;
  const cabinetSmpsW = cab.smpsW * cab.smpsCount;
  const eff = cab.smpsEfficiency > 0 ? cab.smpsEfficiency : 1;
  const cabinetW = cab.powerMode === 'area' ? cab.powerPerM2 * cabinetAreaM2(cab) : cabinetSmpsW / eff;
  const pf = pw.powerFactor > 0 ? pw.powerFactor : 1;
  const totalW = cabinetW * n;
  return {
    cabinetSmpsW,
    cabinetW,
    cabinetA: cabinetW / (pw.phaseVoltage * pf),
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
