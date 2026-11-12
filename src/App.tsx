import { useState } from 'react';
import { InfoPanel } from './components/layout/InfoPanel';
import { Sidebar } from './components/layout/Sidebar';
import { TopBar } from './components/layout/TopBar';
import { Workspace, type TabKind } from './components/layout/Workspace';
import { ProjectProvider } from './store/ProjectContext';
import { ThemeProvider } from './store/theme';

export default function App() {
  const [tab, setTab] = useState<TabKind>('mapping');
  return (
    <ThemeProvider>
    <ProjectProvider>
      <div className="app">
        <TopBar tab={tab} />
        <div className="body">
          <Sidebar />
          <Workspace tab={tab} onTab={setTab} />
          <InfoPanel />
        </div>
      </div>
    </ProjectProvider>
    </ThemeProvider>
  );
}
