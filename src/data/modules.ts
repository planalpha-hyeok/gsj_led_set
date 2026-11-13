/**
 * LED 모듈 목록 (파인피치 위주)
 * 모듈 해상도 = 모듈 크기 ÷ 피치 (제조사 표기 기준 값)
 * 250×250 모듈은 P2 이하만 사용 (현장 사용 기준)
 */
export interface ModuleSpec {
  id: string;
  widthMm: number;
  heightMm: number;
  pitch: string;
  widthPx: number;
  heightPx: number;
}

export const modules: ModuleSpec[] = [
  { id: 'm320-p125', widthMm: 320, heightMm: 160, pitch: 'P1.25', widthPx: 256, heightPx: 128 },
  { id: 'm320-p153', widthMm: 320, heightMm: 160, pitch: 'P1.53', widthPx: 208, heightPx: 104 },
  { id: 'm320-p1667', widthMm: 320, heightMm: 160, pitch: 'P1.667', widthPx: 192, heightPx: 96 },
  { id: 'm320-p186', widthMm: 320, heightMm: 160, pitch: 'P1.86', widthPx: 172, heightPx: 86 },
  { id: 'm320-p20', widthMm: 320, heightMm: 160, pitch: 'P2.0', widthPx: 160, heightPx: 80 },
  { id: 'm320-p25', widthMm: 320, heightMm: 160, pitch: 'P2.5', widthPx: 128, heightPx: 64 },
  { id: 'm250-p125', widthMm: 250, heightMm: 250, pitch: 'P1.25', widthPx: 200, heightPx: 200 },
  { id: 'm250-p156', widthMm: 250, heightMm: 250, pitch: 'P1.56', widthPx: 160, heightPx: 160 },
  { id: 'm250-p195', widthMm: 250, heightMm: 250, pitch: 'P1.95', widthPx: 128, heightPx: 128 },
];

export function findModule(id: string | undefined): ModuleSpec | undefined {
  return modules.find((m) => m.id === id);
}

/** 자주 쓰는 캐비닛 배치 (모듈 가로 × 세로 장수). 모듈 크기가 맞는 것만 표시 */
export interface LayoutPreset {
  moduleW: number;
  moduleH: number;
  cols: number;
  rows: number;
}

export const layoutPresets: LayoutPreset[] = [
  { moduleW: 320, moduleH: 160, cols: 2, rows: 3 }, // 640×480
  { moduleW: 320, moduleH: 160, cols: 2, rows: 4 }, // 640×640
  { moduleW: 320, moduleH: 160, cols: 1, rows: 3 }, // 320×480
  { moduleW: 320, moduleH: 160, cols: 1, rows: 4 }, // 320×640
  { moduleW: 320, moduleH: 160, cols: 3, rows: 3 }, // 960×480
  { moduleW: 320, moduleH: 160, cols: 3, rows: 4 }, // 960×640
  { moduleW: 250, moduleH: 250, cols: 2, rows: 2 }, // 500×500
  { moduleW: 250, moduleH: 250, cols: 2, rows: 4 }, // 500×1000
];
