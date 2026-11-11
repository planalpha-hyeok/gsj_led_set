import { nextFreePort } from '../../core/dataMapping';
import { PHASES, suggestPhase } from '../../core/threePhase';
import { newDataRun, newPowerRun } from '../../store/defaults';
import { useProject } from '../../store/ProjectContext';
import { mapRun } from '../../store/wiringOps';
import type { Phase } from '../../types';
import { phaseColor, phaseName, portColor } from '../diagram/palette';
import { Icon } from '../ui/Icon';
import type { WiringKind } from './useWiringEditor';

interface Props {
  kind: WiringKind;
  activeId: string | undefined;
  setActiveId: (id: string) => void;
}

/** 배선 편집 도구 막대: 포트/회로 목록, 선택, 추가·삭제, 포트 번호·상 변경 */
export function WiringBar({ kind, activeId, setActiveId }: Props) {
  const { project: p, analysis: a, updateDataRuns, updatePowerRuns } = useProject();
  const three = p.power.system === 'three';
  const ctrl = a.controller;

  const items = kind === 'data'
    ? (a.data?.ports ?? []).map((pr) => ({ id: pr.id, label: pr.label, color: portColor(pr.index), count: pr.cells.length, load: pr.load }))
    : a.circuits.circuits.map((c) => ({
      id: c.id, label: three ? `${c.label} ${c.phase}` : c.label,
      color: three ? phaseColor[c.phase] : portColor(c.index), count: c.cells.length, load: c.load,
    }));

  const add = () => {
    if (kind === 'data') {
      const { unit, port } = nextFreePort(p, ctrl?.ports ?? 1);
      const run = newDataRun(unit, port);
      updateDataRuns((r) => [...r, run]);
      setActiveId(run.id);
    } else {
      const run = newPowerRun(suggestPhase(a.phases.phases));
      updatePowerRuns((r) => [...r, run]);
      setActiveId(run.id);
    }
  };

  const clear = () => {
    if (!activeId) return;
    if (kind === 'data') updateDataRuns((r) => mapRun(r, activeId, () => []));
    else updatePowerRuns((r) => mapRun(r, activeId, () => []));
  };

  const remove = () => {
    if (!activeId) return;
    const idx = items.findIndex((x) => x.id === activeId);
    if (kind === 'data') updateDataRuns((r) => r.filter((x) => x.id !== activeId));
    else updatePowerRuns((r) => r.filter((x) => x.id !== activeId));
    const next = items[idx + 1] ?? items[idx - 1];
    if (next) setActiveId(next.id);
  };

  const activeData = kind === 'data' ? p.dataRuns.find((r) => r.id === activeId) : undefined;
  const activePower = kind === 'power' ? p.powerRuns.find((r) => r.id === activeId) : undefined;
  const maxUnit = Math.max(0, ...p.dataRuns.map((r) => r.unit));

  return (
    <div className="wiring-bar">
      <div className="wiring-chips">
        {items.map((it) => (
          <button key={it.id} type="button" className={`chip ${it.id === activeId ? 'on' : ''} ${it.load > 1 ? 'over' : ''}`}
            onClick={() => setActiveId(it.id)} title={`${it.count}개 · ${(it.load * 100).toFixed(0)}%`}>
            <span className="chip-dot" style={{ background: it.color }} />
            <span className="chip-label">{it.label}</span>
            <span className="chip-meta mono">{it.count} · {(it.load * 100).toFixed(0)}%</span>
          </button>
        ))}
        <button type="button" className="chip add" onClick={add}>
          <Icon name="plus" size={13} />{kind === 'data' ? '포트 추가' : '회로 추가'}
        </button>
      </div>

      {(activeData || activePower) && (
        <div className="wiring-active">
          {activeData && (
            <>
              <label className="mini-field">컨트롤러
                <select value={activeData.unit} onChange={(e) => updateDataRuns((r) => r.map((x) => x.id === activeData.id ? { ...x, unit: Number(e.target.value) } : x))}>
                  {Array.from({ length: maxUnit + 2 }, (_, u) => <option key={u} value={u}>#{u + 1}</option>)}
                </select>
              </label>
              <label className="mini-field">포트
                <select value={activeData.port} onChange={(e) => updateDataRuns((r) => r.map((x) => x.id === activeData.id ? { ...x, port: Number(e.target.value) } : x))}>
                  {Array.from({ length: Math.max(ctrl?.ports ?? 1, activeData.port) }, (_, i) => <option key={i} value={i + 1}>{i + 1}</option>)}
                </select>
              </label>
            </>
          )}
          {activePower && three && (
            <label className="mini-field">상
              <select value={activePower.phase} onChange={(e) => updatePowerRuns((r) => r.map((x) => x.id === activePower.id ? { ...x, phase: e.target.value as Phase } : x))}>
                {PHASES.map((ph) => <option key={ph} value={ph}>{phaseName[ph]}</option>)}
              </select>
            </label>
          )}
          <button type="button" className="btn ghost sm" onClick={clear}>비우기</button>
          <button type="button" className="btn ghost sm danger" onClick={remove}><Icon name="trash" size={14} />삭제</button>
        </div>
      )}

      <div className="wiring-help">
        캐비닛 클릭·드래그: 연결 · 마지막 캐비닛 다시 클릭: 취소 · 다른 색 캐비닛 클릭: 선택 · 빈 곳 드래그/휠: 이동 · Ctrl+휠: 확대 · Ctrl+Z: 되돌리기
      </div>
    </div>
  );
}
