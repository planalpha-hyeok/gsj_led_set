import { buildReport, circuitHeader, portHeader, type ReportRow } from '../../core/report';
import { useProject } from '../../store/ProjectContext';

function KvCard({ title, rows }: { title: string; rows: ReportRow[] }) {
  return (
    <div className="report-card">
      <h3>{title}</h3>
      <dl className="kv">
        {rows.map((r, i) => (
          <div key={i} className="kv-row">
            <dt>{r.label}</dt>
            <dd><span className="mono">{r.value}</span>{r.note && <small>{r.note}</small>}</dd>
          </div>
        ))}
      </dl>
    </div>
  );
}

function Table({ title, header, rows }: { title: string; header: string[]; rows: string[][] }) {
  return (
    <div className="report-card">
      <h3>{title} <span className="count">{rows.length}</span></h3>
      <div className="table-wrap">
        <table className="data-table">
          <thead><tr>{header.map((h) => <th key={h}>{h}</th>)}</tr></thead>
          <tbody>
            {rows.map((r, i) => (
              <tr key={i}>{r.map((v, j) => <td key={j} className={j === 0 ? '' : 'mono'}>{v}</td>)}</tr>
            ))}
          </tbody>
        </table>
      </div>
    </div>
  );
}

/** 요약 보고서 탭 (HTML 표) */
export function ReportView() {
  const { project, analysis } = useProject();
  const r = buildReport(project, analysis);
  return (
    <div className="report">
      <div className="report-grid-3">
        <KvCard title="화면 / 장비" rows={r.summary} />
        <KvCard title="전력" rows={r.power} />
        <KvCard title="자재 수량" rows={r.bom} />
      </div>
      <div className="report-grid-2">
        <Table title="데이터 포트" header={portHeader} rows={r.ports} />
        <Table title="전원 회로" header={circuitHeader} rows={r.circuits} />
      </div>
      <p className="hint">본 계산은 참고용입니다. 장비 사양은 제조사 사양서, 전기 공사는 전기 관련 법규와 자격자 확인을 따르십시오.</p>
    </div>
  );
}
