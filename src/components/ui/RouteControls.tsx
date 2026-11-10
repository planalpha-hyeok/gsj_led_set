import type { Corner, RouteSettings } from '../../types';
import { Segmented } from './Field';

interface Props {
  value: RouteSettings;
  onChange: (patch: Partial<RouteSettings>) => void;
  splitLabel: string;
}

const CORNERS: Corner[] = ['TL', 'TR', 'BL', 'BR'];
const cornerTitle: Record<Corner, string> = { TL: '좌상단', TR: '우상단', BL: '좌하단', BR: '우하단' };

/** 연결 경로 설정 (시작 모서리, 방향, 패턴, 분할) — 데이터·전원 공용 */
export function RouteControls({ value, onChange, splitLabel }: Props) {
  return (
    <div className="route-controls">
      <div className="field">
        <span className="field-label">시작 위치</span>
        <div className="corner-picker" role="radiogroup" aria-label="시작 위치">
          {CORNERS.map((c) => (
            <button key={c} type="button" role="radio" aria-checked={value.start === c} title={cornerTitle[c]}
              className={`corner corner-${c} ${value.start === c ? 'on' : ''}`} onClick={() => onChange({ start: c })}>
              <span className="corner-dot" />
            </button>
          ))}
          <span className="corner-label">{cornerTitle[value.start]}</span>
        </div>
      </div>
      <Segmented label="방향" value={value.direction} onChange={(v) => onChange({ direction: v })}
        options={[{ value: 'horizontal', label: '가로 (행)' }, { value: 'vertical', label: '세로 (열)' }]} />
      <Segmented label="패턴" value={value.pattern} onChange={(v) => onChange({ pattern: v })}
        options={[
          { value: 'serpentine', label: 'S자 왕복', title: '줄이 바뀔 때 방향을 반대로 (일반적)' },
          { value: 'zigzag', label: 'Z자 복귀', title: '매 줄 같은 쪽에서 시작' },
        ]} />
      <Segmented label={splitLabel} value={value.split} onChange={(v) => onChange({ split: v })}
        options={[
          { value: 'line', label: '줄 단위', title: '행/열을 나누지 않고 통째로 배정' },
          { value: 'fill', label: '최대 채움', title: '용량이 찰 때까지 이어서 배정' },
        ]} />
    </div>
  );
}
