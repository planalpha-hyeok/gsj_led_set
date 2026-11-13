import type { ModuleSpec } from '../data/modules';
import type { CabinetSpec } from '../types';

/** 모듈 종류 × 배치로 캐비닛 크기·해상도·이름 계산 (전력·무게 등 나머지 값은 유지) */
export function cabinetFromModules(base: CabinetSpec, m: ModuleSpec, cols: number, rows: number): CabinetSpec {
  const widthMm = m.widthMm * cols;
  const heightMm = m.heightMm * rows;
  return {
    ...base,
    id: `${m.id}-${cols}x${rows}`,
    name: `${widthMm}×${heightMm} ${m.pitch} (모듈 ${cols * rows}장)`,
    widthMm,
    heightMm,
    widthPx: m.widthPx * cols,
    heightPx: m.heightPx * rows,
    moduleId: m.id,
    moduleCols: cols,
    moduleRows: rows,
  };
}

/** 캐비닛 면적 (㎡) */
export function cabinetAreaM2(c: CabinetSpec): number {
  return (c.widthMm * c.heightMm) / 1_000_000;
}
