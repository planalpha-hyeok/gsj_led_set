import type { Cell, ControllerSpec, Issue, Project } from '../types';
import { effectivePortCapacity, effectiveUnitCapacity } from './portCapacity';
import { generateLines, splitIntoGroups } from './routePath';

export interface PortRun {
  /** 전체 포트 일련번호 (0부터) */
  index: number;
  /** 컨트롤러 장비 번호 (0부터) */
  unit: number;
  /** 장비 내 포트 번호 (1부터) */
  portNo: number;
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
  issues: Issue[];
}

/** 데이터 매핑: 포트별 캐비닛 경로와 컨트롤러 대수 계산 */
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
    return { portCapacity, unitCapacity, cabinetsPerPort, ports: [], units: [], issues };
  }

  const lines = generateLines(p.cols, p.rows, p.mapping);
  const groups = splitIntoGroups(lines, cabinetsPerPort, p.mapping.split);

  // 포트를 컨트롤러 장비에 순서대로 배정 (포트 수·장비 총용량 둘 다 확인)
  const ports: PortRun[] = [];
  const units: ControllerUnit[] = [];
  let unit = 0;
  let portInUnit = 0;
  let unitPixels = 0;
  groups.forEach((cells, index) => {
    const pixels = cells.length * cabPx;
    if (portInUnit >= ctrl.ports || (portInUnit > 0 && unitPixels + pixels > unitCapacity)) {
      unit++;
      portInUnit = 0;
      unitPixels = 0;
    }
    portInUnit++;
    unitPixels += pixels;
    ports.push({ index, unit, portNo: portInUnit, cells, pixels, load: pixels / portCapacity });
  });

  const unitCount = ports.length ? ports[ports.length - 1].unit + 1 : 0;
  for (let u = 0; u < unitCount; u++) {
    const mine = ports.filter((pr) => pr.unit === u);
    const cells = mine.flatMap((pr) => pr.cells);
    const pixels = mine.reduce((s, pr) => s + pr.pixels, 0);
    const cs = cells.map((c) => c.c);
    const rs = cells.map((c) => c.r);
    const widthPx = (Math.max(...cs) - Math.min(...cs) + 1) * p.cabinet.widthPx;
    const heightPx = (Math.max(...rs) - Math.min(...rs) + 1) * p.cabinet.heightPx;
    units.push({ index: u, ports: mine.length, pixels, load: pixels / unitCapacity, widthPx, heightPx });

    if (ctrl.maxWidth && widthPx > ctrl.maxWidth) {
      issues.push({ severity: 'error', message: `컨트롤러 #${u + 1}: 담당 가로 ${widthPx}px가 최대 가로 ${ctrl.maxWidth}px 초과` });
    }
    if (ctrl.maxHeight && heightPx > ctrl.maxHeight) {
      issues.push({ severity: 'error', message: `컨트롤러 #${u + 1}: 담당 세로 ${heightPx}px가 최대 세로 ${ctrl.maxHeight}px 초과` });
    }
  }

  if (unitCount > 1) {
    issues.push({ severity: 'warning', message: `컨트롤러 ${unitCount}대 필요 → 장비 간 영상 분배(스플리터/프로세서) 필요` });
  }

  return { portCapacity, unitCapacity, cabinetsPerPort, ports, units, issues };
}
