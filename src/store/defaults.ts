import { cabinetTemplates } from '../data/cabinets';
import type { Project } from '../types';

export function newId(prefix = 'p'): string {
  return `${prefix}-${Date.now().toString(36)}-${Math.random().toString(36).slice(2, 7)}`;
}

/** 새 프로젝트 기본값 */
export function defaultProject(): Project {
  const tpl = cabinetTemplates.find((c) => c.id === 'tpl-500-p39')!;
  return {
    version: 1,
    id: newId(),
    name: '새 프로젝트',
    updatedAt: Date.now(),
    cabinet: { ...tpl },
    cols: 12,
    rows: 6,
    controllerId: 'ns-vx1000',
    receivingCardId: 'ns-a8spro',
    bitDepth: 8,
    frameRate: 60,
    mapping: { start: 'TL', direction: 'vertical', pattern: 'serpentine', split: 'line' },
    power: {
      system: 'single',
      phaseVoltage: 220,
      lineVoltage: 380,
      powerFactor: 0.95,
      breakerA: 20,
      derate: 0.8,
      chainLimitA: 16,
      basis: 'max',
      route: { start: 'BL', direction: 'vertical', pattern: 'serpentine', split: 'line' },
    },
    customControllers: [],
    customCards: [],
  };
}
