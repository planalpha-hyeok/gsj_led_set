import { describe, expect, it } from 'vitest';
import { defaultProject } from '../store/defaults';
import { analyze } from './analyze';
import { effectivePortCapacity } from './portCapacity';
import { generateLines, splitIntoGroups } from './routePath';
import { assignPhases } from './threePhase';
import type { ControllerSpec } from '../types';

describe('generateLines', () => {
  it('좌상단 가로 S자', () => {
    const l = generateLines(3, 2, { start: 'TL', direction: 'horizontal', pattern: 'serpentine' });
    expect(l).toEqual([
      [{ c: 0, r: 0 }, { c: 1, r: 0 }, { c: 2, r: 0 }],
      [{ c: 2, r: 1 }, { c: 1, r: 1 }, { c: 0, r: 1 }],
    ]);
  });
  it('우하단 세로 Z자', () => {
    const l = generateLines(2, 2, { start: 'BR', direction: 'vertical', pattern: 'zigzag' });
    expect(l).toEqual([
      [{ c: 1, r: 1 }, { c: 1, r: 0 }],
      [{ c: 0, r: 1 }, { c: 0, r: 0 }],
    ]);
  });
});

describe('splitIntoGroups', () => {
  const lines = generateLines(4, 3, { start: 'TL', direction: 'vertical', pattern: 'serpentine' }); // 열 4개, 열마다 3개
  it('줄 단위: 용량 7 → 2열(6개)씩', () => {
    const g = splitIntoGroups(lines, 7, 'line');
    expect(g.map((x) => x.length)).toEqual([6, 6]);
  });
  it('채움: 용량 7 → 7, 5', () => {
    const g = splitIntoGroups(lines, 7, 'fill');
    expect(g.map((x) => x.length)).toEqual([7, 5]);
  });
  it('한 줄이 용량보다 길면 줄마다 자름', () => {
    const g = splitIntoGroups(lines, 2, 'line');
    expect(g.map((x) => x.length)).toEqual([2, 1, 2, 1, 2, 1, 2, 1]);
  });
});

describe('effectivePortCapacity', () => {
  const ctrl = { pixelsPerPort: 650000, maxPixels: 2600000, ports: 4 } as ControllerSpec;
  it('8bit 60Hz = 650,000', () => expect(effectivePortCapacity(ctrl, 8, 60)).toBe(650000));
  it('10bit = 50%', () => expect(effectivePortCapacity(ctrl, 10, 60)).toBe(325000));
  it('120Hz = 50%', () => expect(effectivePortCapacity(ctrl, 8, 120)).toBe(325000));
});

describe('assignPhases', () => {
  it('같은 전류 회로 6개 → 상마다 2개, 불평형 0%', () => {
    const cs = Array.from({ length: 6 }, (_, i) => ({ index: i, cells: [], currentA: 10, watts: 2200, load: 0.8, phase: 'L1' as const }));
    const r = assignPhases(cs, true);
    expect(r.phases.map((p) => p.circuits)).toEqual([2, 2, 2]);
    expect(r.imbalancePct).toBe(0);
  });
});

describe('analyze (기본 프로젝트)', () => {
  it('500×500 P3.9 128×128, 12×6, VX1000', () => {
    const a = analyze(defaultProject());
    // 128×128 = 16,384px → 포트당 floor(650000/16384) = 39개
    expect(a.data!.cabinetsPerPort).toBe(39);
    // 세로 6개 열 단위 → 포트당 6열(36개) → 12열 / 6 = 2포트
    expect(a.data!.ports.length).toBe(2);
    expect(a.screen.widthPx).toBe(128 * 12);
    // 200W / (220V × 0.95) = 0.957A, 회로 한도 min(20×0.8, 16) = 16A → 16개
    expect(a.circuits.cabinetsPerCircuit).toBe(16);
  });
});
