import type { ControllerSpec, ReceivingCardSpec } from '../types';

/**
 * NovaStar 제품 사양
 * - 포트당 650,000 픽셀은 8bit / 60Hz 기준 (MCTRL4K 공식 페이지 기준 값)
 * - source 등급: official = 공식 사양서 확인, reseller = 판매처 자료로만 확인
 * - 공식/판매처 자료로 확인되지 않은 모델은 넣지 않았음 → "사용자 장비 추가"로 입력
 */
const GBE = 650_000;

export const novastarControllers: ControllerSpec[] = [
  // ── MCTRL 송출기 ──
  { id: 'ns-mctrl300', brand: 'novastar', model: 'MCTRL300', series: 'MCTRL', category: 'sender',
    ports: 2, portType: '1G', pixelsPerPort: GBE, maxPixels: 1_300_000, source: 'reseller',
    note: '포트 2개 × 65만. 입력 최대 1920×1200' },
  { id: 'ns-mctrl600', brand: 'novastar', model: 'MCTRL600', series: 'MCTRL', category: 'sender',
    ports: 4, portType: '1G', pixelsPerPort: GBE, maxPixels: 2_300_000, source: 'reseller' },
  { id: 'ns-mctrl660', brand: 'novastar', model: 'MCTRL660', series: 'MCTRL', category: 'sender',
    ports: 4, portType: '1G', pixelsPerPort: GBE, maxPixels: 2_300_000, source: 'reseller' },
  { id: 'ns-mctrl660pro', brand: 'novastar', model: 'MCTRL660 PRO', series: 'MCTRL', category: 'sender',
    ports: 6, portType: '1G', pixelsPerPort: GBE, maxPixels: 2_300_000, source: 'reseller',
    note: '장비 최대 1920×1200@60Hz. 10G 광포트 2개 별도' },
  { id: 'ns-mctrl700', brand: 'novastar', model: 'MCTRL700', series: 'MCTRL', category: 'sender',
    ports: 6, portType: '1G', pixelsPerPort: GBE, maxPixels: 2_300_000, source: 'reseller',
    note: '장비 최대 1920×1200@60Hz' },
  { id: 'ns-mctrl4k', brand: 'novastar', model: 'MCTRL4K', series: 'MCTRL', category: 'sender',
    ports: 16, portType: '1G', pixelsPerPort: GBE, maxPixels: 8_800_000, maxWidth: 7680, maxHeight: 7680,
    source: 'official', note: '10/12bit 입력 시 포트당 32만 픽셀' },

  // ── VX 올인원 컨트롤러 ──
  { id: 'ns-vx4sn', brand: 'novastar', model: 'VX4S-N', series: 'VX', category: 'all-in-one',
    ports: 4, portType: '1G', pixelsPerPort: GBE, maxPixels: 2_300_000, source: 'reseller' },
  { id: 'ns-vx400', brand: 'novastar', model: 'VX400', series: 'VX', category: 'all-in-one',
    ports: 4, portType: '1G', pixelsPerPort: GBE, maxPixels: 2_600_000, source: 'official' },
  { id: 'ns-vx400pro', brand: 'novastar', model: 'VX400 Pro', series: 'VX Pro', category: 'all-in-one',
    ports: 4, portType: '1G', pixelsPerPort: GBE, maxPixels: 2_600_000, source: 'reseller' },
  { id: 'ns-vx600', brand: 'novastar', model: 'VX600', series: 'VX', category: 'all-in-one',
    ports: 6, portType: '1G', pixelsPerPort: GBE, maxPixels: 3_900_000, source: 'reseller' },
  { id: 'ns-vx600pro', brand: 'novastar', model: 'VX600 Pro', series: 'VX Pro', category: 'all-in-one',
    ports: 6, portType: '1G', pixelsPerPort: GBE, maxPixels: 3_900_000, maxWidth: 10240, maxHeight: 8192,
    source: 'reseller' },
  { id: 'ns-vx1000', brand: 'novastar', model: 'VX1000', series: 'VX', category: 'all-in-one',
    ports: 10, portType: '1G', pixelsPerPort: GBE, maxPixels: 6_500_000, maxWidth: 10240, maxHeight: 8192,
    source: 'official' },
  { id: 'ns-vx1000pro', brand: 'novastar', model: 'VX1000 Pro', series: 'VX Pro', category: 'all-in-one',
    ports: 10, portType: '1G', pixelsPerPort: GBE, maxPixels: 6_500_000, source: 'reseller' },
  { id: 'ns-vx2000pro', brand: 'novastar', model: 'VX2000 Pro', series: 'VX Pro', category: 'all-in-one',
    ports: 20, portType: '1G', pixelsPerPort: GBE, maxPixels: 13_000_000, maxWidth: 16384, maxHeight: 8192,
    source: 'reseller', note: '10bit 시 포트당 32.5만~48만 (수신카드에 따라 다름)' },
  { id: 'ns-vx16s', brand: 'novastar', model: 'VX16S', series: 'VX', category: 'all-in-one',
    ports: 16, portType: '1G', pixelsPerPort: GBE, maxPixels: 10_400_000, source: 'reseller' },

  // ── COEX ──
  { id: 'ns-mx40pro', brand: 'novastar', model: 'MX40 Pro', series: 'COEX', category: 'processor',
    ports: 20, portType: '1G', pixelsPerPort: GBE, maxPixels: 9_000_000, source: 'official',
    note: '10bit 시 포트당 48만 (A10s Pro 사용 시)' },
  { id: 'ns-cx40pro', brand: 'novastar', model: 'CX40 Pro', series: 'COEX', category: 'processor',
    ports: 6, portType: '5G', pixelsPerPort: null, maxPixels: 9_000_000, source: 'official',
    note: '5G 포트. 포트당 용량은 사양서 미확인 → 총용량 ÷ 포트수로 계산' },

  // ── Taurus 멀티미디어 플레이어 ──
  { id: 'ns-tb30', brand: 'novastar', model: 'TB30', series: 'Taurus', category: 'player',
    ports: 1, portType: '1G', pixelsPerPort: GBE, maxPixels: 650_000, maxWidth: 4096, maxHeight: 4096,
    source: 'official', note: '이더넷 2개 중 1개는 백업용' },
  { id: 'ns-tb50', brand: 'novastar', model: 'TB50', series: 'Taurus', category: 'player',
    ports: 2, portType: '1G', pixelsPerPort: GBE, maxPixels: 1_300_000, maxWidth: 4096, maxHeight: 4096,
    source: 'reseller' },
  { id: 'ns-tb60', brand: 'novastar', model: 'TB60', series: 'Taurus', category: 'player',
    ports: 4, portType: '1G', pixelsPerPort: GBE, maxPixels: 2_300_000, maxWidth: 4096, maxHeight: 4096,
    source: 'official' },
];

export const novastarCards: ReceivingCardSpec[] = [
  { id: 'ns-dh7508', brand: 'novastar', model: 'DH7508', series: 'DH', maxPixels: 192 * 256,
    loadText: '192×256 (PWM) / 176×256 (일반 IC)', source: 'official' },
  { id: 'ns-mrv208-1', brand: 'novastar', model: 'MRV208-1', series: 'MRV', maxPixels: 256 * 256,
    loadText: '256×256', source: 'reseller' },
  { id: 'ns-mrv336', brand: 'novastar', model: 'MRV336', series: 'MRV', maxPixels: 256 * 256,
    loadText: '256×256', source: 'reseller', note: '일부 자료 256×226 표기 (오기로 추정)' },
  { id: 'ns-mrv412', brand: 'novastar', model: 'MRV412', series: 'MRV', maxPixels: 512 * 512,
    loadText: '512×512', source: 'reseller' },
  { id: 'ns-mrv432', brand: 'novastar', model: 'MRV432', series: 'MRV', maxPixels: 512 * 512,
    loadText: '512×512', source: 'official' },
  { id: 'ns-a5splus', brand: 'novastar', model: 'A5s Plus', series: 'Armor', maxPixels: 512 * 384,
    loadText: '512×384 (PWM) / 384×384 (일반 IC)', source: 'reseller' },
  { id: 'ns-a7splus', brand: 'novastar', model: 'A7s Plus', series: 'Armor', maxPixels: 512 * 512,
    loadText: '512×512', source: 'reseller' },
  { id: 'ns-a8sn', brand: 'novastar', model: 'A8s-N', series: 'Armor', maxPixels: 512 * 384,
    loadText: '512×384', source: 'reseller' },
  { id: 'ns-a8spro', brand: 'novastar', model: 'A8s Pro', series: 'Armor', maxPixels: 512 * 512,
    loadText: '512×512', source: 'reseller' },
  { id: 'ns-a10spro', brand: 'novastar', model: 'A10s Pro', series: 'Armor', maxPixels: 512 * 512,
    loadText: '512×512', source: 'reseller' },
];
