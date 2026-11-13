import type { Issue, Project, ReceivingCardSpec } from '../types';
import { gridLayout } from './gridLayout';

export interface CardResult {
  cardsPerCabinet: number;
  totalCards: number;
  load: number;
  issues: Issue[];
}

/**
 * 수신카드 수량·부하 확인
 * 캐비닛 1개 픽셀이 카드 1장 용량을 넘으면 캐비닛당 필요한 장수를 늘린다.
 * (실제 모듈 결선은 캐비닛 설계에 따라 다르므로 참고값)
 */
export function checkReceivingCard(p: Project, card: ReceivingCardSpec): CardResult {
  const issues: Issue[] = [];
  // 가장 큰 캐비닛 기준으로 확인, 총 장수는 캐비닛마다 계산해서 합산
  const g = gridLayout(p);
  const cellPx = g.cols.flatMap((col) => g.rows.map((row) => col.px * row.px));
  const cabPx = Math.max(0, ...cellPx);
  const cardsPerCabinet = Math.max(1, Math.ceil(cabPx / card.maxPixels));
  const load = cabPx / (card.maxPixels * cardsPerCabinet);
  const totalCards = cellPx.reduce((s, px) => s + Math.max(1, Math.ceil(px / card.maxPixels)), 0);

  if (cardsPerCabinet > 1) {
    issues.push({
      severity: 'warning',
      message: `캐비닛 픽셀(${cabPx.toLocaleString()})이 ${card.model} 용량(${card.loadText})을 넘음 → 캐비닛당 ${cardsPerCabinet}장 필요`,
    });
  }
  return { cardsPerCabinet, totalCards, load, issues };
}
