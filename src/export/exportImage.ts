import { jsPDF } from 'jspdf';
import type { Analysis } from '../core/analyze';
import type { Project } from '../types';
import type { DiagramTheme } from '../components/diagram/palette';
import { downloadDataUrl, safeName } from './download';
import { PAGE_H, ROWS_PER_PAGE } from '../components/report/ReportSheet';
import { diagramTitle, renderReportSvg, renderSvg, svgHeight, svgToImage, type DiagramKind } from './svgRender';

/** 현재 배선도를 PNG로 저장 */
export async function exportPng(kind: DiagramKind, p: Project, a: Analysis, theme: DiagramTheme): Promise<void> {
  const { dataUrl } = await svgToImage(renderSvg(kind, p, a, theme), { scale: 2, maxSide: 8000, format: 'png' });
  downloadDataUrl(dataUrl, `${safeName(p.name)}_${diagramTitle[kind]}.png`);
}

/**
 * 전체 PDF (A4 세로, 항상 흰 배경)
 * 요약 → 데이터 포트 표 → 전원 회로 표 (행이 많으면 여러 쪽) → 화면 배치 → 데이터 배선 → 전원 배선
 * 한글 글꼴 문제를 피하기 위해 각 페이지를 이미지로 넣는다.
 * PNG를 압축(FAST)해서 넣는다. JPEG는 흰 배경이 회색으로 번져 보여서 사용하지 않음. 긴 변 3600px ≈ A4 300dpi.
 */
export async function exportPdf(p: Project, a: Analysis): Promise<void> {
  const pdf = new jsPDF({ orientation: 'portrait', unit: 'mm', format: 'a4' });
  const pageW = pdf.internal.pageSize.getWidth();
  const pageH = pdf.internal.pageSize.getHeight();
  const margin = 10;

  // 요약과 표가 한 쪽에 들어가면 한 쪽으로, 넘치면 요약 1쪽 + 표를 38행씩 나눠서
  const pages: string[] = [];
  const onePage = renderReportSvg(p, a, 'light', { part: 'all', minPage: true });
  if (svgHeight(onePage) <= PAGE_H) {
    pages.push(onePage);
  } else {
    pages.push(renderReportSvg(p, a, 'light', { part: 'summary', minPage: true }));
    const tablePages = (part: 'ports' | 'circuits', total: number, title: string) => {
      const n = Math.max(1, Math.ceil(total / ROWS_PER_PAGE));
      for (let i = 0; i < n; i++) {
        pages.push(renderReportSvg(p, a, 'light', {
          part, rowFrom: i * ROWS_PER_PAGE, rowTo: (i + 1) * ROWS_PER_PAGE, minPage: true,
          pageNote: n > 1 ? `${title} ${i + 1}/${n}` : undefined,
        }));
      }
    };
    tablePages('ports', a.data?.ports.length ?? 0, '데이터 포트');
    tablePages('circuits', a.circuits.circuits.length, '전원 회로');
  }
  for (const kind of ['screen', 'mapping', 'power'] as DiagramKind[]) pages.push(renderSvg(kind, p, a, 'light'));

  for (let i = 0; i < pages.length; i++) {
    const { dataUrl, width, height } = await svgToImage(pages[i], { scale: 3, maxSide: 3600, format: 'png' });
    if (i > 0) pdf.addPage();
    // 페이지 폭에 맞추고, 너무 길면 높이에 맞춤. 위쪽 정렬
    const scale = Math.min((pageW - margin * 2) / width, (pageH - margin * 2 - 6) / height);
    const w = width * scale;
    const h = height * scale;
    pdf.addImage(dataUrl, 'PNG', (pageW - w) / 2, margin, w, h, undefined, 'FAST');
    pdf.setFontSize(8);
    pdf.setTextColor(140);
    pdf.text(`${i + 1} / ${pages.length}`, pageW - margin, pageH - 5, { align: 'right' });
  }
  pdf.save(`${safeName(p.name)}_LED배선도.pdf`);
}
