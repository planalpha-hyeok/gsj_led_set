/** 공통 타입 정의 */

export type Brand = 'colorlight' | 'novastar' | 'custom';

/**
 * 사양 출처 등급
 * - official: 제조사 공식 사양서/공식 페이지에서 확인
 * - reseller: 판매처·유통사 자료로만 확인 (공식 사양서와 다를 수 있음)
 * - unverified: 확인되지 않음 → 현장 사용 전 반드시 사양서 확인
 * - user: 사용자가 직접 입력
 */
export type SpecSource = 'official' | 'reseller' | 'unverified' | 'user';

export type ControllerCategory = 'sender' | 'all-in-one' | 'player' | 'processor';

export type PortType = '1G' | '5G';

export interface ControllerSpec {
  id: string;
  brand: Brand;
  model: string;
  series: string;
  category: ControllerCategory;
  /** 출력 이더넷 포트 수 */
  ports: number;
  portType: PortType;
  /** 포트당 최대 픽셀 (8bit, 60Hz 기준). 확인되지 않으면 null → 총용량 ÷ 포트수로 계산 */
  pixelsPerPort: number | null;
  /** 장비 1대 최대 픽셀 */
  maxPixels: number;
  /** 최대 가로/세로 픽셀 (확인된 경우만) */
  maxWidth?: number;
  maxHeight?: number;
  source: SpecSource;
  note?: string;
}

export interface ReceivingCardSpec {
  id: string;
  brand: Brand;
  model: string;
  series: string;
  /** 최대 부하 픽셀 수 (PWM IC 기준이 일반적, 계산에 사용) */
  maxPixels: number;
  /** 사양서 표기 그대로 (예: "512×384") */
  loadText: string;
  source: SpecSource;
  note?: string;
}

export interface CabinetSpec {
  id: string;
  name: string;
  widthPx: number;
  heightPx: number;
  widthMm: number;
  heightMm: number;
  powerMaxW: number;
  powerAvgW: number;
  weightKg: number;
}

export type PowerSystem = 'single' | 'three';
export type PowerBasis = 'max' | 'avg';

export interface PowerSettings {
  system: PowerSystem;
  /** 상전압 (단상 220V / 3상 380V 계통의 상전압 220V) */
  phaseVoltage: number;
  /** 3상 선간전압 */
  lineVoltage: number;
  powerFactor: number;
  breakerA: number;
  /** 연속 사용 기준 차단기 사용률 (예: 0.8) */
  derate: number;
  /** 전원 케이블/커넥터 1라인 허용 전류 */
  chainLimitA: number;
  basis: PowerBasis;
}

export type Phase = 'L1' | 'L2' | 'L3';

/** 수동 데이터 배선: 포트 1개가 연결하는 캐비닛 순서 */
export interface DataRun {
  id: string;
  /** 컨트롤러 번호 (0부터) */
  unit: number;
  /** 포트 번호 (1부터) */
  port: number;
  cells: Cell[];
}

/** 수동 전원 배선: 회로 1개가 연결하는 캐비닛 순서 */
export interface PowerRun {
  id: string;
  phase: Phase;
  cells: Cell[];
}

export type BitDepth = 8 | 10 | 12;

export interface Project {
  version: 2;
  id: string;
  name: string;
  updatedAt: number;
  cabinet: CabinetSpec;
  cols: number;
  rows: number;
  controllerId: string;
  receivingCardId: string;
  bitDepth: BitDepth;
  frameRate: number;
  power: PowerSettings;
  dataRuns: DataRun[];
  powerRuns: PowerRun[];
  customControllers: ControllerSpec[];
  customCards: ReceivingCardSpec[];
}

export interface Cell {
  c: number;
  r: number;
}

export type Severity = 'error' | 'warning' | 'info';

export interface Issue {
  severity: Severity;
  message: string;
}
