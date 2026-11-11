import type { ControllerSpec, Project, ReceivingCardSpec, SpecSource } from '../types';
import { colorlightCards, colorlightControllers } from './colorlight';
import { novastarCards, novastarControllers } from './novastar';

export const builtInControllers: ControllerSpec[] = [...novastarControllers, ...colorlightControllers];
export const builtInCards: ReceivingCardSpec[] = [...novastarCards, ...colorlightCards];

export function allControllers(p: Project): ControllerSpec[] {
  return [...builtInControllers, ...p.customControllers];
}

export function allCards(p: Project): ReceivingCardSpec[] {
  return [...builtInCards, ...p.customCards];
}

export function findController(p: Project): ControllerSpec | undefined {
  return allControllers(p).find((c) => c.id === p.controllerId);
}

export function findCard(p: Project): ReceivingCardSpec | undefined {
  return allCards(p).find((c) => c.id === p.receivingCardId);
}

export const sourceLabel: Record<SpecSource, string> = {
  official: '공식 사양',
  reseller: '판매처 자료',
  unverified: '미확인',
  user: '사용자 입력',
};

export const brandLabel = { novastar: 'NovaStar', colorlight: 'Colorlight', custom: '사용자' } as const;

export const categoryLabel = {
  sender: '송출기',
  'all-in-one': '올인원',
  player: '플레이어',
  processor: '프로세서',
} as const;
