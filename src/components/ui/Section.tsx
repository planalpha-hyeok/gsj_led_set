import { useState, type ReactNode } from 'react';
import { Icon, type IconName } from './Icon';

interface Props {
  title: string;
  icon?: IconName;
  aside?: ReactNode;
  defaultOpen?: boolean;
  children: ReactNode;
}

/** 사이드 패널의 접을 수 있는 설정 묶음 */
export function Section({ title, icon, aside, defaultOpen = true, children }: Props) {
  const [open, setOpen] = useState(defaultOpen);
  return (
    <section className={`section ${open ? 'open' : ''}`}>
      <div className="section-head">
        <button type="button" className="section-toggle" onClick={() => setOpen(!open)} aria-expanded={open}>
          {icon && <Icon name={icon} size={15} />}
          <span className="section-title">{title}</span>
          <Icon name="chevron" size={14} className="section-chevron" />
        </button>
        {aside && <span className="section-aside">{aside}</span>}
      </div>
      {open && <div className="section-body">{children}</div>}
    </section>
  );
}
