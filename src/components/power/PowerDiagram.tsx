import type { Analysis } from '../../core/analyze';
import type { Cell, Project } from '../../types';
import { AssignedGrid } from '../diagram/AssignedGrid';
import { CursorMark } from '../diagram/CursorMark';
import { DiagramBase } from '../diagram/DiagramBase';
import { computeGeometry } from '../diagram/geometry';
import { Legend, legendHeight, type LegendItem } from '../diagram/Legend';
import { phaseColorFor, phaseName, portColor, type DiagramTheme } from '../diagram/palette';
import { RouteLine } from '../diagram/RouteLine';

interface Props {
  project: Project;
  analysis: Analysis;
  theme: DiagramTheme;
  /** 편집 중인 회로 순서 (화면 편집 시에만) */
  activeIndex?: number;
  /** 키보드 커서 (화면 편집 시에만) */
  cursor?: Cell;
  drawing?: boolean;
}

/** 전원 배선도: 3상이면 상별 색, 단상이면 회로별 색 */
export function PowerDiagram({ project: p, analysis: a, theme, activeIndex, cursor, drawing }: Props) {
  const three = p.power.system === 'three';
  const circuits = a.circuits.circuits;
  const g0 = computeGeometry(p);
  const g = computeGeometry(p, legendHeight(circuits.length, g0.width));

  const colorOf = (i: number) => (three ? phaseColorFor(circuits[i].phase, theme) : portColor(i, theme));
  const nameOf = (i: number) => `${circuits[i].label}${three ? ` ${circuits[i].phase}` : ''}`;

  const items: LegendItem[] = circuits.map((ci) => ({
    color: colorOf(ci.index),
    label: nameOf(ci.index),
    detail: `${ci.cells.length}개 · ${Math.round(ci.watts).toLocaleString()}W · ${ci.currentA.toFixed(1)}A · ${(ci.load * 100).toFixed(0)}%`,
    danger: ci.load > 1,
  }));

  const unassigned = a.circuits.unassigned;
  const subtitle = (three
    ? `3상 ${p.power.lineVoltage}V · ` + a.phases.phases.map((x) => `${phaseName[x.phase]} ${x.currentA.toFixed(1)}A`).join(' · ')
    : `단상 ${p.power.phaseVoltage}V · 회로 ${circuits.length}개 · 회로당 최대 ${a.circuits.cabinetsPerCircuit}개 (${a.circuits.circuitLimitA.toFixed(1)}A)`)
    + (unassigned ? ` · 미연결 ${unassigned}개` : '');

  return (
    <DiagramBase project={p} geom={g} theme={theme} title="전원 배선" subtitle={subtitle}>
      <AssignedGrid project={p} geom={g} theme={theme} groups={circuits.map((ci) => ci.cells)}
        colorOf={colorOf} nameOf={nameOf} activeGroup={activeIndex} />
      {circuits.map((ci) => (
        <RouteLine key={ci.id} cells={ci.cells} geom={g} color={colorOf(ci.index)}
          startLabel={ci.label} labelColor={theme === 'light' ? '#ffffff' : '#0b0e12'}
          dim={activeIndex !== undefined && activeIndex !== ci.index} />
      ))}
      {cursor && <CursorMark cell={cursor} geom={g} drawing={!!drawing} theme={theme} />}
      <Legend items={items} geom={g} theme={theme} />
    </DiagramBase>
  );
}
