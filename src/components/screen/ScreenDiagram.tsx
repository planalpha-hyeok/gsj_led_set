import type { Analysis } from '../../core/analyze';
import type { Project } from '../../types';
import { DiagramBase } from '../diagram/DiagramBase';
import { cellName, cellOrigin, computeGeometry } from '../diagram/geometry';
import { MONO_STACK, themeColors, type DiagramTheme } from '../diagram/palette';

interface Props {
  project: Project;
  analysis: Analysis;
  theme: DiagramTheme;
}

/** 화면 배치도: 캐비닛 위치와 시작 픽셀 좌표 */
export function ScreenDiagram({ project: p, analysis: a, theme }: Props) {
  const t = themeColors[theme];
  const g = computeGeometry(p);
  const s = a.screen;
  return (
    <DiagramBase
      project={p}
      geom={g}
      theme={theme}
      title="화면 배치"
      subtitle={`${p.cabinet.name} · ${p.cols} × ${p.rows} = ${s.cabinetCount}개 · ${s.widthPx} × ${s.heightPx} px (${s.aspect})`}
    >
      <rect x={g.padL - 4} y={g.padT - 4} width={g.gridW + 8} height={g.gridH + 8} rx={8} fill="none"
        stroke={t.accent} strokeOpacity={0.35} strokeWidth={2} />
      {Array.from({ length: p.rows }, (_, r) =>
        Array.from({ length: p.cols }, (_, c) => {
          const o = cellOrigin(g, { c, r });
          return (
            <g key={`${c}-${r}`}>
              <rect x={o.x + 2} y={o.y + 2} width={g.cellW - 4} height={g.cellH - 4} rx={4} fill={t.cell} stroke={t.cellStroke} strokeWidth={1.5}>
                <title>{`${cellName({ c, r })} · X ${c * p.cabinet.widthPx}, Y ${r * p.cabinet.heightPx}`}</title>
              </rect>
              <text x={o.x + g.cellW / 2} y={o.y + g.cellH / 2 - g.font * 0.3} fill={t.text} fontSize={g.font}
                textAnchor="middle" fontWeight={700} fontFamily={MONO_STACK}>{cellName({ c, r })}</text>
              <text x={o.x + g.cellW / 2} y={o.y + g.cellH / 2 + g.font * 0.9} fill={t.textMuted} fontSize={g.font * 0.66}
                textAnchor="middle" fontFamily={MONO_STACK}>{`${c * p.cabinet.widthPx},${r * p.cabinet.heightPx}`}</text>
            </g>
          );
        }),
      )}
    </DiagramBase>
  );
}
