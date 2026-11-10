import type { Cell, Corner, RouteSettings } from '../types';

/**
 * 캐비닛 연결 경로 생성 (데이터·전원 공용)
 * 결과: "줄" 배열. 가로 방향이면 줄 = 행, 세로 방향이면 줄 = 열.
 * 각 줄 안의 셀은 케이블이 지나가는 순서대로 정렬된다.
 */
export function generateLines(cols: number, rows: number, route: Pick<RouteSettings, 'start' | 'direction' | 'pattern'>): Cell[][] {
  const { start, direction, pattern } = route;
  const fromLeft = start === 'TL' || start === 'BL';
  const fromTop = start === 'TL' || start === 'TR';

  const lineCount = direction === 'horizontal' ? rows : cols;
  const lineLen = direction === 'horizontal' ? cols : rows;
  const lines: Cell[][] = [];

  for (let i = 0; i < lineCount; i++) {
    // 줄 순서: 시작 모서리 쪽 줄부터
    const lineIdx = direction === 'horizontal'
      ? (fromTop ? i : rows - 1 - i)
      : (fromLeft ? i : cols - 1 - i);

    // 줄 안 진행 방향: S자는 줄마다 반전, Z자는 항상 같은 방향
    const firstForward = direction === 'horizontal' ? fromLeft : fromTop;
    const forward = pattern === 'serpentine' && i % 2 === 1 ? !firstForward : firstForward;

    const line: Cell[] = [];
    for (let j = 0; j < lineLen; j++) {
      const k = forward ? j : lineLen - 1 - j;
      line.push(direction === 'horizontal' ? { c: k, r: lineIdx } : { c: lineIdx, r: k });
    }
    lines.push(line);
  }
  return lines;
}

/**
 * 경로를 그룹(포트 또는 전원 회로)으로 나눈다.
 * - line: 줄 단위로 나눔. 한 줄이 용량보다 길면 그 줄을 잘라서 나눔.
 * - fill: 전체 경로를 이어서 용량까지 채움.
 */
export function splitIntoGroups(lines: Cell[][], maxPerGroup: number, mode: 'line' | 'fill'): Cell[][] {
  if (maxPerGroup < 1 || lines.length === 0) return [];
  const lineLen = lines[0].length;

  if (mode === 'fill') {
    return chunk(lines.flat(), maxPerGroup);
  }

  if (maxPerGroup >= lineLen) {
    const linesPerGroup = Math.floor(maxPerGroup / lineLen);
    return chunk(lines, linesPerGroup).map((g) => g.flat());
  }

  // 한 줄이 그룹 용량보다 긴 경우: 줄마다 따로 자름
  return lines.flatMap((line) => chunk(line, maxPerGroup));
}

function chunk<T>(arr: T[], size: number): T[][] {
  const out: T[][] = [];
  for (let i = 0; i < arr.length; i += size) out.push(arr.slice(i, i + size));
  return out;
}

export const cornerLabel: Record<Corner, string> = {
  TL: '좌상단',
  TR: '우상단',
  BL: '좌하단',
  BR: '우하단',
};
