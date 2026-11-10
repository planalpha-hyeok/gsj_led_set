import type { BitDepth, ControllerSpec } from '../types';

/**
 * 포트 1개의 실제 사용 가능 픽셀 수
 * - 기준: 8bit / 60Hz 사양 값
 * - 10/12bit: 기준 × 0.5 (MCTRL4K 공식 값 65만 → 32만 기준, 보수적 적용)
 * - 주사율: 기준 × 60 / 주사율 (대역폭 비례 근사치)
 */
export function effectivePortCapacity(ctrl: ControllerSpec, bitDepth: BitDepth, frameRate: number): number {
  const base = ctrl.pixelsPerPort ?? Math.floor(ctrl.maxPixels / ctrl.ports);
  const bitFactor = bitDepth === 8 ? 1 : 0.5;
  const fps = frameRate > 0 ? frameRate : 60;
  return Math.floor(base * bitFactor * (60 / fps));
}

/** 장비 1대 실제 사용 가능 픽셀 수 (포트 합계와 장비 총용량 중 작은 값) */
export function effectiveUnitCapacity(ctrl: ControllerSpec, bitDepth: BitDepth, frameRate: number): number {
  const fps = frameRate > 0 ? frameRate : 60;
  const bitFactor = bitDepth === 8 ? 1 : 0.5;
  const unitMax = Math.floor(ctrl.maxPixels * bitFactor * (60 / fps));
  const portSum = effectivePortCapacity(ctrl, bitDepth, frameRate) * ctrl.ports;
  return Math.min(unitMax, portSum);
}
