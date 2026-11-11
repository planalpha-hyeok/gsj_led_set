import type { ControllerSpec, ReceivingCardSpec } from '../types';

/**
 * Colorlight 제품 사양
 * - 포트당 650,000 픽셀 (8bit / 60Hz 기준, 판매처 자료 공통 값)
 * - source 등급: official = 공식 사양서 확인, reseller = 판매처 자료로만 확인
 * - 공식/판매처 자료로 확인되지 않은 모델은 넣지 않았음 → "사용자 장비 추가"로 입력
 */
const GBE = 650_000;

export const colorlightControllers: ControllerSpec[] = [
  // ── S 시리즈 송출기 ──
  { id: 'cl-s2', brand: 'colorlight', model: 'S2', series: 'S', category: 'sender',
    ports: 2, portType: '1G', pixelsPerPort: GBE, maxPixels: 1_310_000, maxWidth: 2560, maxHeight: 2560,
    source: 'official', note: '입력 최대 1920×1200' },

  // ── X 시리즈 컨트롤러 ──
  { id: 'cl-x2', brand: 'colorlight', model: 'X2', series: 'X', category: 'all-in-one',
    ports: 2, portType: '1G', pixelsPerPort: GBE, maxPixels: 1_300_000, source: 'reseller',
    note: '입력 최대 1920×1200' },
  { id: 'cl-x4', brand: 'colorlight', model: 'X4', series: 'X', category: 'all-in-one',
    ports: 4, portType: '1G', pixelsPerPort: GBE, maxPixels: 2_300_000, source: 'reseller' },
  { id: 'cl-x4m', brand: 'colorlight', model: 'X4m', series: 'X', category: 'all-in-one',
    ports: 4, portType: '1G', pixelsPerPort: 655_360, maxPixels: 2_600_000, maxWidth: 3840, maxHeight: 2000,
    source: 'reseller', note: '포트당 655,360px. 입력 최대 1920×1080@60Hz' },
  { id: 'cl-x8m', brand: 'colorlight', model: 'X8m', series: 'X', category: 'all-in-one',
    ports: 8, portType: '1G', pixelsPerPort: GBE, maxPixels: 5_240_000, source: 'reseller',
    note: '입력 최대 4096×2160@60Hz' },
  { id: 'cl-x16', brand: 'colorlight', model: 'X16', series: 'X', category: 'all-in-one',
    ports: 16, portType: '1G', pixelsPerPort: GBE, maxPixels: 8_880_000, maxWidth: 8192, maxHeight: 8192,
    source: 'reseller', note: '판매처마다 10.48M / 11.8M 표기도 있음. 가장 일치하는 8.88M 적용' },
  { id: 'cl-x20', brand: 'colorlight', model: 'X20', series: 'X', category: 'all-in-one',
    ports: 20, portType: '1G', pixelsPerPort: GBE, maxPixels: 13_000_000, maxWidth: 16384, maxHeight: 8192,
    source: 'reseller' },

  // ── A 시리즈 플레이어 ──
  { id: 'cl-a100', brand: 'colorlight', model: 'A100', series: 'A', category: 'player',
    ports: 2, portType: '1G', pixelsPerPort: GBE, maxPixels: 2_300_000, source: 'reseller',
    note: '표기 총용량 230만이나 포트 2개 × 65만 = 130만이 실제 상한' },
  { id: 'cl-a200', brand: 'colorlight', model: 'A200', series: 'A', category: 'player',
    ports: 4, portType: '1G', pixelsPerPort: GBE, maxPixels: 2_300_000, maxWidth: 4096, maxHeight: 2560,
    source: 'reseller' },
];

export const colorlightCards: ReceivingCardSpec[] = [
  { id: 'cl-k5plus', brand: 'colorlight', model: 'K5+', series: 'K', maxPixels: 512 * 384,
    loadText: '512×384', source: 'reseller', note: '정밀 피치용. 공식 사양서 미확인' },
  { id: 'cl-5a75b', brand: 'colorlight', model: '5A-75B', series: '5A', maxPixels: 192 * 1024,
    loadText: '192×1024 (PWM) / 128×1024 (일반 IC)', source: 'reseller' },
  { id: 'cl-5a75e', brand: 'colorlight', model: '5A-75E', series: '5A', maxPixels: 128 * 512,
    loadText: '128×512 (보수적 값)', source: 'reseller',
    note: '판매처마다 128×512 / 256×1024로 다름 → 낮은 값 적용' },
  { id: 'cl-e80', brand: 'colorlight', model: 'E80', series: 'E', maxPixels: 128 * 1024,
    loadText: '128×1024', source: 'reseller' },
  { id: 'cl-e120', brand: 'colorlight', model: 'E120', series: 'E', maxPixels: 192 * 1024,
    loadText: '192×1024 (PWM) / 128×1024 (일반 IC)', source: 'reseller' },
  { id: 'cl-e320', brand: 'colorlight', model: 'E320', series: 'E', maxPixels: 256 * 1024,
    loadText: '256×1024', source: 'reseller' },
  { id: 'cl-i5a905', brand: 'colorlight', model: 'i5A-905', series: 'i', maxPixels: 256 * 256,
    loadText: '256×256 (특수 설정 시 열 1024까지)', source: 'reseller' },
  { id: 'cl-i9', brand: 'colorlight', model: 'i9', series: 'i', maxPixels: 256 * 1024,
    loadText: '256×1024', source: 'reseller', note: '단일 자료로만 확인' },
];
