import { sourceLabel } from '../../data';
import type { SpecSource } from '../../types';

/** 사양 출처 표시 배지 */
export function SourceBadge({ source }: { source: SpecSource }) {
  return <span className={`badge badge-${source}`}>{sourceLabel[source]}</span>;
}

/** 사용률 막대 (0~1, 1 초과는 빨간색) */
export function LoadBar({ value }: { value: number }) {
  const pct = Math.min(100, value * 100);
  const cls = value > 1 ? 'err' : value > 0.9 ? 'warn' : 'ok';
  return (
    <span className="loadbar">
      <span className={`loadbar-fill ${cls}`} style={{ width: `${pct}%` }} />
    </span>
  );
}

/** 진행률 막대 (0~1, 완료 시 강조색) */
export function ProgressBar({ value }: { value: number }) {
  const pct = Math.max(0, Math.min(100, value * 100));
  return (
    <span className="loadbar">
      <span className={`loadbar-fill ${pct >= 100 ? 'ok' : 'progress'}`} style={{ width: `${pct}%` }} />
    </span>
  );
}
