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
}

export const themeColors: Record<DiagramTheme, ThemeColors> = {
  dark: {
    bg: '#0b0e12',
    cell: '#151a21',
    cellStroke: '#2a313c',
    text: '#e8ecf2',
    textMuted: '#7b8597',
    ruler: '#4a5465',
    title: '#ffffff',
  },
  light: {
    bg: '#ffffff',
    cell: '#f3f5f8',
    cellStroke: '#c5ccd6',
    text: '#111418',
    textMuted: '#5d6675',
    ruler: '#8e98a8',
    title: '#000000',
  },
};

/** 포트별 색상 (구분이 잘 되는 순서로 배치, 20개 초과 시 반복) */
const PORT_COLORS = [
  '#35e0b8', '#6aa7ff', '#ffb547', '#ff6b9a', '#a98bff',
  '#5fd3ff', '#ffd84d', '#ff8a5c', '#7be36b', '#e27bff',
  '#2bc4a0', '#4f86e8', '#e8992b', '#e8507f', '#8d6ef0',
  '#3cb8e6', '#e6bf2e', '#e86e3e', '#5cc84c', '#c65ee6',
];

export function portColor(i: number): string {
  return PORT_COLORS[i % PORT_COLORS.length];
}

/** 3상 색상 (R/S/T 표기 병행) */
export const phaseColor: Record<Phase, string> = {
  L1: '#ff6b6b',
  L2: '#ffc547',
  L3: '#5b9dff',
};

export const phaseName: Record<Phase, string> = {
  L1: 'L1 (R)',
  L2: 'L2 (S)',
  L3: 'L3 (T)',
};

export const FONT_STACK = "'Pretendard Variable', Pretendard, 'Malgun Gothic', 'Apple SD Gothic Neo', sans-serif";
export const MONO_STACK = "'JetBrains Mono', Consolas, monospace";
