import { nextFreePort } from '../../core/dataMapping';
import { PHASES, suggestPhase } from '../../core/threePhase';
import { newDataRun, newPowerRun } from '../../store/defaults';
import { useProject } from '../../store/ProjectContext';
import { mapRun } from '../../store/wiringOps';
import type { Cell, Phase } from '../../types';
import { cellName } from '../diagram/geometry';
import { phaseColorFor, phaseName, portColor } from '../diagram/palette';
import { useTheme } from '../../store/theme';
import { Icon } from '../ui/Icon';
import type { WiringKind } from './useWiringEditor';

interface Props {
  kind: WiringKind;
  activeId: string | undefined;
  setActiveId: (id: string) => void;
  /** 키보드 배선 상태 */
  drawing: boolean;
  cursor: Cell;
}

/** 배선 편집 도구 막대: 포트/회로 목록, 선택, 추가·삭제, 포트 번호·상 변경 */
export function WiringBar({ kind, activeId, setActiveId, drawing, cursor }: Props) {
  const { project: p, analysis: a, updateDataRuns, updatePowerRuns } = useProject();
  const { theme } = useTheme();
  const three = p.power.system === 'three';
  const ctrl = a.controller;

  const items = kind === 'data'
    ? (a.data?.ports ?? []).map((pr) => ({ id: pr.id, label: pr.label, color: portColor(pr.index, theme), count: pr.cells.length, load: pr.load }))
    : a.circuits.circuits.map((c) => ({
      id: c.id, label: three ? `${c.label} ${c.phase}` : c.label,
      color: three ? phaseColorFor(c.phase, theme) : portColor(c.index, theme), count: c.cells.length, load: c.load,
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

      <div className="wiring-status">
        <span className={`kb-state ${drawing ? 'drawing' : ''}`}>
          {drawing ? `배선 중 · ${items.find((x) => x.id === activeId)?.label ?? ''}` : '대기'} · 커서 {cellName(cursor)}
        </span>
        <span className="wiring-help">
          {drawing
            ? <><kbd>방향키</kbd> 연결 · 되돌아가면 취소 <kbd>Shift</kbd>+<kbd>방향키</kbd> 끝까지 <kbd>Backspace</kbd> 하나 취소 <kbd>Space</kbd>/<kbd>Esc</kbd> 끝</>
            : <><kbd>방향키</kbd> 이동 <kbd>Space</kbd> 시작·이어그리기 / 배선 위: 여기부터 삭제 / 시작점: 전체 삭제 <kbd>Tab</kbd> {kind === 'data' ? '포트' : '회로'} 선택 <kbd>N</kbd> 새 {kind === 'data' ? '포트' : '회로'} <kbd>Ctrl</kbd>+<kbd>Z</kbd> 되돌리기 · 마우스 클릭·드래그도 가능</>}
        </span>
      </div>
    </div>
  );
}
