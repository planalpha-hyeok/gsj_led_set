import { findCard, findController } from '../data';
import type { ControllerSpec, Issue, Project, ReceivingCardSpec } from '../types';
import { buildDataMap, type DataMapResult } from './dataMapping';
import { computePower, recommendBreaker, type PowerTotals } from './powerCalc';
import { buildCircuits, type CircuitResult } from './powerCircuit';
import { checkReceivingCard, type CardResult } from './receivingCard';
import { computeScreen, type ScreenInfo } from './screenLayout';
import { computePhaseLoads, type PhaseResult } from './threePhase';

export interface Analysis {
  screen: ScreenInfo;
  controller?: ControllerSpec;
  card?: ReceivingCardSpec;
  data?: DataMapResult;
  cardResult?: CardResult;
  power: PowerTotals;
  circuits: CircuitResult;
  phases: PhaseResult;
  /** 메인 차단기 추천 (단상: 총전류, 3상: 최대 상전류 기준) */
  mainBreakerA: number | null;
  issues: Issue[];
}

/** 프로젝트 전체 계산 (화면에서 사용하는 유일한 진입점) */
export function analyze(p: Project): Analysis {
  const issues: Issue[] = [];
  const screen = computeScreen(p);
  const controller = findController(p);
  const card = findCard(p);

  let data: DataMapResult | undefined;
  if (controller) {
    data = buildDataMap(p, controller);
    issues.push(...data.issues);
  } else {
    issues.push({ severity: 'error', message: '컨트롤러를 선택하세요' });
  }

  let cardResult: CardResult | undefined;
  if (card) {
    cardResult = checkReceivingCard(p, card);
    issues.push(...cardResult.issues);
  } else {
    issues.push({ severity: 'error', message: '수신카드를 선택하세요' });
  }

  if (controller && card && controller.brand !== 'custom' && card.brand !== 'custom' && controller.brand !== card.brand) {
    issues.push({ severity: 'error', message: '컨트롤러와 수신카드 제조사가 다릅니다 (NovaStar ↔ Colorlight 호환 안 됨)' });
  }

  for (const spec of [controller, card]) {
    if (spec?.source === 'reseller') {
      issues.push({ severity: 'info', message: `${spec.model}: 판매처 자료 기준 사양 → 공식 사양서로 확인 권장` });
    }
  }

  const power = computePower(p);
  if (power.cabinetW <= 0) {
    issues.push({ severity: 'error', message: '캐비닛 전력이 0입니다. SMPS 또는 ㎡당 W 값을 입력하세요' });
  }
  const circuits = buildCircuits(p, power.cabinetW, power.cabinetA);
  issues.push(...circuits.issues);
  const three = p.power.system === 'three';
  const phases = computePhaseLoads(circuits.circuits, three);
  issues.push(...phases.issues);

  const mainCurrent = three ? Math.max(...phases.phases.map((x) => x.currentA)) : power.singlePhaseA;
  const mainBreakerA = recommendBreaker(mainCurrent, p.power.derate);
  if (mainBreakerA === null) {
    issues.push({ severity: 'warning', message: `메인 전류 ${mainCurrent.toFixed(0)}A → 분전반을 여러 개로 나눠야 함` });
  }

  return { screen, controller, card, data, cardResult, power, circuits, phases, mainBreakerA, issues };
}
