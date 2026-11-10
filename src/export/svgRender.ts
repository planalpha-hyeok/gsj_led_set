import { createElement, type ReactElement } from 'react';
import { renderToStaticMarkup } from 'react-dom/server';
import type { Analysis } from '../core/analyze';
import type { Project } from '../types';
import type { DiagramTheme } from '../components/diagram/palette';
import { MappingDiagram } from '../components/mapping/MappingDiagram';
import { PowerDiagram } from '../components/power/PowerDiagram';
import { ScreenDiagram } from '../components/screen/ScreenDiagram';
import { ReportSheet } from '../components/report/ReportSheet';

export type DiagramKind = 'screen' | 'mapping' | 'power' | 'report' | 'report-summary' | 'report-tables';

export const diagramTitle: Record<DiagramKind, string> = {
  screen: '화면배치',
  mapping: '데이터매핑',
  power: '전원배선',
  report: '요약보고서',
  'report-summary': '요약',
  'report-tables': '포트회로표',
};

/** 배선도를 SVG 문자열로 생성 (화면에 그린 것과 같은 컴포넌트 사용) */
export function renderSvg(kind: DiagramKind, project: Project, analysis: Analysis, theme: DiagramTheme): string {
  const props = { project, analysis, theme };
  const el: ReactElement =
    kind === 'screen' ? createElement(ScreenDiagram, props)
      : kind === 'mapping' ? createElement(MappingDiagram, props)
        : kind === 'power' ? createElement(PowerDiagram, props)
          : createElement(ReportSheet, {
            ...props,
            part: kind === 'report-summary' ? 'summary' : kind === 'report-tables' ? 'tables' : 'all',
          });
  return renderToStaticMarkup(el);
}

export interface RasterOptions {
  /** 배율 (고해상도 출력) */
  scale?: number;
  /** 결과 이미지 긴 변 최대 픽셀 (브라우저 캔버스 한계·파일 크기 제한) */
  maxSide?: number;
  format?: 'png' | 'jpeg';
}

/** SVG 문자열 → 이미지 (data URL) */
export function svgToImage(svg: string, opts: RasterOptions = {}): Promise<{ dataUrl: string; width: number; height: number }> {
  const { scale = 2, maxSide = 8000, format = 'png' } = opts;
  return new Promise((resolve, reject) => {
    const m = svg.match(/width="([\d.]+)" height="([\d.]+)"/);
    const w = m ? Number(m[1]) : 1000;
    const h = m ? Number(m[2]) : 800;
    const k = Math.min(scale, maxSide / Math.max(w, h));
    const img = new Image();
    img.onload = () => {
      const canvas = document.createElement('canvas');
      canvas.width = Math.round(w * k);
      canvas.height = Math.round(h * k);
      const ctx = canvas.getContext('2d');
      if (!ctx) return reject(new Error('캔버스를 만들 수 없습니다'));
      ctx.scale(k, k);
      ctx.drawImage(img, 0, 0, w, h);
      const dataUrl = format === 'jpeg' ? canvas.toDataURL('image/jpeg', 0.92) : canvas.toDataURL('image/png');
      resolve({ dataUrl, width: w, height: h });
    };
    img.onerror = () => reject(new Error('이미지 변환 실패'));
    img.src = `data:image/svg+xml;charset=utf-8,${encodeURIComponent(svg)}`;
  });
}
