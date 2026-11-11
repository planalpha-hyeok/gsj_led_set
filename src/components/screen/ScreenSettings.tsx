import { cabinetTemplates } from '../../data/cabinets';
import { useProject } from '../../store/ProjectContext';
import { NumberField, Segmented, SelectField, TextField } from '../ui/Field';
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
          <NumberField label="무게" value={cab.weightKg} min={0} step={0.1} unit="kg" onChange={(v) => updateCabinet({ weightKg: v })} />
        </div>
        <Segmented label="SMPS 용량 (다른 값은 아래 정격에 직접 입력)" value={cab.smpsW} onChange={(v) => updateCabinet({ smpsW: v })}
          options={[{ value: 100, label: '100W' }, { value: 200, label: '200W' }]} />
        <div className="grid-2">
          <NumberField label="SMPS 정격" value={cab.smpsW} min={1} unit="W" onChange={(v) => updateCabinet({ smpsW: v })} />
          <NumberField label="캐비닛당 개수" value={cab.smpsCount} min={1} max={20} integer unit="개" onChange={(v) => updateCabinet({ smpsCount: v })} />
          <NumberField label="SMPS 효율" value={Math.round(cab.smpsEfficiency * 100)} min={50} max={100} integer unit="%"
            onChange={(v) => updateCabinet({ smpsEfficiency: v / 100 })} />
          <div className="field">
            <span className="field-label">캐비닛 입력 전력</span>
            <span className="field-readout mono">{(cab.smpsW * cab.smpsCount / (cab.smpsEfficiency || 1)).toFixed(1)} W</span>
          </div>
        </div>
        <p className="hint">입력 전력 = SMPS 정격 × 개수 ÷ 효율. 효율은 SMPS 사양서 값으로 입력하세요 (기본 85%는 임시값). 템플릿의 무게도 예시입니다.</p>
      </Section>
    </>
  );
}
