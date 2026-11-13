import { describe, expect, it } from 'vitest';
import { defaultProject } from '../store/defaults';
import { normalizeProject } from '../store/persistence';
import { appendCell, findOwner, pruneRuns, removeLast, truncateAfter } from '../store/wiringOps';
import type { Cell, ControllerSpec, DataRun, Project } from '../types';
import { analyze } from './analyze';
import { cabinetFromModules } from './cabinetBuild';
import { findModule } from '../data/modules';
import { nextFreePort } from './dataMapping';
import { effectivePortCapacity } from './portCapacity';
import { computePhaseLoads, suggestPhase } from './threePhase';

const col = (c: number, rows: number): Cell[] => Array.from({ length: rows }, (_, r) => ({ c, r }));

/** 기본 프로젝트(640×480 P1.86, 5×3, X4m)에 배선 채우기 */
function wired(): Project {
  const p = defaultProject();
  // 포트당 floor(655,360 / (344×258=88,752)) = 7개 → 열 2개(6개)씩
  p.dataRuns = [
    { id: 'a', unit: 0, port: 1, cells: [...col(0, 3), ...col(1, 3).reverse()] },
    { id: 'b', unit: 0, port: 2, cells: [...col(2, 3), ...col(3, 3).reverse()] },
    { id: 'c', unit: 0, port: 3, cells: col(4, 3) },
  ];
  p.powerRuns = [
    { id: 'x', phase: 'L1', cells: [...col(0, 3), ...col(1, 3).reverse(), ...col(2, 3)] },
    { id: 'y', phase: 'L1', cells: [...col(3, 3), ...col(4, 3).reverse()] },
  ];
  return p;
}

describe('기본 프로젝트', () => {
  it('640×480 P1.86 5×3 = 1720×774, X4m 포트당 최대 7개', () => {
    const a = analyze(defaultProject());
    expect(a.screen.widthPx).toBe(1720);
    expect(a.screen.heightPx).toBe(774);
    expect(a.data!.cabinetsPerPort).toBe(7);
    expect(a.data!.unassigned).toBe(15);
  });
  it('배선 완료 시 미연결 0, 오류 없음', () => {
    const a = analyze(wired());
    expect(a.data!.unassigned).toBe(0);
    expect(a.circuits.unassigned).toBe(0);
    expect(a.issues.filter((i) => i.severity === 'error')).toEqual([]);
    // 6 × 88,752 = 532,512 / 655,360 = 81.3%
    expect(a.data!.ports[0].load).toBeCloseTo(532512 / 655360, 6);
    // SMPS 200W × 1 ÷ 0.85 = 235.3W, ÷ (220 × 0.95) = 1.126A × 9 = 10.1A (회로 한도 16A)
    expect(a.power.cabinetW).toBeCloseTo(200 / 0.85, 6);
    expect(a.circuits.circuits[0].currentA).toBeCloseTo((200 / 0.85 / 209) * 9, 6);
    // 회로당 최대 floor(16 / 1.126) = 14개
    expect(a.circuits.cabinetsPerCircuit).toBe(14);
  });
});

describe('이전 버전 파일 변환', () => {
  it('최대 전력 W → SMPS 같은 값 × 1개, 효율 100% (계산 결과 유지)', () => {
    const old = { ...defaultProject(), cabinet: { ...defaultProject().cabinet, powerMaxW: 300, powerAvgW: 100 } } as unknown as Partial<Project>;
    delete (old.cabinet as Partial<Project['cabinet']>).smpsW;
    const p = normalizeProject(old);
    expect(p.cabinet.smpsW).toBe(300);
    expect(p.cabinet.smpsCount).toBe(1);
    expect(p.cabinet.smpsEfficiency).toBe(1);
    expect('powerMaxW' in p.cabinet).toBe(false);
    expect(analyze(p).power.cabinetW).toBe(300);
  });
});

describe('수동 배선 검사', () => {
  it('포트 용량 초과', () => {
    const p = wired();
    p.dataRuns = [{ id: 'a', unit: 0, port: 1, cells: [...col(0, 3), ...col(1, 3), ...col(2, 2)] }];
    expect(analyze(p).issues.some((i) => i.severity === 'error' && i.message.includes('포트 용량 초과'))).toBe(true);
  });
  it('같은 포트 번호 중복', () => {
    const p = wired();
    p.dataRuns[1].port = 1;
    expect(analyze(p).issues.some((i) => i.message.includes('같은 포트 번호'))).toBe(true);
  });
  it('포트 번호가 장비 포트 수보다 큼', () => {
    const p = wired();
    p.dataRuns[2].port = 5;
    expect(analyze(p).issues.some((i) => i.message.includes('포트가 4개뿐'))).toBe(true);
  });
  it('붙어 있지 않은 연결 경고', () => {
    const p = wired();
    p.dataRuns[2].cells = [{ c: 4, r: 0 }, { c: 4, r: 2 }, { c: 4, r: 1 }];
    expect(analyze(p).issues.some((i) => i.message.includes('붙어 있지 않음'))).toBe(true);
  });
  it('회로 과부하', () => {
    const p = wired();
    p.powerRuns = [{ id: 'x', phase: 'L1', cells: [...col(0, 3), ...col(1, 3), ...col(2, 3), ...col(3, 3), ...col(4, 3)] }];
    expect(analyze(p).issues.some((i) => i.message.includes('회로 과부하'))).toBe(true);
  });
});

describe('배선 편집 함수', () => {
  const runs: DataRun[] = [{ id: 'a', unit: 0, port: 1, cells: [{ c: 0, r: 0 }, { c: 0, r: 1 }] }, { id: 'b', unit: 0, port: 2, cells: [] }];
  it('추가: 다른 배선에 있는 캐비닛은 추가 안 됨', () => {
    expect(appendCell(runs, 'b', { c: 0, r: 0 })).toBe(runs);
    expect(appendCell(runs, 'b', { c: 1, r: 0 })[1].cells).toEqual([{ c: 1, r: 0 }]);
  });
  it('마지막 제거 / 잘라내기 / 찾기', () => {
    expect(removeLast(runs, 'a')[0].cells).toEqual([{ c: 0, r: 0 }]);
    expect(truncateAfter(runs, 'a', 0)[0].cells).toEqual([{ c: 0, r: 0 }]);
    expect(findOwner(runs, { c: 0, r: 1 })).toEqual({ run: runs[0], pos: 1 });
  });
  it('화면 축소 시 범위 밖 제거', () => {
    expect(pruneRuns(runs, 1, 1)[0].cells).toEqual([{ c: 0, r: 0 }]);
  });
  it('다음 빈 포트', () => {
    const p = wired();
    expect(nextFreePort(p, 4)).toEqual({ unit: 0, port: 4 });
    p.dataRuns.push({ id: 'd', unit: 0, port: 4, cells: [] });
    expect(nextFreePort(p, 4)).toEqual({ unit: 1, port: 1 });
  });
});

describe('포트 용량 / 3상', () => {
  const ctrl = { pixelsPerPort: 650000, maxPixels: 2600000, ports: 4 } as ControllerSpec;
  it('8bit 60Hz / 10bit / 120Hz', () => {
    expect(effectivePortCapacity(ctrl, 8, 60)).toBe(650000);
    expect(effectivePortCapacity(ctrl, 10, 60)).toBe(325000);
    expect(effectivePortCapacity(ctrl, 8, 120)).toBe(325000);
  });
  it('상별 합계, 불평형률, 추천 상', () => {
    const mk = (phase: 'L1' | 'L2' | 'L3', a: number) => ({ index: 0, id: '', label: '', cells: [], currentA: a, watts: a * 220, load: 0, phase });
    const r = computePhaseLoads([mk('L1', 10), mk('L2', 10), mk('L3', 5)], true);
    expect(r.phases.map((x) => x.currentA)).toEqual([10, 10, 5]);
    expect(r.imbalancePct).toBeCloseTo((5 / (25 / 3)) * 100, 6);
    expect(suggestPhase(r.phases)).toBe('L3');
  });
});

describe('모듈 조합 캐비닛', () => {
  const base = defaultProject().cabinet;
  it('320×160 P1.86 2×4 = 640×640, 344×344px', () => {
    const c = cabinetFromModules(base, findModule('m320-p186')!, 2, 4);
    expect([c.widthMm, c.heightMm, c.widthPx, c.heightPx]).toEqual([640, 640, 344, 344]);
    expect(c.name).toBe('640×640 P1.86 (모듈 8장)');
  });
  it('250×250 P1.95 2×4 = 500×1000, 256×512px', () => {
    const c = cabinetFromModules(base, findModule('m250-p195')!, 2, 4);
    expect([c.widthMm, c.heightMm, c.widthPx, c.heightPx]).toEqual([500, 1000, 256, 512]);
  });
  it('기본 캐비닛 = 640×480 P1.86 모듈 6장', () => {
    expect(base.name).toBe('640×480 P1.86 (모듈 6장)');
    expect([base.widthPx, base.heightPx]).toEqual([344, 258]);
  });
});

describe('㎡당 W 전력', () => {
  it('600W/㎡ × 640×480(0.3072㎡) = 184.32W, 효율 미적용', () => {
    const p = defaultProject();
    p.cabinet = { ...p.cabinet, powerMode: 'area', powerPerM2: 600 };
    expect(analyze(p).power.cabinetW).toBeCloseTo(184.32, 6);
  });
  it('전력 0이면 오류', () => {
    const p = defaultProject();
    p.cabinet = { ...p.cabinet, powerMode: 'area', powerPerM2: 0 };
    expect(analyze(p).issues.some((i) => i.message.includes('캐비닛 전력이 0'))).toBe(true);
  });
  it('이전 파일(모듈 정보 없음) → 직접 입력 캐비닛, SMPS 기준', () => {
    const raw = { ...defaultProject(), cabinet: { id: 'x', name: 'old', widthPx: 128, heightPx: 128, widthMm: 500, heightMm: 500, powerMaxW: 200, weightKg: 7 } } as unknown as Partial<Project>;
    const p = normalizeProject(raw);
    expect(p.cabinet.moduleId).toBeUndefined();
    expect(p.cabinet.powerMode).toBe('smps');
    expect(p.cabinet.widthPx).toBe(128);
  });
});

describe('크기 혼합 화면 (열별 폭·행별 높이)', () => {
  // 기본 640×480(2×3장, 344×258px) 5×3, E열만 1장(320mm), 3행만 2장(320mm)
  const mixed = () => {
    const p = defaultProject();
    p.colModules = { '4': 1 };
    p.rowModules = { '2': 2 };
    return p;
  };
  it('전체 크기·해상도', () => {
    const a = analyze(mixed());
    expect([a.screen.widthMm, a.screen.heightMm]).toEqual([640 * 4 + 320, 480 * 2 + 320]);
    expect([a.screen.widthPx, a.screen.heightPx]).toEqual([344 * 4 + 172, 258 * 2 + 172]);
  });
  it('모서리 칸 = 320×320, 크기별 수량', async () => {
    const { gridLayout, cellSize, cabinetSizeCounts } = await import('./gridLayout');
    const g = gridLayout(mixed());
    expect(cellSize(g, { c: 4, r: 2 })).toEqual({ widthMm: 320, heightMm: 320, widthPx: 172, heightPx: 172 });
    expect(cabinetSizeCounts(g).map((x) => `${x.widthMm}x${x.heightMm}:${x.count}`)).toEqual(['640x480:8', '640x320:4', '320x480:2', '320x320:1']);
  });
  it('포트 픽셀은 캐비닛마다 실제 크기로 합산', () => {
    const p = mixed();
    p.dataRuns = [{ id: 'a', unit: 0, port: 1, cells: [{ c: 4, r: 0 }, { c: 4, r: 1 }, { c: 4, r: 2 }] }];
    expect(analyze(p).data!.ports[0].pixels).toBe(172 * 258 * 2 + 172 * 172);
  });
  it('㎡당 W 기준이면 캐비닛 면적별 전력, SMPS 기준이면 크기 무관 동일', () => {
    const p = mixed();
    p.cabinet = { ...p.cabinet, powerMode: 'area', powerPerM2: 600 };
    const area = (2.88 * 1.28);
    expect(analyze(p).power.totalW).toBeCloseTo(600 * area, 6);
    const q = mixed();
    expect(analyze(q).power.totalW).toBeCloseTo((200 / 0.85) * 15, 6);
  });
  it('모듈 조합이 아닌 캐비닛은 변경 무시', async () => {
    const { gridLayout } = await import('./gridLayout');
    const p = mixed();
    delete p.cabinet.moduleId;
    expect(gridLayout(p).widthMm).toBe(640 * 5);
  });
});
