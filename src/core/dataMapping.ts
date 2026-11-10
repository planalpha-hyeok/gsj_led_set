import type { Cell, ControllerSpec, Issue, Project } from '../types';
import { effectivePortCapacity, effectiveUnitCapacity } from './portCapacity';
import { checkGroups, inBounds } from './wiringCheck';

export interface PortRun {
  /** dataRuns 배열 순서 (색상 기준) */
  index: number;
  id: string;
  /** 컨트롤러 번호 (0부터) */
  unit: number;
  /** 장비 내 포트 번호 (1부터) */
  portNo: number;
  label: string;
  cells: Cell[];
  pixels: number;
  /** 포트 용량 대비 사용률 (0~1) */
  load: number;
}

export interface ControllerUnit {
  index: number;
  ports: number;
  pixels: number;
  load: number;
  widthPx: number;
  heightPx: number;
}

export interface DataMapResult {
  portCapacity: number;
  unitCapacity: number;
  cabinetsPerPort: number;
  ports: PortRun[];
  units: ControllerUnit[];
  unassigned: number;
  issues: Issue[];
}

export function portLabel(unit: number, port: number, multiUnit: boolean): string {
  return multiUnit ? `U${unit + 1}-P${port}` : `P${port}`;
}

/** 수동 데이터 배선 검사: 포트 용량, 포트 번호, 컨트롤러 용량 */
export function buildDataMap(p: Project, ctrl: ControllerSpec): DataMapResult {
  const issues: Issue[] = [];
  const cabPx = p.cabinet.widthPx * p.cabinet.heightPx;
  const portCapacity = effectivePortCapacity(ctrl, p.bitDepth, p.frameRate);
  const unitCapacity = effectiveUnitCapacity(ctrl, p.bitDepth, p.frameRate);
  const cabinetsPerPort = cabPx > 0 ? Math.floor(portCapacity / cabPx) : 0;

  if (ctrl.pixelsPerPort === null) {
    issues.push({ severity: 'info', message: `${ctrl.model}: 포트당 용량 미확인 → 총용량 ÷ 포트수로 계산함` });
  }
  if (p.bitDepth !== 8) {
    issues.push({ severity: 'info', message: `${p.bitDepth}bit: 포트 용량을 8bit의 50%로 계산함 (보수적 기준)` });
  }
  if (p.frameRate !== 60) {
    issues.push({ severity: 'info', message: `${p.frameRate}Hz: 포트 용량을 60Hz 대비 비례 계산함 (근사치)` });
  }
  if (cabinetsPerPort < 1) {
    issues.push({ severity: 'error', message: `캐비닛 1개(${cabPx.toLocaleString()}px)가 포트 용량(${portCapacity.toLocaleString()}px)보다 큽니다` });
  }

  const unitIds = [...new Set(p.dataRuns.map((r) => r.unit))].sort((a, b) => a - b);
  const multiUnit = unitIds.length > 1 || (unitIds[0] ?? 0) > 0;

  const ports: PortRun[] = p.dataRuns.map((r, index) => {
    const cells = r.cells.filter((c) => inBounds(p, c));
    const pixels = cells.length * cabPx;
    return {
      index, id: r.id, unit: r.unit, portNo: r.port,
      label: portLabel(r.unit, r.port, multiUnit),
      cells, pixels, load: portCapacity > 0 ? pixels / portCapacity : 0,
    };
  });

  // 포트 검사
  const used = new Set<string>();
  for (const pr of ports) {
    const key = `${pr.unit}-${pr.portNo}`;
    if (used.has(key)) issues.push({ severity: 'error', message: `${pr.label}: 같은 포트 번호가 두 번 사용됨` });
    used.add(key);
    if (pr.portNo > ctrl.ports) {
      issues.push({ severity: 'error', message: `${pr.label}: ${ctrl.model}는 포트가 ${ctrl.ports}개뿐입니다` });
    }
    if (pr.load > 1) {
      issues.push({ severity: 'error', message: `${pr.label}: 포트 용량 초과 ${(pr.load * 100).toFixed(0)}% (최대 캐비닛 ${cabinetsPerPort}개)` });
    }
  }

  // 컨트롤러별 검사
  const units: ControllerUnit[] = unitIds.map((u) => {
    const mine = ports.filter((pr) => pr.unit === u);
    const cells = mine.flatMap((pr) => pr.cells);
    const pixels = mine.reduce((s, pr) => s + pr.pixels, 0);
    let widthPx = 0;
    let heightPx = 0;
    if (cells.length) {
      const cs = cells.map((c) => c.c);
      const rs = cells.map((c) => c.r);
      widthPx = (Math.max(...cs) - Math.min(...cs) + 1) * p.cabinet.widthPx;
      heightPx = (Math.max(...rs) - Math.min(...rs) + 1) * p.cabinet.heightPx;
    }
    const name = multiUnit ? `컨트롤러 #${u + 1}` : ctrl.model;
    if (pixels > unitCapacity) {
      issues.push({ severity: 'error', message: `${name}: 장비 총용량 초과 (${pixels.toLocaleString()} / ${unitCapacity.toLocaleString()} px)` });
    }
    if (ctrl.maxWidth && widthPx > ctrl.maxWidth) {
      issues.push({ severity: 'error', message: `${name}: 담당 가로 ${widthPx}px가 최대 가로 ${ctrl.maxWidth}px 초과` });
    }
    if (ctrl.maxHeight && heightPx > ctrl.maxHeight) {
      issues.push({ severity: 'error', message: `${name}: 담당 세로 ${heightPx}px가 최대 세로 ${ctrl.maxHeight}px 초과` });
    }
    return { index: u, ports: mine.length, pixels, load: unitCapacity > 0 ? pixels / unitCapacity : 0, widthPx, heightPx };
  });

  if (units.length > 1) {
    issues.push({ severity: 'warning', message: `컨트롤러 ${units.length}대 사용 → 장비 간 영상 분배(스플리터/프로세서) 필요` });
  }

  const check = checkGroups(p, ports.map((pr) => ({ name: pr.label, cells: pr.cells })), '데이터');
  issues.push(...check.issues);

  return { portCapacity, unitCapacity, cabinetsPerPort, ports, units, unassigned: check.unassigned, issues };
}

/** 새 포트를 추가할 때 비어 있는 첫 번째 (컨트롤러, 포트) */
export function nextFreePort(p: Project, portsPerUnit: number): { unit: number; port: number } {
  const used = new Set(p.dataRuns.map((r) => `${r.unit}-${r.port}`));
  for (let u = 0; u < 100; u++) {
    for (let port = 1; port <= portsPerUnit; port++) {
      if (!used.has(`${u}-${port}`)) return { unit: u, port };
    }
  }
  return { unit: 0, port: 1 };
}
