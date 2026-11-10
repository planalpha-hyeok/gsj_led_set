import type { Issue, Project, ReceivingCardSpec } from '../types';

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
  const cabPx = p.cabinet.widthPx * p.cabinet.heightPx;
  const cardsPerCabinet = Math.max(1, Math.ceil(cabPx / card.maxPixels));
  const load = cabPx / (card.maxPixels * cardsPerCabinet);

  if (cardsPerCabinet > 1) {
    issues.push({
      severity: 'warning',
      message: `캐비닛 픽셀(${cabPx.toLocaleString()})이 ${card.model} 용량(${card.loadText})을 넘음 → 캐비닛당 ${cardsPerCabinet}장 필요`,
    });
  }
  return { cardsPerCabinet, totalCards: cardsPerCabinet * p.cols * p.rows, load, issues };
}
