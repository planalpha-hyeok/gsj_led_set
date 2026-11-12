import { DeviceSettings } from '../devices/DeviceSettings';
import { PowerSettings } from '../power/PowerSettings';
import { ScreenSettings } from '../screen/ScreenSettings';

/** 왼쪽 설정 패널 */
export function Sidebar({ open }: { open: boolean }) {
  return (
    <aside className={`sidebar ${open ? 'open' : ''}`} aria-hidden={!open}>
      <ScreenSettings />
      <DeviceSettings />
      <PowerSettings />
    </aside>
  );
}
