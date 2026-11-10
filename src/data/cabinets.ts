import type { CabinetSpec } from '../types';

/**
 * 캐비닛 템플릿 (일반적인 규격 예시)
 * 전력·무게는 제조사마다 다르므로 실제 캐비닛 사양서 값으로 반드시 수정해서 사용
 */
export const cabinetTemplates: CabinetSpec[] = [
  // 320×160 모듈(P1.86, 172×86px) 가로 2 × 세로 3 = 6개
  { id: 'tpl-640480-p186', name: '640×480 P1.86 (모듈 6개)', widthPx: 344, heightPx: 258, widthMm: 640, heightMm: 480,
    powerMaxW: 260, powerAvgW: 90, weightKg: 8 },
  { id: 'tpl-500-p26', name: '500×500 P2.6', widthPx: 192, heightPx: 192, widthMm: 500, heightMm: 500,
    powerMaxW: 200, powerAvgW: 70, weightKg: 7.5 },
  { id: 'tpl-500-p29', name: '500×500 P2.9', widthPx: 168, heightPx: 168, widthMm: 500, heightMm: 500,
    powerMaxW: 200, powerAvgW: 70, weightKg: 7.5 },
  { id: 'tpl-500-p39', name: '500×500 P3.9', widthPx: 128, heightPx: 128, widthMm: 500, heightMm: 500,
    powerMaxW: 200, powerAvgW: 70, weightKg: 7.5 },
  { id: 'tpl-5001000-p39', name: '500×1000 P3.9', widthPx: 128, heightPx: 256, widthMm: 500, heightMm: 1000,
    powerMaxW: 400, powerAvgW: 140, weightKg: 13 },
  { id: 'tpl-600337-p125', name: '600×337.5 P1.25 (16:9)', widthPx: 480, heightPx: 270, widthMm: 600, heightMm: 337.5,
    powerMaxW: 150, powerAvgW: 50, weightKg: 6 },
  { id: 'tpl-640480-p25', name: '640×480 P2.5', widthPx: 256, heightPx: 192, widthMm: 640, heightMm: 480,
    powerMaxW: 260, powerAvgW: 90, weightKg: 8 },
  { id: 'tpl-960-p5', name: '960×960 P5', widthPx: 192, heightPx: 192, widthMm: 960, heightMm: 960,
    powerMaxW: 600, powerAvgW: 200, weightKg: 30 },
];
