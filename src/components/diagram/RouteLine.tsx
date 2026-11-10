import type { Cell } from '../../types';
import { cellCenter, type Geometry } from './geometry';

interface Props {
  cells: Cell[];
  geom: Geometry;
  color: string;
  /** 시작점 표시 글자 (예: "P1") */
  startLabel: string;
  labelColor: string;
}

/** 케이블 경로: 캐비닛 중심을 잇는 선 + 진행 방향 화살표 + 시작 표시 */
export function RouteLine({ cells, geom: g, color, startLabel, labelColor }: Props) {
  if (cells.length === 0) return null;
  const pts = cells.map((c) => cellCenter(g, c));
  const sw = Math.max(2, g.cellW * 0.03);
  const arrow = g.cellW * 0.09;
  const r = Math.min(g.cellW, g.cellH) * 0.17;

  return (
    <g>
      <polyline
        points={pts.map((pt) => `${pt.x},${pt.y}`).join(' ')}
        fill="none" stroke={color} strokeWidth={sw} strokeLinejoin="round" strokeLinecap="round" opacity={0.95}
      />
      {/* 구간 가운데에 화살표 */}
      {pts.slice(1).map((b, i) => {
        const a = pts[i];
        const mx = (a.x + b.x) / 2;
        const my = (a.y + b.y) / 2;
        const ang = (Math.atan2(b.y - a.y, b.x - a.x) * 180) / Math.PI;
        return (
          <polygon key={i} points={`${arrow},0 ${-arrow * 0.8},${-arrow * 0.75} ${-arrow * 0.8},${arrow * 0.75}`}
            fill={color} transform={`translate(${mx},${my}) rotate(${ang})`} />
        );
      })}
      {/* 시작점 */}
      <circle cx={pts[0].x} cy={pts[0].y} r={r} fill={color} />
      <text x={pts[0].x} y={pts[0].y} fill={labelColor} fontSize={r * 0.9} fontWeight={800}
        textAnchor="middle" dominantBaseline="central">{startLabel}</text>
      {/* 끝점 */}
      <rect x={pts[pts.length - 1].x - r * 0.45} y={pts[pts.length - 1].y - r * 0.45} width={r * 0.9} height={r * 0.9}
        fill="none" stroke={color} strokeWidth={sw} />
    </g>
  );
}
