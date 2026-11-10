import { cabinetTemplates } from '../data/cabinets';
import type { DataRun, Phase, PowerRun, Project } from '../types';

export function newId(prefix = 'p'): string {
  return `${prefix}-${Date.now().toString(36)}-${Math.random().toString(36).slice(2, 7)}`;
}

export function newDataRun(unit: number, port: number): DataRun {
  return { id: newId('d'), unit, port, cells: [] };
}

export function newPowerRun(phase: Phase): PowerRun {
  return { id: newId('c'), phase, cells: [] };
}

/** 새 프로젝트 기본값: 640×480 P1.86 캐비닛 5×3, Colorlight X4m */
export function defaultProject(): Project {
  const tpl = cabinetTemplates.find((c) => c.id === 'tpl-640480-p186')!;
  return {
    version: 2,
    id: newId(),
    name: '새 프로젝트',
    updatedAt: Date.now(),
    cabinet: { ...tpl },
    cols: 5,
    rows: 3,
    controllerId: 'cl-x4m',
    receivingCardId: 'cl-e120',
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
      basis: 'max',
    },
    dataRuns: [newDataRun(0, 1)],
    powerRuns: [newPowerRun('L1')],
    customControllers: [],
    customCards: [],
  };
}
