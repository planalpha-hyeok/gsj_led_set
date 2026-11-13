import { cabinetFromModules, cabinetAreaM2 } from '../../core/cabinetBuild';
import { findModule, layoutPresets, modules } from '../../data/modules';
import { useProject } from '../../store/ProjectContext';
import { NumberField, Segmented, SelectField, TextField } from '../ui/Field';
import { Section } from '../ui/Section';

const CUSTOM = 'custom';

/**
 * 캐비닛 사양
 * - 모듈 종류 + 배치(가로 × 세로 장수) → 크기·해상도 자동 계산
 * - "직접 입력"을 고르면 크기·해상도를 직접 입력
 * - 전력: SMPS 기준 또는 ㎡당 W 기준
 */
export function CabinetSettings() {
  const { project: p, updateCabinet } = useProject();
  const cab = p.cabinet;
  const mod = findModule(cab.moduleId);
  const cols = cab.moduleCols ?? 1;
  const rows = cab.moduleRows ?? 1;

  const applyModule = (id: string, c: number, r: number) => {
    const m = findModule(id);
    if (m) updateCabinet(cabinetFromModules(cab, m, c, r));
  };

  const onModule = (id: string) => {
    if (id === CUSTOM) {
      updateCabinet({ moduleId: undefined, moduleCols: undefined, moduleRows: undefined });
      return;
    }
    const m = findModule(id)!;
    // 모듈 크기가 바뀌면 그 크기의 첫 번째 자주 쓰는 배치로
    const sameSize = mod && mod.widthMm === m.widthMm && mod.heightMm === m.heightMm;
    const preset = layoutPresets.find((x) => x.moduleW === m.widthMm && x.moduleH === m.heightMm);
    applyModule(id, sameSize ? cols : preset?.cols ?? 1, sameSize ? rows : preset?.rows ?? 1);
  };

  const presets = mod ? layoutPresets.filter((x) => x.moduleW === mod.widthMm && x.moduleH === mod.heightMm) : [];
  const groups = [...new Set(modules.map((m) => `${m.widthMm}×${m.heightMm}`))];

  const switchPowerMode = (mode: 'smps' | 'area') => {
    if (mode === cab.powerMode) return;
    // ㎡당 W로 바꿀 때 값이 없으면 지금 SMPS 기준 전력을 ㎡당 값으로 옮겨서 시작
    if (mode === 'area' && cab.powerPerM2 <= 0) {
      const area = cabinetAreaM2(cab);
      const w = (cab.smpsW * cab.smpsCount) / (cab.smpsEfficiency || 1);
      updateCabinet({ powerMode: mode, powerPerM2: area > 0 ? Math.round(w / area) : 0 });
    } else {
      updateCabinet({ powerMode: mode });
    }
  };

  return (
    <Section title="캐비닛 사양" icon="screen">
      <SelectField label="모듈" value={mod ? mod.id : CUSTOM} onChange={onModule}>
        {groups.map((g) => (
          <optgroup key={g} label={`${g} 모듈`}>
            {modules.filter((m) => `${m.widthMm}×${m.heightMm}` === g).map((m) => (
              <option key={m.id} value={m.id}>{m.pitch} · {g} ({m.widthPx}×{m.heightPx}px)</option>
            ))}
          </optgroup>
        ))}
        <option value={CUSTOM}>직접 입력 (크기·해상도)</option>
      </SelectField>

      {mod ? (
        <>
          {presets.length > 0 && (
            <div className="field">
              <span className="field-label">자주 쓰는 캐비닛</span>
              <div className="preset-chips">
                {presets.map((x) => {
                  const on = x.cols === cols && x.rows === rows;
                  return (
                    <button key={`${x.cols}x${x.rows}`} type="button" className={`chip ${on ? 'on' : ''}`}
                      onClick={() => applyModule(mod.id, x.cols, x.rows)}>
                      {x.moduleW * x.cols}×{x.moduleH * x.rows}
                      <span className="chip-meta mono">{x.cols * x.rows}장</span>
                    </button>
                  );
                })}
              </div>
            </div>
          )}
          <div className="grid-2">
            <NumberField label="모듈 가로 장수" value={cols} min={1} max={20} integer unit="장" onChange={(v) => applyModule(mod.id, v, rows)} />
            <NumberField label="모듈 세로 장수" value={rows} min={1} max={20} integer unit="장" onChange={(v) => applyModule(mod.id, cols, v)} />
          </div>
          <div className="cab-summary mono">
            <span>{cab.widthMm}×{cab.heightMm} mm</span>
            <span>{cab.widthPx}×{cab.heightPx} px</span>
            <span>모듈 {cols * rows}장</span>
          </div>
        </>
      ) : (
        <>
          <TextField label="이름" value={cab.name} onChange={(v) => updateCabinet({ name: v })} />
          <div className="grid-2">
            <NumberField label="가로 픽셀" value={cab.widthPx} min={1} integer unit="px" onChange={(v) => updateCabinet({ widthPx: v })} />
            <NumberField label="세로 픽셀" value={cab.heightPx} min={1} integer unit="px" onChange={(v) => updateCabinet({ heightPx: v })} />
            <NumberField label="가로 크기" value={cab.widthMm} min={1} unit="mm" onChange={(v) => updateCabinet({ widthMm: v })} />
            <NumberField label="세로 크기" value={cab.heightMm} min={1} unit="mm" onChange={(v) => updateCabinet({ heightMm: v })} />
          </div>
        </>
      )}

      <NumberField label="캐비닛 무게" value={cab.weightKg} min={0} step={0.1} unit="kg" onChange={(v) => updateCabinet({ weightKg: v })} />

      <Segmented label="전력 기준" value={cab.powerMode} onChange={switchPowerMode}
        options={[{ value: 'smps', label: 'SMPS' }, { value: 'area', label: '㎡당 W (사양서)' }]} />

      {cab.powerMode === 'smps' ? (
        <>
          <Segmented label="SMPS 용량 (다른 값은 아래 정격에 직접 입력)" value={cab.smpsW} onChange={(v) => updateCabinet({ smpsW: v })}
            options={[{ value: 100, label: '100W' }, { value: 200, label: '200W' }]} />
          <div className="grid-2">
            <NumberField label="SMPS 정격" value={cab.smpsW} min={1} unit="W" onChange={(v) => updateCabinet({ smpsW: v })} />
            <NumberField label="캐비닛당 개수" value={cab.smpsCount} min={1} max={20} integer unit="개" onChange={(v) => updateCabinet({ smpsCount: v })} />
            <NumberField label="SMPS 효율" value={Math.round(cab.smpsEfficiency * 100)} min={50} max={100} integer unit="%"
              onChange={(v) => updateCabinet({ smpsEfficiency: v / 100 })} />
            <Readout label="캐비닛 입력 전력" value={`${((cab.smpsW * cab.smpsCount) / (cab.smpsEfficiency || 1)).toFixed(1)} W`} />
          </div>
          <p className="hint">입력 전력 = SMPS 정격 × 개수 ÷ 효율. 효율은 SMPS 사양서 값으로 입력하세요 (기본 85%는 임시값).</p>
        </>
      ) : (
        <>
          <div className="grid-2">
            <NumberField label="최대 소비전력" value={cab.powerPerM2} min={0} unit="W/㎡" onChange={(v) => updateCabinet({ powerPerM2: v })} />
            <Readout label="캐비닛 입력 전력" value={`${(cab.powerPerM2 * cabinetAreaM2(cab)).toFixed(1)} W`} />
          </div>
          <p className="hint">캐비닛 전력 = 사양서의 ㎡당 최대 소비전력 × 캐비닛 면적 ({cabinetAreaM2(cab).toFixed(4)}㎡). 사양서 값이 이미 입력 전력이므로 효율은 적용하지 않습니다.</p>
        </>
      )}
    </Section>
  );
}

function Readout({ label, value }: { label: string; value: string }) {
  return (
    <div className="field">
      <span className="field-label">{label}</span>
      <span className="field-readout mono">{value}</span>
    </div>
  );
}
