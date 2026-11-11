import { jsPDF } from 'jspdf';
import type { Analysis } from '../core/analyze';
import type { Project } from '../types';
import type { DiagramTheme } from '../components/diagram/palette';
import { downloadDataUrl, safeName } from './download';
import { diagramTitle, renderSvg, svgToImage, type DiagramKind } from './svgRender';

/** 현재 배선도를 PNG로 저장 */
export async function exportPng(kind: DiagramKind, p: Project, a: Analysis, theme: DiagramTheme): Promise<void> {
  const { dataUrl } = await svgToImage(renderSvg(kind, p, a, theme), { scale: 2, maxSide: 8000, format: 'png' });
  downloadDataUrl(dataUrl, `${safeName(p.name)}_${diagramTitle[kind]}.png`);
}

/**
 * 전체 PDF: 요약 → 포트·회로 표 → 화면 배치 → 데이터 매핑 → 전원 배선 (A4 가로)
 * 한글 글꼴 문제를 피하기 위해 각 페이지를 이미지로 넣는다.
 * PNG는 jsPDF에서 압축되지 않아 파일이 매우 커지므로 JPEG 사용 (긴 변 4000px ≈ A4 300dpi 이상).
 */
export async function exportPdf(p: Project, a: Analysis): Promise<void> {
  const pdf = new jsPDF({ orientation: 'landscape', unit: 'mm', format: 'a4' });
  const pageW = pdf.internal.pageSize.getWidth();
  const pageH = pdf.internal.pageSize.getHeight();
  const margin = 10;
  const kinds: DiagramKind[] = ['report-summary', 'report-tables', 'screen', 'mapping', 'power'];

  for (let i = 0; i < kinds.length; i++) {
    const { dataUrl, width, height } = await svgToImage(renderSvg(kinds[i], p, a, 'light'), { scale: 3, maxSide: 4000, format: 'jpeg' });
    if (i > 0) pdf.addPage();
    const scale = Math.min((pageW - margin * 2) / width, (pageH - margin * 2 - 6) / height);
    const w = width * scale;
    const h = height * scale;
    pdf.addImage(dataUrl, 'JPEG', (pageW - w) / 2, margin, w, h);
    pdf.setFontSize(8);
    pdf.setTextColor(140);
    pdf.text(`${i + 1} / ${kinds.length}`, pageW - margin, pageH - 5, { align: 'right' });
  }
  pdf.save(`${safeName(p.name)}_LED배선도.pdf`);
}
