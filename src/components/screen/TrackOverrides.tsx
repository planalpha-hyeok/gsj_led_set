import { useState } from 'react';
import { findModule } from '../../data/modules';
import { useProject } from '../../store/ProjectContext';
import { colName } from '../diagram/geometry';
import { Icon } from '../ui/Icon';

type Kind = 'col' | 'row';

/**
 * 크기가 다른 열/행 지정 (모듈 장수)
 * 예) E열만 모듈 1장 폭(320mm), 3행만 모듈 2장 높이(320mm)
 * 열과 행을 둘 다 바꾸면 만나는 칸은 바뀐 폭 × 바뀐 높이 캐비닛이 된다
 */
export function TrackOverrides() {
  const { project: p, update } = useProject();
  const mod = findModule(p.cabinet.moduleId);
  if (!mod) {
    return <p className="hint">열·행마다 크기를 바꾸려면 캐비닛을 모듈 조합으로 선택하세요.</p>;
  }
  return (
    <div className="track-overrides">
      <TrackEditor kind="col" count={p.cols} map={p.colModules} unitMm={mod.widthMm} base={p.cabinet.moduleCols ?? 1}
        onChange={(m) => update({ colModules: m })} />
      <TrackEditor kind="row" count={p.rows} map={p.rowModules} unitMm={mod.heightMm} base={p.cabinet.moduleRows ?? 1}
        onChange={(m) => update({ rowModules: m })} />
    </div>
  );
}

interface EditorProps {
  kind: Kind;
  count: number;
  map: Record<string, number>;
  /** 모듈 1장 크기 (mm) */
  unitMm: number;
  /** 기본 캐비닛의 모듈 장수 */
  base: number;
  onChange: (m: Record<string, number>) => void;
}

function TrackEditor({ kind, count, map, unitMm, base, onChange }: EditorProps) {
  const name = (i: number) => (kind === 'col' ? `${colName(i)}열` : `${i + 1}행`);
  const entries = Object.entries(map)
    .map(([k, v]) => [Number(k), v] as const)
    .filter(([i, v]) => i < count && v !== base)
    .sort((a, b) => a[0] - b[0]);
  const [index, setIndex] = useState(count - 1);
  const [mods, setMods] = useState(Math.max(1, base - 1));
  const safeIndex = Math.min(index, count - 1);

  const add = () => {
    const next = { ...map };
    if (mods === base) delete next[String(safeIndex)];
    else next[String(safeIndex)] = mods;
    onChange(next);
  };
  const remove = (i: number) => {
    const next = { ...map };
    delete next[String(i)];
    onChange(next);
  };

  return (
    <div className="track-editor">
      <div className="field-label">{kind === 'col' ? '폭이 다른 열' : '높이가 다른 행'} <span className="muted">(기본 {base}장 = {base * unitMm}mm)</span></div>
      {entries.length > 0 && (
        <div className="preset-chips">
          {entries.map(([i, v]) => (
            <span key={i} className="chip on track-chip">
              {name(i)} · {v}장 <span className="chip-meta mono">{v * unitMm}mm</span>
              <button type="button" className="chip-x" onClick={() => remove(i)} aria-label={`${name(i)} 기본 크기로`}><Icon name="x" size={12} /></button>
            </span>
          ))}
        </div>
      )}
      <div className="track-add">
        <select value={safeIndex} onChange={(e) => setIndex(Number(e.target.value))} aria-label={kind === 'col' ? '열 선택' : '행 선택'}>
          {Array.from({ length: count }, (_, i) => <option key={i} value={i}>{name(i)}</option>)}
        </select>
        <select value={mods} onChange={(e) => setMods(Number(e.target.value))} aria-label="모듈 장수">
          {Array.from({ length: 8 }, (_, i) => i + 1).map((n) => <option key={n} value={n}>{n}장 ({n * unitMm}mm)</option>)}
        </select>
        <button type="button" className="btn sm" onClick={add}><Icon name="plus" size={13} />적용</button>
      </div>
    </div>
  );
}
