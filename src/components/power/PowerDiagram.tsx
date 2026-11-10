import type { Analysis } from '../../core/analyze';
import type { Project } from '../../types';
import { AssignedGrid } from '../diagram/AssignedGrid';
import { DiagramBase } from '../diagram/DiagramBase';
import { computeGeometry } from '../diagram/geometry';
import { Legend, legendHeight, type LegendItem } from '../diagram/Legend';
import { phaseColor, phaseName, portColor, type DiagramTheme } from '../diagram/palette';
import { RouteLine } from '../diagram/RouteLine';

interface Props {
  project: Project;
  analysis: Analysis;
  theme: DiagramTheme;
}

/** 전원 배선도: 3상이면 상별 색, 단상이면 회로별 색 */
export function PowerDiagram({ project: p, analysis: a, theme }: Props) {
  const three = p.power.system === 'three';
  const circuits = a.phases.circuits;
  const g0 = computeGeometry(p);
  const g = computeGeometry(p, legendHeight(circuits.length, g0.width));

  const colorOf = (i: number) => (three ? phaseColor[circuits[i].phase] : portColor(i));
  const nameOf = (i: number) => `C${i + 1}${three ? ` ${circuits[i].phase}` : ''}`;

  const items: LegendItem[] = circuits.map((ci) => ({
    color: colorOf(ci.index),
    label: nameOf(ci.index),
    detail: `${ci.cells.length}개 · ${ci.watts.toLocaleString()}W · ${ci.currentA.toFixed(1)}A · ${(ci.load * 100).toFixed(0)}%`,
    danger: ci.load > 1,
  }));

  const subtitle = three
    ? `3상 ${p.power.lineVoltage}V · ` + a.phases.phases.map((x) => `${phaseName[x.phase]} ${x.currentA.toFixed(1)}A`).join(' · ')
    : `단상 ${p.power.phaseVoltage}V · 회로 ${circuits.length}개 · 회로당 최대 ${a.circuits.cabinetsPerCircuit}개 (${a.circuits.circuitLimitA.toFixed(1)}A)`;

  return (
    <DiagramBase project={p} geom={g} theme={theme} title="전원 배선" subtitle={subtitle}>
      <AssignedGrid project={p} geom={g} theme={theme} groups={circuits.map((ci) => ci.cells)}
        colorOf={colorOf} nameOf={nameOf} />
      {circuits.map((ci) => (
        <RouteLine key={ci.index} cells={ci.cells} geom={g} color={colorOf(ci.index)}
          startLabel={`C${ci.index + 1}`} labelColor="#0b0e12" />
      ))}
      <Legend items={items} geom={g} theme={theme} />
    </DiagramBase>
  );
}
