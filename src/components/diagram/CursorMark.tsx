import type { Cell } from '../../types';
import { cellOrigin, type Geometry } from './geometry';

/** 키보드 커서 표시 (화면 편집 시에만, 내보내기에는 나오지 않음) */
export function CursorMark({ cell, geom: g, drawing }: { cell: Cell; geom: Geometry; drawing: boolean }) {
  const o = cellOrigin(g, cell);
  const color = drawing ? '#ffffff' : '#35e0b8';
  return (
    <g pointerEvents="none">
      <rect x={o.x - 2} y={o.y - 2} width={g.cellW + 4} height={g.cellH + 4} rx={7}
        fill="none" stroke={color} strokeWidth={3.5} strokeDasharray={drawing ? undefined : '10 6'}>
        <animate attributeName="stroke-opacity" values="1;0.45;1" dur="1.2s" repeatCount="indefinite" />
      </rect>
    </g>
  );
}
