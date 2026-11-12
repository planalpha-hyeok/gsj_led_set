import { jsPDF } from 'jspdf';
import type { Analysis } from '../core/analyze';
import type { Project } from '../types';
import type { DiagramTheme } from '../components/diagram/palette';
import { downloadDataUrl, safeName } from './download';
import { ROWS_PER_PAGE } from '../components/report/ReportSheet';
import { diagramTitle, renderReportSvg, renderSvg, svgToImage, type DiagramKind } from './svgRender';

/** 현재 배선도를 PNG로 저장 */
export async function exportPng(kind: DiagramKind, p: Project, a: Analysis, theme: DiagramTheme): Promise<void> {
  const { dataUrl } = await svgToImage(renderSvg(kind, p, a, theme), { scale: 2, maxSide: 8000, format: 'png' });
  downloadDataUrl(dataUrl, `${safeName(p.name)}_${diagramTitle[kind]}.png`);
}

/**
 * 전체 PDF (A4 세로, 항상 흰 배경)
 * 요약 → 데이터 포트 표 → 전원 회로 표 (행이 많으면 여러 쪽) → 화면 배치 → 데이터 배선 → 전원 배선
 * 한글 글꼴 문제를 피하기 위해 각 페이지를 이미지로 넣는다.
 * PNG는 jsPDF에서 압축되지 않아 파일이 매우 커지므로 JPEG 사용 (긴 변 4000px ≈ A4 300dpi 이상).
 */
export async function exportPdf(p: Project, a: Analysis): Promise<void> {
  const pdf = new jsPDF({ orientation: 'portrait', unit: 'mm', format: 'a4' });
  const pageW = pdf.internal.pageSize.getWidth();
  const pageH = pdf.internal.pageSize.getHeight();
  const margin = 10;

  const pages: string[] = [renderReportSvg(p, a, 'light', { part: 'summary' })];
  const tablePages = (part: 'ports' | 'circuits', total: number, title: string) => {
    const n = Math.max(1, Math.ceil(total / ROWS_PER_PAGE));
    for (let i = 0; i < n; i++) {
      pages.push(renderReportSvg(p, a, 'light', {
        part, rowFrom: i * ROWS_PER_PAGE, rowTo: (i + 1) * ROWS_PER_PAGE,
        pageNote: n > 1 ? `${title} ${i + 1}/${n}` : undefined,
      }));
    }
  };
  tablePages('ports', a.data?.ports.length ?? 0, '데이터 포트');
  tablePages('circuits', a.circuits.circuits.length, '전원 회로');
  for (const kind of ['screen', 'mapping', 'power'] as DiagramKind[]) pages.push(renderSvg(kind, p, a, 'light'));

  for (let i = 0; i < pages.length; i++) {
    const { dataUrl, width, height } = await svgToImage(pages[i], { scale: 3, maxSide: 4000, format: 'jpeg' });
    if (i > 0) pdf.addPage();
    // 페이지 폭에 맞추고, 너무 길면 높이에 맞춤. 위쪽 정렬
    const scale = Math.min((pageW - margin * 2) / width, (pageH - margin * 2 - 6) / height);
    const w = width * scale;
    const h = height * scale;
    pdf.addImage(dataUrl, 'JPEG', (pageW - w) / 2, margin, w, h);
    pdf.setFontSize(8);
    pdf.setTextColor(140);
    pdf.text(`${i + 1} / ${pages.length}`, pageW - margin, pageH - 5, { align: 'right' });
  }
  pdf.save(`${safeName(p.name)}_LED배선도.pdf`);
}
