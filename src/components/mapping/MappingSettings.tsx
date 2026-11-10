import { useProject } from '../../store/ProjectContext';
import { RouteControls } from '../ui/RouteControls';
import { Section } from '../ui/Section';

/** 데이터 매핑 경로 설정 */
export function MappingSettings() {
  const { project: p, updateMapping } = useProject();
  return (
    <Section title="데이터 경로" icon="route">
      <RouteControls value={p.mapping} onChange={updateMapping} splitLabel="포트 분할" />
    </Section>
  );
}
