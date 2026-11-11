import type { Geometry } from './geometry';
import { MONO_STACK, themeColors, type DiagramTheme } from './palette';

export interface LegendItem {
  color: string;
  label: string;
  detail: string;
  danger?: boolean;
}

export const LEGEND_ROW_H = 26;
export const LEGEND_COL_W = 300;

export function legendColumns(width: number): number {
  return Math.max(1, Math.floor((width - 80) / LEGEND_COL_W));
}

/** 범례 영역에 필요한 높이 */
export function legendHeight(count: number, width: number): number {
  return Math.ceil(count / legendColumns(width)) * LEGEND_ROW_H + 30;
}

/** 배선도 아래 범례 (포트 또는 회로 목록) */
export function Legend({ items, geom: g, theme }: { items: LegendItem[]; geom: Geometry; theme: DiagramTheme }) {
  const t = themeColors[theme];
  const cols = legendColumns(g.width);
  const top = g.padT + g.gridH + 50;
  return (
    <g>
      {items.map((it, i) => {
        const x = g.padL + (i % cols) * LEGEND_COL_W;
        const y = top + Math.floor(i / cols) * LEGEND_ROW_H;
        return (
          <g key={i}>
            <rect x={x} y={y - 9} width={14} height={14} rx={3} fill={it.color} />
            <text x={x + 22} y={y} fill={t.text} fontSize={12.5} fontWeight={700} dominantBaseline="central">{it.label}</text>
            <text x={x + 82} y={y} fill={it.danger ? '#ff5d6c' : t.textMuted} fontSize={11.5}
              dominantBaseline="central" fontFamily={MONO_STACK}>{it.detail}</text>
          </g>
        );
      })}
    </g>
  );
}
