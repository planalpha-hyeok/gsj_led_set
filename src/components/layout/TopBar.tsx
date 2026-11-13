import { useEffect, useRef, useState } from 'react';
import { exportPdf, exportPng } from '../../export/exportImage';
import { exportProjectJson, importProjectJson } from '../../export/projectFile';
import { useProject } from '../../store/ProjectContext';
import { useInstall } from '../../store/pwa';
import { useTheme } from '../../store/theme';
import { InstallDialog } from './InstallDialog';
import { Icon, type IconName } from '../ui/Icon';
import { ProjectsDialog } from './ProjectsDialog';
import type { Panels } from './usePanels';
import type { TabKind } from './Workspace';

interface MenuItem {
  icon: IconName;
  label: string;
  onClick: () => void;
  disabled?: boolean;
  /** 아주 좁은 화면에서만 메뉴에 표시 (넓으면 상단에 따로 있음) */
  xsOnly?: boolean;
  /** 1100px 미만에서만 메뉴에 표시 (넓으면 상단에 따로 있음) */
  narrowOnly?: boolean;
}

/** "⋯" 메뉴: 앱 설치, 좁은 화면에서는 파일 관련 버튼도 한 곳에 모음 */
function MoreMenu({ items }: { items: MenuItem[] }) {
  const [open, setOpen] = useState(false);
  const ref = useRef<HTMLDivElement>(null);
  useEffect(() => {
    if (!open) return;
    const close = (e: PointerEvent) => {
      if (!ref.current?.contains(e.target as Node)) setOpen(false);
    };
    window.addEventListener('pointerdown', close);
    return () => window.removeEventListener('pointerdown', close);
  }, [open]);
  return (
    <div className="more-menu" ref={ref}>
      <button type="button" className="icon-btn" onClick={() => setOpen(!open)} aria-expanded={open} aria-label="더보기" title="더보기">
        <Icon name="more" size={18} />
      </button>
      {open && (
        <div className="menu-pop" role="menu">
          {items.map((it) => (
            <button key={it.label} type="button" role="menuitem" className={`menu-item ${it.xsOnly ? 'xs-only' : ''} ${it.narrowOnly ? 'narrow-only' : ''}`}
              disabled={it.disabled} onClick={() => { setOpen(false); it.onClick(); }}>
              <Icon name={it.icon} size={16} />{it.label}
            </button>
          ))}
        </div>
      )}
    </div>
  );
}

/**
 * 상단 바: 패널 열기, 프로젝트 이름, 실행 취소, 저장/불러오기, 내보내기
 * 좁은 화면에서는 글자를 숨기고 파일 관련 버튼은 "⋯" 메뉴로 모아 한 줄을 유지
 */
export function TopBar({ tab, panels }: { tab: TabKind; panels: Panels }) {
  const { project, analysis, update, setProject, undo, redo, canUndo, canRedo } = useProject();
  const { theme, setTheme } = useTheme();
  const [dialog, setDialog] = useState(false);
  const [installHelp, setInstallHelp] = useState(false);
  const install = useInstall();
  const [busy, setBusy] = useState<string | null>(null);
  const fileRef = useRef<HTMLInputElement>(null);

  const errors = analysis.issues.filter((i) => i.severity === 'error').length;
  const warnings = analysis.issues.filter((i) => i.severity === 'warning').length;

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

  const toggleTheme = () => setTheme(theme === 'dark' ? 'light' : 'dark');
  const savePng = () => run('PNG 저장', () => exportPng(tab, project, analysis, theme));
  const savePdf = () => run('PDF 저장', () => exportPdf(project, analysis));
  const openProjects = () => setDialog(true);
  const saveFile = () => exportProjectJson(project);
  const openFile = () => fileRef.current?.click();

  return (
    <header className="topbar">
      <button type="button" className={`icon-btn panel-btn ${panels.left ? 'on' : ''}`} onClick={panels.toggleLeft}
        title="설정 패널" aria-pressed={panels.left}>
        <Icon name="sliders" size={18} /><span className="btn-label">설정</span>
      </button>

      <div className="brand">
        <span className="brand-mark" aria-hidden="true">
          {Array.from({ length: 9 }, (_, i) => <i key={i} />)}
        </span>
        <span className="brand-name">LED Wall Planner</span>
      </div>

      <input className="project-name" value={project.name} onChange={(e) => update({ name: e.target.value }, 'name')}
        aria-label="프로젝트 이름" spellCheck={false} />

      <div className="topbar-actions">
        <button type="button" className="icon-btn" onClick={undo} disabled={!canUndo} title="실행 취소 (Ctrl+Z)"><Icon name="undo" size={17} /></button>
        <button type="button" className="icon-btn" onClick={redo} disabled={!canRedo} title="다시 실행 (Ctrl+Y)"><Icon name="redo" size={17} /></button>
        <button type="button" className="icon-btn xs-hide" onClick={toggleTheme}
          title={theme === 'dark' ? '라이트 모드로' : '다크 모드로'} aria-label="화면 모드 전환">
          <Icon name={theme === 'dark' ? 'sun' : 'moon'} size={17} />
        </button>

        <span className="divider wide-only" />
        <button type="button" className="btn ghost wide-only" onClick={openProjects}><Icon name="folder" size={15} />프로젝트</button>
        <button type="button" className="btn ghost wide-only" onClick={saveFile}><Icon name="save" size={15} />파일 저장</button>
        <button type="button" className="btn ghost wide-only" onClick={openFile}><Icon name="upload" size={15} />파일 열기</button>
        <input ref={fileRef} type="file" accept=".json,application/json" hidden onChange={(e) => onFile(e.target.files?.[0])} />

        <MoreMenu items={[
          { icon: 'folder', label: '프로젝트', onClick: openProjects, narrowOnly: true },
          { icon: 'save', label: '파일 저장', onClick: saveFile, narrowOnly: true },
          { icon: 'upload', label: '파일 열기', onClick: openFile, narrowOnly: true },
          { icon: 'download', label: 'PNG 저장', onClick: savePng, disabled: !!busy, xsOnly: true },
          { icon: theme === 'dark' ? 'sun' : 'moon', label: theme === 'dark' ? '라이트 모드' : '다크 모드', onClick: toggleTheme, xsOnly: true },
          ...(install.installed ? [] : [{
            icon: 'install' as IconName, label: '앱 설치',
            onClick: async () => { if (!(await install.promptInstall())) setInstallHelp(true); },
          }]),
        ]} />

        <span className="divider" />
        <button type="button" className="btn xs-hide" disabled={!!busy} onClick={savePng} title="현재 탭을 PNG로 저장">
          <Icon name="download" size={15} /><span className="btn-label">{busy === 'PNG 저장' ? '저장 중…' : 'PNG'}</span>
        </button>
        <button type="button" className="btn primary" disabled={!!busy} onClick={savePdf} title="PDF 보고서 저장">
          <Icon name="download" size={15} /><span>{busy === 'PDF 저장' ? '…' : 'PDF'}</span>
        </button>

        <button type="button" className={`icon-btn panel-btn ${panels.right ? 'on' : ''}`} onClick={panels.toggleRight}
          title="결과 패널" aria-pressed={panels.right}>
          <Icon name="report" size={18} /><span className="btn-label">결과</span>
          {(errors > 0 || warnings > 0) && (
            <span className={`dot-badge ${errors ? 'err' : 'warn'}`}>{errors || warnings}</span>
          )}
        </button>
      </div>

      {dialog && <ProjectsDialog onClose={() => setDialog(false)} />}
      {installHelp && <InstallDialog onClose={() => setInstallHelp(false)} />}
    </header>
  );
}
