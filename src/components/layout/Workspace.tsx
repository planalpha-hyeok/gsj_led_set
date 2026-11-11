import { useState } from 'react';
import { useProject } from '../../store/ProjectContext';
import { diagramSize, type ViewKind } from '../diagram/diagramSize';
import { ZoomPan } from '../diagram/ZoomPan';
import { MappingDiagram } from '../mapping/MappingDiagram';
import { PowerDiagram } from '../power/PowerDiagram';
import { ReportView } from '../report/ReportView';
import { ScreenDiagram } from '../screen/ScreenDiagram';
import { Icon, type IconName } from '../ui/Icon';
import { useWiringEditor } from '../wiring/useWiringEditor';
import { WiringBar } from '../wiring/WiringBar';

export type TabKind = ViewKind | 'report';

export const TABS: { id: TabKind; label: string; icon: IconName }[] = [
  { id: 'screen', label: '화면 배치', icon: 'grid' },
  { id: 'mapping', label: '데이터 배선', icon: 'route' },
  { id: 'power', label: '전원 배선', icon: 'bolt' },
  { id: 'report', label: '요약 보고서', icon: 'report' },
];

interface Props {
  tab: TabKind;
  onTab: (t: TabKind) => void;
}

/** 가운데 작업 영역: 탭 + 배선 편집 + 배선도 */
export function Workspace({ tab, onTab }: Props) {
  const { project: p, analysis: a } = useProject();
  const [dataSel, setDataSel] = useState<string>();
  const [powerSel, setPowerSel] = useState<string>();

  // 선택한 배선이 삭제/실행 취소로 사라지면 첫 번째 배선을 선택
  const dataActive = p.dataRuns.some((r) => r.id === dataSel) ? dataSel : p.dataRuns[0]?.id;
  const powerActive = p.powerRuns.some((r) => r.id === powerSel) ? powerSel : p.powerRuns[0]?.id;

  const dataEditor = useWiringEditor('data', dataActive, setDataSel);
  const powerEditor = useWiringEditor('power', powerActive, setPowerSel);

  const renderStage = () => {
    if (tab === 'report') return <ReportView />;
    const size = diagramSize(tab, p, a);
    const props = { project: p, analysis: a, theme: 'dark' as const };
    const fitKey = `${tab}-${p.cols}-${p.rows}-${p.id}`;
    if (tab === 'screen') {
      return <ZoomPan width={size.width} height={size.height} fitKey={fitKey}><ScreenDiagram {...props} /></ZoomPan>;
    }
    const isData = tab === 'mapping';
    const activeIndex = isData
      ? p.dataRuns.findIndex((r) => r.id === dataActive)
      : p.powerRuns.findIndex((r) => r.id === powerActive);
    return (
      <div className="stage-edit">
        <WiringBar kind={isData ? 'data' : 'power'} activeId={isData ? dataActive : powerActive}
          setActiveId={isData ? setDataSel : setPowerSel} />
        <div className="stage-canvas">
          <ZoomPan width={size.width} height={size.height} fitKey={fitKey} interaction={isData ? dataEditor : powerEditor}>
            {isData
              ? <MappingDiagram {...props} activeIndex={activeIndex >= 0 ? activeIndex : undefined} />
              : <PowerDiagram {...props} activeIndex={activeIndex >= 0 ? activeIndex : undefined} />}
          </ZoomPan>
        </div>
      </div>
    );
  };

  return (
    <main className="workspace">
      <nav className="tabs" role="tablist">
        {TABS.map((t) => (
          <button key={t.id} type="button" role="tab" aria-selected={tab === t.id}
            className={`tab ${tab === t.id ? 'on' : ''}`} onClick={() => onTab(t.id)}>
            <Icon name={t.icon} size={15} />
            {t.label}
          </button>
        ))}
      </nav>
      <div className="stage">{renderStage()}</div>
    </main>
  );
}
