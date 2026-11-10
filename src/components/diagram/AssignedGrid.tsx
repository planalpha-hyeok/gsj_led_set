import type { Cell, Project } from '../../types';
import { cellName, cellOrigin, type Geometry } from './geometry';
import { MONO_STACK, themeColors, type DiagramTheme } from './palette';

export interface Assignment {
  /** 그룹 번호 (포트 또는 회로) */
  group: number;
  /** 그룹 내 연결 순서 (1부터) */
  order: number;
}

interface Props {
  project: Project;
  geom: Geometry;
  theme: DiagramTheme;
  groups: Cell[][];
  colorOf: (group: number) => string;
  nameOf: (group: number) => string;
}

/** 그룹(포트/회로)별로 색칠된 캐비닛 격자 */
export function AssignedGrid({ project: p, geom: g, theme, groups, colorOf, nameOf }: Props) {
  const t = themeColors[theme];
  const lookup = new Map<string, Assignment>();
  groups.forEach((cells, gi) => cells.forEach((c, i) => lookup.set(`${c.c},${c.r}`, { group: gi, order: i + 1 })));

  return (
    <g>
      {Array.from({ length: p.rows }, (_, r) =>
        Array.from({ length: p.cols }, (_, c) => {
          const o = cellOrigin(g, { c, r });
          const hit = lookup.get(`${c},${r}`);
          const color = hit ? colorOf(hit.group) : t.cellStroke;
          return (
            <g key={`${c}-${r}`}>
              <rect x={o.x + 2} y={o.y + 2} width={g.cellW - 4} height={g.cellH - 4} rx={4}
                fill={hit ? color : t.cell} fillOpacity={hit ? 0.16 : 1}
                stroke={color} strokeOpacity={hit ? 0.7 : 1} strokeWidth={1.5}>
                <title>{`${cellName({ c, r })}${hit ? ` · ${nameOf(hit.group)} · ${hit.order}번째` : ' · 미연결'}`}</title>
              </rect>
              <text x={o.x + 8} y={o.y + 8 + g.font * 0.7} fill={t.textMuted} fontSize={g.font * 0.72} fontFamily={MONO_STACK}>
                {cellName({ c, r })}
              </text>
              {hit && (
                <text x={o.x + g.cellW - 8} y={o.y + g.cellH - 8} fill={t.text} fontSize={g.font * 0.8}
                  textAnchor="end" fontFamily={MONO_STACK} fontWeight={600}>{hit.order}</text>
              )}
            </g>
          );
        }),
      )}
    </g>
  );
}
