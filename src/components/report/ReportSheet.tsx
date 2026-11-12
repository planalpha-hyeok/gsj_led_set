import type { ReactElement } from 'react';
import type { Analysis } from '../../core/analyze';
import { buildReport, circuitHeader, portHeader, type ReportRow } from '../../core/report';
import type { Project } from '../../types';
import { FONT_STACK, MONO_STACK, themeColors, type DiagramTheme } from '../diagram/palette';

/**
 * 보고서 SVG (A4 세로 비율, PNG·PDF 공용)
 * - all: 요약 + 표 전체 (PNG)
 * - summary: 요약·확인 사항 (PDF 1쪽)
 * - ports / circuits: 표 (PDF, rowFrom~rowTo 만큼 나눠서)
 */
export type ReportPart = 'all' | 'summary' | 'ports' | 'circuits';

interface Props {
  project: Project;
  analysis: Analysis;
  theme: DiagramTheme;
  part?: ReportPart;
  rowFrom?: number;
  rowTo?: number;
  /** 표가 여러 쪽일 때 "(1/2)" 표시 */
  pageNote?: string;
  /** PDF용: 높이를 A4 세로 비율 이상으로 */
  minPage?: boolean;
}

export const SHEET_W = 800;
const M = 48;
const ROW = 24;
const NOTE_H = 14;
const GAP = 28;
/** PDF 한 쪽에 넣을 표 행 수 */
export const ROWS_PER_PAGE = 38;
/** A4 세로 비율 높이 */
export const PAGE_H = Math.round(SHEET_W * 1.414);

const accentOf = (theme: DiagramTheme) => themeColors[theme].accent;

export function ReportSheet({ project: p, analysis: a, theme, part = 'all', rowFrom = 0, rowTo, pageNote, minPage }: Props) {
  const t = themeColors[theme];
  const r = buildReport(p, a);
  const issues = a.issues.filter((i) => i.severity !== 'info');
  const errColor = theme === 'light' ? '#c8102e' : '#ff5d6c';
  const warnColor = theme === 'light' ? '#a86200' : '#ffb547';
  const els: ReactElement[] = [];
  let y = 0;

  // ── 머리글 ──
  els.push(<rect key="bar" x={0} y={0} width={SHEET_W} height={6} fill={accentOf(theme)} />);
  els.push(<text key="t" x={M} y={58} fill={t.title} fontSize={26} fontWeight={800}>{p.name}</text>);
  els.push(
    <text key="st" x={M} y={84} fill={t.textMuted} fontSize={12.5}>
      LED 전광판 배선·전원 계획서 · {new Date(p.updatedAt).toLocaleString('ko-KR')}{pageNote ? ` · ${pageNote}` : ''}
    </text>,
  );
  y = 116;

  /** 제목 + 이름/값 목록. 반환값: 그린 높이 */
  const kv = (key: string, title: string, rows: ReportRow[], x: number, top: number, w: number): number => {
    let yy = top;
    els.push(<text key={`${key}-t`} x={x} y={yy} fill={t.title} fontSize={14} fontWeight={800}>{title}</text>);
    els.push(<line key={`${key}-l`} x1={x} x2={x + w} y1={yy + 9} y2={yy + 9} stroke={t.cellStroke} />);
    yy += 30;
    rows.forEach((row, i) => {
      els.push(<text key={`${key}-${i}-a`} x={x} y={yy} fill={t.textMuted} fontSize={12}>{row.label}</text>);
      els.push(<text key={`${key}-${i}-b`} x={x + w} y={yy} fill={t.text} fontSize={12} textAnchor="end" fontFamily={MONO_STACK}>{row.value}</text>);
      if (row.note) {
        els.push(<text key={`${key}-${i}-c`} x={x + w} y={yy + NOTE_H} fill={t.textMuted} fontSize={10} textAnchor="end">{row.note}</text>);
        yy += NOTE_H;
      }
      yy += ROW;
    });
    return yy - top;
  };

  /** 표. 반환값: 그린 높이 */
  const table = (key: string, title: string, header: string[], rows: string[][], top: number): number => {
    const w = SHEET_W - M * 2;
    const cw = w / header.length;
    els.push(<text key={`${key}-t`} x={M} y={top} fill={t.title} fontSize={14} fontWeight={800}>{title}</text>);
    const ht = top + 12;
    els.push(<rect key={`${key}-h`} x={M} y={ht} width={w} height={ROW} rx={4} fill={t.cell} />);
    header.forEach((h, i) => els.push(
      <text key={`${key}-h${i}`} x={M + cw * i + 10} y={ht + ROW * 0.68} fill={t.textMuted} fontSize={11.5} fontWeight={700}>{h}</text>,
    ));
    rows.forEach((row, ri) => {
      const ry = ht + ROW * (ri + 1);
      els.push(<line key={`${key}-r${ri}`} x1={M} x2={M + w} y1={ry + ROW} y2={ry + ROW} stroke={t.cellStroke} strokeOpacity={0.5} />);
      row.forEach((v, ci) => els.push(
        <text key={`${key}-${ri}-${ci}`} x={M + cw * ci + 10} y={ry + ROW * 0.68} fill={t.text} fontSize={11.5}
          fontFamily={ci === 0 ? FONT_STACK : MONO_STACK} fontWeight={ci === 0 ? 700 : 400}>{v}</text>,
      ));
    });
    if (rows.length === 0) {
      els.push(<text key={`${key}-empty`} x={M + 10} y={ht + ROW * 1.68} fill={t.textMuted} fontSize={11.5}>배선 없음</text>);
    }
    return 12 + ROW * (Math.max(1, rows.length) + 1);
  };

  if (part === 'all' || part === 'summary') {
    const colW = (SHEET_W - M * 2 - GAP) / 2;
    const h1 = kv('sum', '화면 / 장비', r.summary, M, y, colW);
    const h2 = kv('pow', '전력', r.power, M + colW + GAP, y, colW);
    y += Math.max(h1, h2) + GAP;
    y += kv('bom', '자재 수량', r.bom, M, y, SHEET_W - M * 2) + GAP;

    els.push(<text key="is-t" x={M} y={y} fill={t.title} fontSize={14} fontWeight={800}>확인 사항</text>);
    y += 28;
    if (issues.length === 0) {
      els.push(<text key="is-none" x={M} y={y} fill={t.textMuted} fontSize={12}>경고 없음</text>);
      y += ROW;
    }
    issues.forEach((e, i) => {
      els.push(
        <text key={`is-${i}`} x={M} y={y} fill={e.severity === 'error' ? errColor : warnColor} fontSize={12}>
          {e.severity === 'error' ? '■ 오류  ' : '▲ 주의  '}{e.message}
        </text>,
      );
      y += ROW;
    });
    y += GAP;
  }

  if (part === 'all' || part === 'ports') {
    const rows = part === 'all' ? r.ports : r.ports.slice(rowFrom, rowTo);
    y += table('ports', '데이터 포트', portHeader, rows, y) + GAP + 8;
  }
  if (part === 'all' || part === 'circuits') {
    const rows = part === 'all' ? r.circuits : r.circuits.slice(rowFrom, rowTo);
    y += table('circ', '전원 회로', circuitHeader, rows, y) + GAP + 8;
  }

  // PDF 쪽은 A4 세로 비율로 맞춰서 모든 쪽의 글자 크기를 같게
  const H = minPage ? Math.max(y + 30, PAGE_H) : y + 30;
  els.push(
    <text key="foot" x={M} y={H - 16} fill={t.textMuted} fontSize={10}>
      본 계산은 참고용입니다. 장비 사양은 제조사 사양서, 전기 공사는 전기 관련 법규와 자격자 확인을 따르십시오.
    </text>,
  );

  return (
    <svg xmlns="http://www.w3.org/2000/svg" width={SHEET_W} height={H} viewBox={`0 0 ${SHEET_W} ${H}`} fontFamily={FONT_STACK}>
      <rect width={SHEET_W} height={H} fill={t.bg} />
      {els}
    </svg>
  );
}
