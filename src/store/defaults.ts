import { cabinetFromModules } from '../core/cabinetBuild';
import { findModule } from '../data/modules';
import type { CabinetSpec, DataRun, Phase, PowerRun, Project } from '../types';

export function newId(prefix = 'p'): string {
  return `${prefix}-${Date.now().toString(36)}-${Math.random().toString(36).slice(2, 7)}`;
}

export function newDataRun(unit: number, port: number): DataRun {
  return { id: newId('d'), unit, port, cells: [] };
}

export function newPowerRun(phase: Phase): PowerRun {
  return { id: newId('c'), phase, cells: [] };
}

/** 캐비닛 기본값: 320×160 P1.86 모듈 2×3 (640×480), SMPS 200W × 1개 */
export function defaultCabinet(): CabinetSpec {
  const base: CabinetSpec = {
    id: '', name: '', widthPx: 0, heightPx: 0, widthMm: 0, heightMm: 0,
    smpsW: 200, smpsCount: 1, smpsEfficiency: 0.85,
    powerMode: 'smps', powerPerM2: 0, weightKg: 8,
  };
  return cabinetFromModules(base, findModule('m320-p186')!, 2, 3);
}

/** 새 프로젝트 기본값: 640×480 P1.86 캐비닛 5×3, Colorlight X4m */
export function defaultProject(): Project {
  return {
    version: 2,
    id: newId(),
    name: '새 프로젝트',
    updatedAt: Date.now(),
    cabinet: defaultCabinet(),
    cols: 5,
    rows: 3,
    controllerId: 'cl-x4m',
    receivingCardId: 'cl-k5plus',
    bitDepth: 8,
    frameRate: 60,
    power: {
      system: 'single',
      phaseVoltage: 220,
      lineVoltage: 380,
      powerFactor: 0.95,
      breakerA: 20,
      derate: 0.8,
      chainLimitA: 16,
    },
    dataRuns: [newDataRun(0, 1)],
    powerRuns: [newPowerRun('L1')],
    customControllers: [],
    customCards: [],
  };
}
