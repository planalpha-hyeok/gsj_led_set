import { useProject } from '../../store/ProjectContext';
import { NumberField } from '../ui/Field';
import { Section } from '../ui/Section';
import { CabinetSettings } from './CabinetSettings';
import { TrackOverrides } from './TrackOverrides';

/** 화면 구성 설정: 캐비닛 규격, 가로×세로 개수 */
export function ScreenSettings() {
  const { project: p, update } = useProject();

  return (
    <>
      <Section title="화면 구성" icon="grid">
        <div className="grid-2">
          <NumberField label="가로 개수" value={p.cols} min={1} max={200} integer onChange={(v) => update({ cols: v })} />
          <NumberField label="세로 개수" value={p.rows} min={1} max={200} integer onChange={(v) => update({ rows: v })} />
        </div>
        <TrackOverrides />
      </Section>

      <CabinetSettings />
    </>
  );
}
