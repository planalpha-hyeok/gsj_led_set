import { useState } from 'react';
import { defaultProject, newId } from '../../store/defaults';
import { deleteProject, listProjects, normalizeProject } from '../../store/persistence';
import { useProject } from '../../store/ProjectContext';
import { Icon } from '../ui/Icon';
import { Modal } from '../ui/Modal';

/** 브라우저에 저장된 프로젝트 목록 */
export function ProjectsDialog({ onClose }: { onClose: () => void }) {
  const { project, setProject } = useProject();
  const [list, setList] = useState(listProjects);

  const open = (id: string) => {
    const p = list.find((x) => x.id === id);
    if (p) setProject(normalizeProject(p));
    onClose();
  };

  const remove = (id: string, name: string) => {
    if (id === project.id) return;
    if (!window.confirm(`"${name}" 프로젝트를 삭제할까요? 되돌릴 수 없습니다.`)) return;
    deleteProject(id);
    setList(listProjects());
  };

  return (
    <Modal title="프로젝트" onClose={onClose} width={560}
      footer={<>
        <button type="button" className="btn" onClick={() => { setProject({ ...project, id: newId(), name: `${project.name} 복사본`, updatedAt: Date.now() }); onClose(); }}>
          <Icon name="copy" size={14} /> 현재 프로젝트 복제
        </button>
        <button type="button" className="btn primary" onClick={() => { setProject(defaultProject()); onClose(); }}>
          <Icon name="plus" size={14} /> 새 프로젝트
        </button>
      </>}>
      <ul className="project-list">
        {list.length === 0 && <li className="empty">저장된 프로젝트가 없습니다</li>}
        {list.map((p) => (
          <li key={p.id} className={p.id === project.id ? 'current' : ''}>
            <button type="button" className="project-open" onClick={() => open(p.id)}>
              <strong>{p.name}</strong>
              <span className="mono">{p.cols}×{p.rows} · {p.cabinet.name} · {new Date(p.updatedAt).toLocaleString('ko-KR')}</span>
            </button>
            {p.id === project.id
              ? <span className="tag">열려 있음</span>
              : <button type="button" className="icon-btn danger" title="삭제" onClick={() => remove(p.id, p.name)}><Icon name="trash" size={15} /></button>}
          </li>
        ))}
      </ul>
      <p className="hint">프로젝트는 이 브라우저에 자동 저장됩니다. 브라우저 데이터를 지우면 사라지므로 중요한 프로젝트는 "파일 저장"으로 보관하세요.</p>
    </Modal>
  );
}
