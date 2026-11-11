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
  const totalW = p.cabinet.widthMm * p.cols;
  const totalH = p.cabinet.heightMm * p.rows;

  return (
    <svg
      xmlns="http://www.w3.org/2000/svg"
      width={g.width}
      height={g.height}
      viewBox={`0 0 ${g.width} ${g.height}`}
      fontFamily={FONT_STACK}
    >
      <rect x={0} y={0} width={g.width} height={g.height} fill={t.bg} />

      <text x={g.padL} y={24} fill={t.title} fontSize={17} fontWeight={700}>{title}</text>
      {subtitle && <text x={g.padL} y={44} fill={t.textMuted} fontSize={12}>{subtitle}</text>}

      {/* 열 이름 (A, B, C...) */}
      {Array.from({ length: p.cols }, (_, c) => (
        <text key={`c${c}`} x={g.padL + c * g.cellW + g.cellW / 2} y={g.padT - 10} fill={t.textMuted}
          fontSize={rulerFont} textAnchor="middle" fontFamily={MONO_STACK}>{colName(c)}</text>
      ))}
      {/* 행 번호 (1, 2, 3...) */}
      {Array.from({ length: p.rows }, (_, r) => (
        <text key={`r${r}`} x={g.padL - 12} y={g.padT + r * g.cellH + g.cellH / 2} fill={t.textMuted}
          fontSize={rulerFont} textAnchor="end" dominantBaseline="central" fontFamily={MONO_STACK}>{r + 1}</text>
      ))}

      {children}

      {/* 전체 크기 */}
      <text x={g.padL + g.gridW} y={g.padT + g.gridH + 22} fill={t.textMuted} fontSize={12} textAnchor="end" fontFamily={MONO_STACK}>
        {`${(totalW / 1000).toFixed(2)} m × ${(totalH / 1000).toFixed(2)} m · ${p.cabinet.widthPx * p.cols} × ${p.cabinet.heightPx * p.rows} px`}
      </text>
    </svg>
  );
}
