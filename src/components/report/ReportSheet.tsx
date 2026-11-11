import type { Analysis } from '../../core/analyze';
import { buildReport, circuitHeader, portHeader, type ReportRow } from '../../core/report';
import type { Project } from '../../types';
import { FONT_STACK, MONO_STACK, themeColors, type DiagramTheme } from '../diagram/palette';

export type ReportPart = 'all' | 'summary' | 'tables';

interface Props {
  project: Project;
  analysis: Analysis;
  theme: DiagramTheme;
  /** all: 전체 (PNG), summary: 요약·확인 사항, tables: 포트·회로 표 (PDF 페이지 분리용) */
  part?: ReportPart;
}

const W = 1120;
const ROW = 22;
const COL_W = (W - 40 * 2 - 30 * 2) / 3;

/** 요약 보고서 SVG (PNG·PDF 공용) */
export function ReportSheet({ project: p, analysis: a, theme, part = 'all' }: Props) {
  const t = themeColors[theme];
  const r = buildReport(p, a);
  const errors = a.issues.filter((i) => i.severity !== 'info');

  const showSummary = part !== 'tables';
  const showTables = part !== 'summary';
  const blockH = (rows: number) => 30 + rows * ROW;
  const topH = showSummary ? Math.max(blockH(r.summary.length), blockH(r.power.length), blockH(r.bom.length)) + 20 : 0;
  const tableRows = Math.max(r.ports.length, r.circuits.length);
  const tablesTop = 110 + topH;
  const tablesH = showTables ? 30 + (tableRows + 1) * ROW + 20 : 0;
  const issuesTop = tablesTop + tablesH;
  const H = showSummary ? issuesTop + 30 + Math.max(1, errors.length) * ROW + 30 : issuesTop + 30;

  const kv = (title: string, rows: ReportRow[], x: number, y: number) => (
    <g>
      <text x={x} y={y} fill={t.title} fontSize={14} fontWeight={700}>{title}</text>
      <line x1={x} x2={x + COL_W} y1={y + 8} y2={y + 8} stroke={t.cellStroke} />
      {rows.map((row, i) => (
        <g key={i}>
          <text x={x} y={y + 28 + i * ROW} fill={t.textMuted} fontSize={11.5}>{row.label}</text>
          <text x={x + COL_W} y={y + 28 + i * ROW} fill={t.text} fontSize={11.5} textAnchor="end" fontFamily={MONO_STACK}>{row.value}</text>
        </g>
      ))}
    </g>
  );

  const table = (title: string, header: string[], rows: string[][], x: number, y: number, width: number) => {
    const cw = width / header.length;
    return (
      <g>
        <text x={x} y={y} fill={t.title} fontSize={14} fontWeight={700}>{title}</text>
        <rect x={x} y={y + 10} width={width} height={ROW} fill={t.cell} />
        {header.map((h, i) => (
          <text key={i} x={x + cw * i + 8} y={y + 10 + ROW * 0.68} fill={t.textMuted} fontSize={11} fontWeight={600}>{h}</text>
        ))}
        {rows.map((row, ri) => (
          <g key={ri}>
            <line x1={x} x2={x + width} y1={y + 10 + ROW * (ri + 2)} y2={y + 10 + ROW * (ri + 2)} stroke={t.cellStroke} strokeOpacity={0.6} />
            {row.map((v, ci) => (
              <text key={ci} x={x + cw * ci + 8} y={y + 10 + ROW * (ri + 1) + ROW * 0.68} fill={t.text} fontSize={11} fontFamily={MONO_STACK}>{v}</text>
            ))}
          </g>
        ))}
      </g>
    );
  };

  const half = (W - 80 - 30) / 2;
  const date = new Date(p.updatedAt).toLocaleString('ko-KR');

  return (
    <svg xmlns="http://www.w3.org/2000/svg" width={W} height={H} viewBox={`0 0 ${W} ${H}`} fontFamily={FONT_STACK}>
      <rect width={W} height={H} fill={t.bg} />
      <rect x={0} y={0} width={W} height={6} fill="#35e0b8" />
      <text x={40} y={52} fill={t.title} fontSize={24} fontWeight={800}>{p.name}</text>
      <text x={40} y={78} fill={t.textMuted} fontSize={12}>LED 전광판 매핑·전원 계획서 · {date}</text>

      {showSummary && (
        <g>
          {kv('화면 / 장비', r.summary, 40, 110)}
          {kv('전력', r.power, 40 + COL_W + 30, 110)}
          {kv('자재 수량', r.bom, 40 + (COL_W + 30) * 2, 110)}
        </g>
      )}

      {showTables && (
        <g>
          {table('데이터 포트', portHeader, r.ports, 40, tablesTop, half)}
          {table('전원 회로', circuitHeader, r.circuits, 40 + half + 30, tablesTop, half)}
        </g>
      )}

      {showSummary && (
        <g>
          <text x={40} y={issuesTop} fill={t.title} fontSize={14} fontWeight={700}>확인 사항</text>
          {errors.length === 0 && <text x={40} y={issuesTop + 28} fill={t.textMuted} fontSize={11.5}>경고 없음</text>}
          {errors.map((e, i) => (
            <text key={i} x={40} y={issuesTop + 28 + i * ROW} fill={e.severity === 'error' ? '#e5484d' : '#d18a00'} fontSize={11.5}>
              {e.severity === 'error' ? '■ 오류  ' : '▲ 주의  '}{e.message}
            </text>
          ))}
        </g>
      )}
      <text x={40} y={H - 14} fill={t.textMuted} fontSize={10}>
        본 계산은 참고용입니다. 장비 사양은 제조사 사양서, 전기 공사는 전기 관련 법규와 자격자 확인을 따르십시오.
      </text>
    </svg>
  );
}
