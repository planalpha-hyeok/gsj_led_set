import { useState } from 'react';
import { InfoPanel } from './components/layout/InfoPanel';
import { Sidebar } from './components/layout/Sidebar';
import { TopBar } from './components/layout/TopBar';
import { usePanels } from './components/layout/usePanels';
import { Workspace, type TabKind } from './components/layout/Workspace';
import { ProjectProvider } from './store/ProjectContext';
import { ThemeProvider } from './store/theme';

export default function App() {
  const [tab, setTab] = useState<TabKind>('mapping');
  const panels = usePanels();
  const bodyClass = [
    'body',
    panels.wide ? 'wide' : 'narrow',
    panels.left ? 'left-open' : '',
    panels.right ? 'right-open' : '',
  ].join(' ');

  return (
    <ThemeProvider>
      <ProjectProvider>
        <div className="app">
          <TopBar tab={tab} panels={panels} />
          <div className={bodyClass}>
            <Sidebar open={panels.left} />
            <Workspace tab={tab} onTab={setTab} />
            <InfoPanel open={panels.right} />
            {!panels.wide && (panels.left || panels.right) && (
              <div className="drawer-backdrop" onClick={panels.closeAll} aria-hidden="true" />
            )}
          </div>
        </div>
      </ProjectProvider>
    </ThemeProvider>
  );
}
