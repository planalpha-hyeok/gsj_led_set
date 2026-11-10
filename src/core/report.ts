import type { Project } from '../types';
import type { Analysis } from './analyze';

export interface ReportRow {
  label: string;
  value: string;
  note?: string;
}

export interface Report {
  summary: ReportRow[];
  power: ReportRow[];
  bom: ReportRow[];
  ports: string[][];
  circuits: string[][];
}

const n = (v: number) => v.toLocaleString();
const sumJumpers = (groups: { cells: unknown[] }[]) => groups.reduce((s, g) => s + Math.max(0, g.cells.length - 1), 0);

/** 보고서용 표 데이터 (화면 표와 PDF가 같은 데이터를 사용) */
export function buildReport(p: Project, a: Analysis): Report {
  const s = a.screen;
  const pw = a.power;
  const three = p.power.system === 'three';
  const ports = a.data?.ports ?? [];
  const units = a.data?.units.length ?? 0;
  const circuits = a.phases.circuits;

  const summary: ReportRow[] = [
    { label: '캐비닛', value: `${p.cabinet.name} · ${p.cols} × ${p.rows} = ${n(s.cabinetCount)}개` },
    { label: '해상도', value: `${n(s.widthPx)} × ${n(s.heightPx)} px (${s.aspect})`, note: `${n(s.totalPixels)} px` },
    { label: '크기', value: `${(s.widthMm / 1000).toFixed(3)} × ${(s.heightMm / 1000).toFixed(3)} m`, note: `${s.areaM2.toFixed(2)} m²` },
    { label: '피치', value: `${s.pitchMm.toFixed(2)} mm` },
    { label: '무게', value: `${n(Math.round(s.weightKg))} kg`, note: '캐비닛만, 구조물 제외' },
    { label: '컨트롤러', value: a.controller ? `${a.controller.model} × ${units}대` : '-', note: a.data ? `포트당 ${n(a.data.portCapacity)} px` : undefined },
    { label: '수신카드', value: a.card ? `${a.card.model} × ${a.cardResult?.totalCards ?? 0}장` : '-' },
  ];

  const power: ReportRow[] = [
    { label: '최대 전력', value: `${(pw.totalMaxW / 1000).toFixed(2)} kW` },
    { label: '평균 전력', value: `${(pw.totalAvgW / 1000).toFixed(2)} kW` },
    { label: '계산 기준', value: p.power.basis === 'max' ? '최대 전력' : '평균 전력', note: `역률 ${p.power.powerFactor}` },
    { label: '피상전력', value: `${pw.totalKVA.toFixed(2)} kVA` },
    three
      ? { label: '상별 전류', value: a.phases.phases.map((x) => `${x.phase} ${x.currentA.toFixed(1)}A`).join(' / '), note: `불평형 ${a.phases.imbalancePct.toFixed(1)}%` }
      : { label: '총 전류', value: `${pw.singlePhaseA.toFixed(1)} A`, note: `${p.power.phaseVoltage}V 단상` },
    { label: '메인 차단기 (참고)', value: a.mainBreakerA ? `${a.mainBreakerA} A 이상` : '분전반 분할 필요', note: `사용률 ${Math.round(p.power.derate * 100)}% 기준` },
    { label: '캐비닛 1개 전류', value: `${pw.cabinetA.toFixed(2)} A` },
    { label: '회로 허용 전류', value: `${a.circuits.circuitLimitA.toFixed(1)} A`, note: `차단기 ${p.power.breakerA}A × ${Math.round(p.power.derate * 100)}%, 케이블 ${p.power.chainLimitA}A 중 작은 값` },
  ];

  const bom: ReportRow[] = [
    { label: '캐비닛', value: `${n(s.cabinetCount)} 개` },
    { label: '수신카드', value: `${n(a.cardResult?.totalCards ?? 0)} 장` },
    { label: '컨트롤러', value: `${units} 대` },
    { label: '데이터 케이블 (컨트롤러 → 캐비닛)', value: `${ports.length} 개` },
    { label: '데이터 점퍼 (캐비닛 간)', value: `${sumJumpers(ports)} 개` },
    { label: '전원 회로 (분기 차단기)', value: `${circuits.length} 개` },
    { label: '전원 인입 케이블', value: `${circuits.length} 개` },
    { label: '전원 점퍼 (캐비닛 간)', value: `${sumJumpers(circuits)} 개` },
  ];

  const multi = units > 1;
  const portRows = ports.map((pr) => [
    multi ? `U${pr.unit + 1}-P${pr.portNo}` : `P${pr.portNo}`,
    String(pr.cells.length),
    n(pr.pixels),
    `${(pr.load * 100).toFixed(1)}%`,
  ]);

  const circuitRows = circuits.map((c) => [
    `C${c.index + 1}`,
    three ? c.phase : '-',
    String(c.cells.length),
    n(c.watts),
    c.currentA.toFixed(2),
    `${(c.load * 100).toFixed(1)}%`,
  ]);

  return { summary, power, bom, ports: portRows, circuits: circuitRows };
}

export const portHeader = ['포트', '캐비닛', '픽셀', '사용률'];
export const circuitHeader = ['회로', '상', '캐비닛', '전력(W)', '전류(A)', '사용률'];
