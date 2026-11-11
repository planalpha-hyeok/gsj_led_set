import type { Analysis } from '../../core/analyze';
import type { Project } from '../../types';
import { AssignedGrid } from '../diagram/AssignedGrid';
import { DiagramBase } from '../diagram/DiagramBase';
import { computeGeometry } from '../diagram/geometry';
import { Legend, legendHeight, type LegendItem } from '../diagram/Legend';
import { portColor, type DiagramTheme } from '../diagram/palette';
import { RouteLine } from '../diagram/RouteLine';

interface Props {
  project: Project;
  analysis: Analysis;
  theme: DiagramTheme;
  /** 편집 중인 포트 순서 (화면 편집 시에만) */
  activeIndex?: number;
}

/** 데이터 배선도 (화면 표시·내보내기 공용, 순수 SVG) */
export function MappingDiagram({ project: p, analysis: a, theme, activeIndex }: Props) {
  const ports = a.data?.ports ?? [];
  const g0 = computeGeometry(p);
  const g = computeGeometry(p, legendHeight(ports.length, g0.width));

  const items: LegendItem[] = ports.map((pr) => ({
    color: portColor(pr.index),
    label: pr.label,
    detail: `${pr.cells.length}개 · ${pr.pixels.toLocaleString()}px · ${(pr.load * 100).toFixed(0)}%`,
    danger: pr.load > 1,
  }));

  const ctrl = a.controller;
  const unassigned = a.data?.unassigned ?? 0;
  const subtitle = ctrl
    ? `${ctrl.model} × ${a.data?.units.length ?? 0}대 · 포트 ${ports.length}개 · 포트당 최대 캐비닛 ${a.data?.cabinetsPerPort ?? 0}개`
      + (unassigned ? ` · 미연결 ${unassigned}개` : '')
    : '컨트롤러 미선택';

  return (
    <DiagramBase project={p} geom={g} theme={theme} title="데이터 배선" subtitle={subtitle}>
      <AssignedGrid project={p} geom={g} theme={theme} groups={ports.map((pr) => pr.cells)}
        colorOf={portColor} nameOf={(i) => ports[i].label} activeGroup={activeIndex} />
      {ports.map((pr) => (
        <RouteLine key={pr.id} cells={pr.cells} geom={g} color={portColor(pr.index)}
          startLabel={pr.label.replace('U', '').replace('P', '')} labelColor="#0b0e12"
          dim={activeIndex !== undefined && activeIndex !== pr.index} />
      ))}
      <Legend items={items} geom={g} theme={theme} />
    </DiagramBase>
  );
}
