import { useRef, useState } from 'react';
import { exportPdf, exportPng } from '../../export/exportImage';
import { exportProjectJson, importProjectJson } from '../../export/projectFile';
import { useProject } from '../../store/ProjectContext';
import { Icon } from '../ui/Icon';
import { ProjectsDialog } from './ProjectsDialog';
import type { TabKind } from './Workspace';

/** 상단 바: 프로젝트 이름, 저장/불러오기, 내보내기 */
export function TopBar({ tab }: { tab: TabKind }) {
  const { project, analysis, update, setProject, undo, redo, canUndo, canRedo } = useProject();
  const [dialog, setDialog] = useState(false);
  const [busy, setBusy] = useState<string | null>(null);
  const fileRef = useRef<HTMLInputElement>(null);

  const run = async (label: string, fn: () => Promise<void>) => {
    setBusy(label);
    try {
      await fn();
    } catch (e) {
      window.alert(`${label} 실패: ${(e as Error).message}`);
    } finally {
      setBusy(null);
    }
  };

  const onFile = async (f: File | undefined) => {
    if (!f) return;
    try {
      setProject(await importProjectJson(f));
    } catch (e) {
      window.alert(`불러오기 실패: ${(e as Error).message}`);
    }
    if (fileRef.current) fileRef.current.value = '';
  };

  return (
    <header className="topbar">
      <div className="brand">
        <span className="brand-mark" aria-hidden="true">
          {Array.from({ length: 9 }, (_, i) => <i key={i} />)}
        </span>
        <span className="brand-name">LED Wall Planner</span>
      </div>

      <input className="project-name" value={project.name} onChange={(e) => update({ name: e.target.value }, 'name')}
        aria-label="프로젝트 이름" spellCheck={false} />

      <div className="topbar-actions">
        <button type="button" className="icon-btn" onClick={undo} disabled={!canUndo} title="실행 취소 (Ctrl+Z)"><Icon name="undo" size={16} /></button>
        <button type="button" className="icon-btn" onClick={redo} disabled={!canRedo} title="다시 실행 (Ctrl+Y)"><Icon name="redo" size={16} /></button>
        <span className="divider" />
        <button type="button" className="btn ghost" onClick={() => setDialog(true)}><Icon name="folder" size={15} />프로젝트</button>
        <button type="button" className="btn ghost" onClick={() => exportProjectJson(project)}><Icon name="save" size={15} />파일 저장</button>
        <button type="button" className="btn ghost" onClick={() => fileRef.current?.click()}><Icon name="upload" size={15} />파일 열기</button>
        <input ref={fileRef} type="file" accept=".json,application/json" hidden onChange={(e) => onFile(e.target.files?.[0])} />
        <span className="divider" />
        <button type="button" className="btn" disabled={!!busy}
          onClick={() => run('PNG 저장', () => exportPng(tab, project, analysis, 'light'))}>
          <Icon name="download" size={15} />{busy === 'PNG 저장' ? '저장 중…' : 'PNG'}
        </button>
        <button type="button" className="btn primary" disabled={!!busy}
          onClick={() => run('PDF 저장', () => exportPdf(project, analysis))}>
          <Icon name="download" size={15} />{busy === 'PDF 저장' ? '만드는 중…' : 'PDF 보고서'}
        </button>
      </div>

      {dialog && <ProjectsDialog onClose={() => setDialog(false)} />}
    </header>
  );
}
