import type { Phase } from '../../types';

/**
 * 배선도 색상
 * SVG는 PNG/PDF로 내보낼 때 CSS를 쓸 수 없으므로 실제 색상값을 직접 사용한다.
 */
export type DiagramTheme = 'dark' | 'light';

export interface ThemeColors {
  bg: string;
  cell: string;
  cellStroke: string;
  text: string;
  textMuted: string;
  ruler: string;
  title: string;
  /** 강조색 (화면 테두리, 커서) */
  accent: string;
}

export const themeColors: Record<DiagramTheme, ThemeColors> = {
  dark: {
    bg: '#050507',
    cell: '#0f0f13',
    cellStroke: '#26262e',
    text: '#ecebf1',
    textMuted: '#7a7787',
    ruler: '#45434f',
    title: '#ffffff',
    accent: '#b04cf5',
  },
  light: {
    bg: '#ffffff',
    cell: '#f4f3f7',
    cellStroke: '#b9b5c6',
    text: '#141218',
    textMuted: '#57525f',
    ruler: '#8c879a',
    title: '#000000',
    accent: '#a020f0',
  },
};

/** 포트별 색상 (구분이 잘 되는 순서로 배치, 20개 초과 시 반복) */
const PORT_COLORS = [
  '#35e0b8', '#6aa7ff', '#ffb547', '#ff6b9a', '#a98bff',
  '#5fd3ff', '#ffd84d', '#ff8a5c', '#7be36b', '#e27bff',
  '#2bc4a0', '#4f86e8', '#e8992b', '#e8507f', '#8d6ef0',
  '#3cb8e6', '#e6bf2e', '#e86e3e', '#5cc84c', '#c65ee6',
];

/**
 * 흰 배경용: 같은 색 계열을 어둡게 (검정과 섞음) → 연한 색이 흰 배경에서 묻히지 않게
 */
function deepen(hex: string, k = 0.3): string {
  const n = parseInt(hex.slice(1), 16);
  const ch = [(n >> 16) & 255, (n >> 8) & 255, n & 255].map((v) => Math.round(v * (1 - k)));
  return `#${ch.map((v) => v.toString(16).padStart(2, '0')).join('')}`;
}

export function portColor(i: number, theme: DiagramTheme = 'dark'): string {
  const c = PORT_COLORS[i % PORT_COLORS.length];
  return theme === 'light' ? deepen(c) : c;
}

/** 3상 색상 (R/S/T 표기 병행) */
export const phaseColor: Record<Phase, string> = {
  L1: '#ff6b6b',
  L2: '#ffc547',
  L3: '#5b9dff',
};

export function phaseColorFor(ph: Phase, theme: DiagramTheme = 'dark'): string {
  return theme === 'light' ? deepen(phaseColor[ph], 0.22) : phaseColor[ph];
}

export const phaseName: Record<Phase, string> = {
  L1: 'L1 (R)',
  L2: 'L2 (S)',
  L3: 'L3 (T)',
};

export const FONT_STACK = "'Pretendard Variable', Pretendard, 'Malgun Gothic', 'Apple SD Gothic Neo', sans-serif";
export const MONO_STACK = "'JetBrains Mono', Consolas, monospace";
