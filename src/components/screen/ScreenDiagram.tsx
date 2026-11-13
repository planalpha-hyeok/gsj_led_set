import type { Analysis } from '../../core/analyze';
import type { Project } from '../../types';
import { DiagramBase } from '../diagram/DiagramBase';
import { gridLayout } from '../../core/gridLayout';
import { cellName, cellRect, computeGeometry } from '../diagram/geometry';
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
  const lay = gridLayout(p);
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
          const o = cellRect(g, { c, r });
          const changed = lay.cols[c].changed || lay.rows[r].changed;
          const x0 = lay.colStartPx[c];
          const y0 = lay.rowStartPx[r];
          const f = Math.min(g.font, o.w * 0.17, o.h * 0.17);
          return (
            <g key={`${c}-${r}`}>
              <rect x={o.x + 2} y={o.y + 2} width={o.w - 4} height={o.h - 4} rx={4}
                fill={changed ? t.accent : t.cell} fillOpacity={changed ? 0.1 : 1}
                stroke={changed ? t.accent : t.cellStroke} strokeOpacity={changed ? 0.6 : 1} strokeWidth={1.5}>
                <title>{`${cellName({ c, r })} · ${lay.cols[c].mm}×${lay.rows[r].mm}mm · ${lay.cols[c].px}×${lay.rows[r].px}px · X ${x0}, Y ${y0}`}</title>
              </rect>
              <text x={o.x + o.w / 2} y={o.y + o.h / 2 - f * 0.5} fill={t.text} fontSize={f}
                textAnchor="middle" fontWeight={700} fontFamily={MONO_STACK}>{cellName({ c, r })}</text>
              <text x={o.x + o.w / 2} y={o.y + o.h / 2 + f * 0.6} fill={t.textMuted} fontSize={f * 0.66}
                textAnchor="middle" fontFamily={MONO_STACK}>{`${x0},${y0}`}</text>
              {changed && (
                <text x={o.x + o.w / 2} y={o.y + o.h / 2 + f * 1.4} fill={t.accent} fontSize={f * 0.62}
                  textAnchor="middle" fontFamily={MONO_STACK} fontWeight={700}>{`${lay.cols[c].mm}×${lay.rows[r].mm}`}</text>
              )}
            </g>
          );
        }),
      )}
    </DiagramBase>
  );
}
