import { useProject } from '../../store/ProjectContext';
import { diagramSize, type ViewKind } from '../diagram/diagramSize';
import { ZoomPan } from '../diagram/ZoomPan';
import { MappingDiagram } from '../mapping/MappingDiagram';
import { PowerDiagram } from '../power/PowerDiagram';
import { ReportView } from '../report/ReportView';
import { ScreenDiagram } from '../screen/ScreenDiagram';
import { Icon, type IconName } from '../ui/Icon';

export type TabKind = ViewKind | 'report';

export const TABS: { id: TabKind; label: string; icon: IconName }[] = [
  { id: 'screen', label: '화면 배치', icon: 'grid' },
  { id: 'mapping', label: '데이터 매핑', icon: 'route' },
  { id: 'power', label: '전원 배선', icon: 'bolt' },
  { id: 'report', label: '요약 보고서', icon: 'report' },
];

interface Props {
  tab: TabKind;
  onTab: (t: TabKind) => void;
}

/** 가운데 작업 영역: 탭 + 배선도 */
export function Workspace({ tab, onTab }: Props) {
  const { project: p, analysis: a } = useProject();

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
      <div className="stage">
        {tab === 'report' ? (
          <ReportView />
        ) : (
          (() => {
            const size = diagramSize(tab, p, a);
            const props = { project: p, analysis: a, theme: 'dark' as const };
            return (
              <ZoomPan width={size.width} height={size.height} fitKey={`${tab}-${p.cols}-${p.rows}-${p.id}`}>
                {tab === 'screen' && <ScreenDiagram {...props} />}
                {tab === 'mapping' && <MappingDiagram {...props} />}
                {tab === 'power' && <PowerDiagram {...props} />}
              </ZoomPan>
            );
          })()
        )}
      </div>
    </main>
  );
}
