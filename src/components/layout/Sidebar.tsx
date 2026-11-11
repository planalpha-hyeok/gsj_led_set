import { DeviceSettings } from '../devices/DeviceSettings';
import { PowerSettings } from '../power/PowerSettings';
import { ScreenSettings } from '../screen/ScreenSettings';

/** 왼쪽 설정 패널 */
export function Sidebar() {
  return (
    <aside className="sidebar">
      <ScreenSettings />
      <DeviceSettings />
      <PowerSettings />
    </aside>
  );
}
