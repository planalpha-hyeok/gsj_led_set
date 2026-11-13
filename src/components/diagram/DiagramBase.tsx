import { gridLayout } from '../../core/gridLayout';
import type { ReactNode } from 'react';
import type { Project } from '../../types';
import { colName, type Geometry } from './geometry';
import { FONT_STACK, MONO_STACK, themeColors, type DiagramTheme } from './palette';

interface Props {
  project: Project;
  geom: Geometry;
  theme: DiagramTheme;
  title: string;
  subtitle?: string;
  children: ReactNode;
}

/** 모든 배선도 공통 틀: 배경, 제목, 행/열 눈금, 크기 표시 */
export function DiagramBase({ project: p, geom: g, theme, title, subtitle, children }: Props) {
  const t = themeColors[theme];
  const rulerFont = Math.max(11, g.font * 0.8);
  const lay = gridLayout(p);
  const totalW = lay.widthMm;
  const totalH = lay.heightMm;

  return (
    <svg
      xmlns="http://www.w3.org/2000/svg"
      width={g.width}
      height={g.height}
      viewBox={`0 0 ${g.width} ${g.height}`}
      fontFamily={FONT_STACK}
    >
      <rect x={0} y={0} width={g.width} height={g.height} fill={t.bg} />

      <text x={g.width / 2} y={34} fill={t.title} fontSize={19} fontWeight={800} textAnchor="middle">{title}</text>
      {subtitle && <text x={g.width / 2} y={56} fill={t.textMuted} fontSize={12.5} textAnchor="middle">{subtitle}</text>}

      {/* 열 이름 (A, B, C...) */}
      {Array.from({ length: p.cols }, (_, c) => (
        <text key={`c${c}`} x={g.colX[c] + g.colW[c] / 2} y={g.padT - 10} fill={lay.cols[c].changed ? t.accent : t.textMuted}
          fontSize={rulerFont} textAnchor="middle" fontFamily={MONO_STACK} fontWeight={lay.cols[c].changed ? 700 : 400}>
          {lay.cols[c].changed ? `${colName(c)} ${lay.cols[c].mm}` : colName(c)}
        </text>
      ))}
      {/* 행 번호 (1, 2, 3...) */}
      {Array.from({ length: p.rows }, (_, r) => (
        <text key={`r${r}`} x={g.padL - 12} y={g.rowY[r] + g.rowH[r] / 2} fill={lay.rows[r].changed ? t.accent : t.textMuted}
          fontSize={rulerFont} textAnchor="end" dominantBaseline="central" fontFamily={MONO_STACK} fontWeight={lay.rows[r].changed ? 700 : 400}>
          {lay.rows[r].changed ? `${lay.rows[r].mm} ${r + 1}` : r + 1}
        </text>
      ))}

      {children}

      {/* 전체 크기 */}
      <text x={g.width / 2} y={g.padT + g.gridH + 28} fill={t.textMuted} fontSize={12} textAnchor="middle" fontFamily={MONO_STACK}>
        {`${(totalW / 1000).toFixed(2)} m × ${(totalH / 1000).toFixed(2)} m · ${lay.widthPx} × ${lay.heightPx} px`}
      </text>
    </svg>
  );
}
