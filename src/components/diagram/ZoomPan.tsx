import { useCallback, useEffect, useLayoutEffect, useRef, useState, type ReactNode } from 'react';
import { Icon } from '../ui/Icon';

interface Props {
  /** 내용 크기 (SVG width/height) */
  width: number;
  height: number;
  /** 이 값이 바뀌면 화면에 맞춤을 다시 실행 */
  fitKey: string;
  children: ReactNode;
}

/** 배선도 확대/이동: 휠 = 확대·축소, 드래그 = 이동 */
export function ZoomPan({ width, height, fitKey, children }: Props) {
  const box = useRef<HTMLDivElement>(null);
  const [view, setView] = useState({ s: 1, x: 0, y: 0 });
  const drag = useRef<{ x: number; y: number; vx: number; vy: number } | null>(null);

  const fit = useCallback(() => {
    const el = box.current;
    if (!el || width <= 0 || height <= 0) return;
    const cw = el.clientWidth;
    const ch = el.clientHeight;
    const s = Math.min(cw / width, ch / height) * 0.94;
    setView({ s, x: (cw - width * s) / 2, y: (ch - height * s) / 2 });
  }, [width, height]);

  useLayoutEffect(fit, [fitKey, fit]);

  useEffect(() => {
    const el = box.current;
    if (!el) return;
    const ro = new ResizeObserver(() => fit());
    ro.observe(el);
    return () => ro.disconnect();
  }, [fit]);

  // 휠 확대: 마우스 위치 기준
  useEffect(() => {
    const el = box.current;
    if (!el) return;
    const onWheel = (e: WheelEvent) => {
      e.preventDefault();
      const rect = el.getBoundingClientRect();
      const mx = e.clientX - rect.left;
      const my = e.clientY - rect.top;
      setView((v) => {
        const s = Math.min(8, Math.max(0.05, v.s * (e.deltaY < 0 ? 1.12 : 1 / 1.12)));
        const k = s / v.s;
        return { s, x: mx - (mx - v.x) * k, y: my - (my - v.y) * k };
      });
    };
    el.addEventListener('wheel', onWheel, { passive: false });
    return () => el.removeEventListener('wheel', onWheel);
  }, []);

  const zoomBy = (k: number) => {
    const el = box.current;
    if (!el) return;
    const cx = el.clientWidth / 2;
    const cy = el.clientHeight / 2;
    setView((v) => {
      const s = Math.min(8, Math.max(0.05, v.s * k));
      const r = s / v.s;
      return { s, x: cx - (cx - v.x) * r, y: cy - (cy - v.y) * r };
    });
  };

  return (
    <div
      ref={box}
      className="zoompan"
      onPointerDown={(e) => {
        if ((e.target as HTMLElement).closest('.zoom-tools')) return;
        (e.currentTarget as HTMLDivElement).setPointerCapture(e.pointerId);
        drag.current = { x: e.clientX, y: e.clientY, vx: view.x, vy: view.y };
      }}
      onPointerMove={(e) => {
        const d = drag.current;
        if (d) setView((v) => ({ ...v, x: d.vx + e.clientX - d.x, y: d.vy + e.clientY - d.y }));
      }}
      onPointerUp={() => { drag.current = null; }}
      onPointerCancel={() => { drag.current = null; }}
    >
      <div className="zoompan-inner" style={{ transform: `translate(${view.x}px, ${view.y}px) scale(${view.s})` }}>
        {children}
      </div>
      <div className="zoom-tools">
        <button type="button" className="icon-btn" onClick={() => zoomBy(1 / 1.25)} title="축소"><Icon name="zoomOut" /></button>
        <span className="zoom-value mono">{Math.round(view.s * 100)}%</span>
        <button type="button" className="icon-btn" onClick={() => zoomBy(1.25)} title="확대"><Icon name="zoomIn" /></button>
        <button type="button" className="icon-btn" onClick={fit} title="화면에 맞춤"><Icon name="fit" /></button>
      </div>
    </div>
  );
}
