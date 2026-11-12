import { useProject } from '../../store/ProjectContext';
import { LoadBar, ProgressBar } from '../ui/Badge';
import { Icon } from '../ui/Icon';
import { phaseColorFor, phaseName } from '../diagram/palette';
import { useTheme } from '../../store/theme';
import type { Severity } from '../../types';

const sevIcon: Record<Severity, 'alert' | 'info'> = { error: 'alert', warning: 'alert', info: 'info' };
const sevOrder: Record<Severity, number> = { error: 0, warning: 1, info: 2 };

function Stat({ label, value, unit, sub, variant }: { label: string; value: string; unit?: string; sub?: string; variant?: string }) {
  return (
    <div className={`stat ${variant ?? ''}`}>
      <span className="stat-label">{label}</span>
      <span className="stat-value mono">{value}{unit && <small>{unit}</small>}</span>
      {sub && <span className="stat-sub mono">{sub}</span>}
    </div>
  );
}

/** 오른쪽 패널: 핵심 수치, 부하, 경고 */
export function InfoPanel({ open }: { open: boolean }) {
  const { project: p, analysis: a } = useProject();
  const { theme } = useTheme();
  const s = a.screen;
  const ports = a.data?.ports ?? [];
  const maxPortLoad = ports.reduce((m, x) => Math.max(m, x.load), 0);
  const maxCircuitLoad = a.circuits.circuits.reduce((m, x) => Math.max(m, x.load), 0);
  const three = p.power.system === 'three';
  const issues = [...a.issues].sort((x, y) => sevOrder[x.severity] - sevOrder[y.severity]);
  const errCount = issues.filter((i) => i.severity === 'error').length;
  const warnCount = issues.filter((i) => i.severity === 'warning').length;

  return (
    <aside className={`info ${open ? 'open' : ''}`} aria-hidden={!open}>
      <div className={`status-pill ${errCount ? 'err' : warnCount ? 'warn' : 'ok'}`}>
        <Icon name={errCount || warnCount ? 'alert' : 'check'} size={15} />
        {errCount ? `오류 ${errCount}건` : warnCount ? `주의 ${warnCount}건` : '문제 없음'}
      </div>

      <div className="stat-grid">
        <Stat variant="wide accent" label="해상도" value={`${s.widthPx}×${s.heightPx}`} sub={`${(s.totalPixels / 1e6).toFixed(2)} MP · ${s.aspect}`} />
        <Stat label="크기" value={`${(s.widthMm / 1000).toFixed(2)}×${(s.heightMm / 1000).toFixed(2)}`} unit="m" sub={`${s.areaM2.toFixed(2)} m²`} />
        <Stat label="캐비닛" value={String(s.cabinetCount)} unit="개" sub={`${Math.round(s.weightKg)} kg`} />
        <Stat label="수신카드" value={String(a.cardResult?.totalCards ?? 0)} unit="장" />
        <Stat label="컨트롤러" value={String(a.data?.units.length ?? 0)} unit="대" sub={a.controller?.model} />
        <Stat label="사용 포트" value={String(ports.length)} unit="개" sub={a.data ? `포트당 최대 ${a.data.cabinetsPerPort}개` : undefined} />
        <Stat label={three ? '선전류(평형)' : '총 전류'} value={(three ? a.power.threePhaseLineA : a.power.singlePhaseA).toFixed(1)} unit="A"
          sub={`회로 ${a.circuits.circuits.length}개`} />
        <Stat variant="wide" label="최대 전력" value={(a.power.totalW / 1000).toFixed(2)} unit="kW" sub={`캐비닛당 ${a.power.cabinetW.toFixed(0)} W`} />
      </div>

      <div className="progress-block">
        <div className="load-row"><span>데이터 배선</span><span className="mono">{s.cabinetCount - (a.data?.unassigned ?? s.cabinetCount)} / {s.cabinetCount}</span></div>
        <ProgressBar value={(s.cabinetCount - (a.data?.unassigned ?? s.cabinetCount)) / Math.max(1, s.cabinetCount)} />
        <div className="load-row"><span>전원 배선</span><span className="mono">{s.cabinetCount - a.circuits.unassigned} / {s.cabinetCount}</span></div>
        <ProgressBar value={(s.cabinetCount - a.circuits.unassigned) / Math.max(1, s.cabinetCount)} />
      </div>

      <div className="load-block">
        <div className="load-row"><span>최대 포트 사용률</span><span className="mono">{(maxPortLoad * 100).toFixed(0)}%</span></div>
        <LoadBar value={maxPortLoad} />
        <div className="load-row"><span>최대 회로 사용률</span><span className="mono">{(maxCircuitLoad * 100).toFixed(0)}%</span></div>
        <LoadBar value={maxCircuitLoad} />
      </div>

      {three && (
        <div className="phase-block">
          <div className="block-title">3상 부하 <span className="mono">불평형 {a.phases.imbalancePct.toFixed(1)}%</span></div>
          {a.phases.phases.map((ph) => {
            const max = Math.max(...a.phases.phases.map((x) => x.currentA), 1);
            return (
              <div key={ph.phase} className="phase-row">
                <span className="phase-name" style={{ color: phaseColorFor(ph.phase, theme) }}>{phaseName[ph.phase]}</span>
                <span className="phase-bar"><span style={{ width: `${(ph.currentA / max) * 100}%`, background: phaseColorFor(ph.phase, theme) }} /></span>
                <span className="mono">{ph.currentA.toFixed(1)}A</span>
              </div>
            );
          })}
          {a.mainBreakerA && <div className="hint">메인 차단기 참고: 상당 {a.mainBreakerA}A 이상</div>}
        </div>
      )}
      {!three && a.mainBreakerA && <div className="hint pad">메인 차단기 참고: {a.mainBreakerA}A 이상</div>}

      <div className="issues">
        <div className="block-title">확인 사항</div>
        {issues.length === 0 && <div className="issue ok"><Icon name="check" size={14} />경고 없음</div>}
        {issues.map((i, k) => (
          <div key={k} className={`issue ${i.severity}`}>
            <Icon name={sevIcon[i.severity]} size={14} />
            <span>{i.message}</span>
          </div>
        ))}
      </div>
    </aside>
  );
}
