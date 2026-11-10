import { cabinetTemplates } from '../../data/cabinets';
import { useProject } from '../../store/ProjectContext';
import { NumberField, SelectField, TextField } from '../ui/Field';
import { Section } from '../ui/Section';

/** 화면 구성 설정: 캐비닛 규격, 가로×세로 개수 */
export function ScreenSettings() {
  const { project: p, update, updateCabinet } = useProject();
  const cab = p.cabinet;

  const applyTemplate = (id: string) => {
    const tpl = cabinetTemplates.find((t) => t.id === id);
    if (tpl) updateCabinet({ ...tpl });
  };

  return (
    <>
      <Section title="화면 구성" icon="grid">
        <div className="grid-2">
          <NumberField label="가로 개수" value={p.cols} min={1} max={200} integer onChange={(v) => update({ cols: v })} />
          <NumberField label="세로 개수" value={p.rows} min={1} max={200} integer onChange={(v) => update({ rows: v })} />
        </div>
      </Section>

      <Section title="캐비닛 사양" icon="screen">
        <SelectField label="템플릿 불러오기" value="" onChange={applyTemplate}>
          <option value="" disabled>템플릿 선택…</option>
          {cabinetTemplates.map((t) => <option key={t.id} value={t.id}>{t.name}</option>)}
        </SelectField>
        <TextField label="이름" value={cab.name} onChange={(v) => updateCabinet({ name: v })} />
        <div className="grid-2">
          <NumberField label="가로 픽셀" value={cab.widthPx} min={1} integer unit="px" onChange={(v) => updateCabinet({ widthPx: v })} />
          <NumberField label="세로 픽셀" value={cab.heightPx} min={1} integer unit="px" onChange={(v) => updateCabinet({ heightPx: v })} />
          <NumberField label="가로 크기" value={cab.widthMm} min={1} unit="mm" onChange={(v) => updateCabinet({ widthMm: v })} />
          <NumberField label="세로 크기" value={cab.heightMm} min={1} unit="mm" onChange={(v) => updateCabinet({ heightMm: v })} />
          <NumberField label="최대 전력" value={cab.powerMaxW} min={0} unit="W" onChange={(v) => updateCabinet({ powerMaxW: v })} />
          <NumberField label="평균 전력" value={cab.powerAvgW} min={0} unit="W" onChange={(v) => updateCabinet({ powerAvgW: v })} />
          <NumberField label="무게" value={cab.weightKg} min={0} step={0.1} unit="kg" onChange={(v) => updateCabinet({ weightKg: v })} />
        </div>
        <p className="hint">템플릿 값은 예시입니다. 전력·무게는 실제 캐비닛 사양서 값으로 수정하세요.</p>
      </Section>
    </>
  );
}
