import { limitByLabel } from '../../core/powerCircuit';
import { useProject } from '../../store/ProjectContext';
import { NumberField, Segmented } from '../ui/Field';
import { Section } from '../ui/Section';

/** 전기 조건 설정 */
export function PowerSettings() {
  const { project: p, analysis, updatePower } = useProject();
  const c = analysis.circuits;
  const pw = p.power;
  return (
    <Section title="전기 조건" icon="bolt">
      <Segmented label="전원 방식" value={pw.system} onChange={(v) => updatePower({ system: v })}
        options={[{ value: 'single', label: '단상' }, { value: 'three', label: '3상 4선' }]} />
      <div className="grid-2">
        <NumberField label="상전압" value={pw.phaseVoltage} min={100} max={480} unit="V" onChange={(v) => updatePower({ phaseVoltage: v })} />
        {pw.system === 'three' && (
          <NumberField label="선간전압" value={pw.lineVoltage} min={100} max={690} unit="V" onChange={(v) => updatePower({ lineVoltage: v })} />
        )}
        <NumberField label="역률" value={pw.powerFactor} min={0.5} max={1} step={0.01} onChange={(v) => updatePower({ powerFactor: v })} />
        <NumberField label="분기 차단기" value={pw.breakerA} min={1} unit="A" onChange={(v) => updatePower({ breakerA: v })} />
        <NumberField label="차단기 사용률" value={Math.round(pw.derate * 100)} min={50} max={100} integer unit="%"
          onChange={(v) => updatePower({ derate: v / 100 })} />
        <NumberField label="케이블 허용 전류" value={pw.chainLimitA} min={1} unit="A" onChange={(v) => updatePower({ chainLimitA: v })} />
        <NumberField label="한 라인 최대 (0=미사용)" value={pw.lineLimitW} min={0} integer unit="W" onChange={(v) => updatePower({ lineLimitW: v })} />
      </div>
      <div className="limit-readout">
        <span>적용 한도 (한 라인)</span>
        <strong className="mono">{Math.round(c.circuitLimitW).toLocaleString()} W · {c.circuitLimitA.toFixed(1)} A</strong>
        <small>{limitByLabel[c.limitBy]} 기준 · 캐비닛 최대 {c.cabinetsPerCircuit}개</small>
      </div>
      <p className="hint">세 기준(차단기 × 사용률, 케이블 허용 전류, 한 라인 최대 W) 중 가장 엄격한 값을 적용합니다. W와 A는 상전압 × 역률로 환산합니다 (220V × {pw.powerFactor} 기준 16A ≈ {Math.round(16 * pw.phaseVoltage * pw.powerFactor).toLocaleString()}W).</p>
    </Section>
  );
}
