import { useCallback, useEffect, useLayoutEffect, useRef, useState, type ReactNode } from 'react';
import type { Cell } from '../../types';
import { Icon } from '../ui/Icon';

/** 캐비닛 클릭/드래그 처리 (배선 편집용) */
export interface CellInteraction {
  /** 캐비닛 위에서 누름. true를 돌려주면 화면 이동을 하지 않음 */
  onDown: (cell: Cell) => boolean;
  /** 누른 채로 다른 캐비닛 위로 이동 */
  onMove: (cell: Cell) => void;
  onUp: () => void;
}

interface Props {
  /** 내용 크기 (SVG width/height) */
  width: number;
  height: number;
  /** 이 값이 바뀌면 화면에 맞춤을 다시 실행 */
  fitKey: string;
  interaction?: CellInteraction;
  /** 항상 보이도록 따라갈 영역 (내용 좌표, 키보드 커서 위치) */
  focus?: { x: number; y: number; w: number; h: number };
  children: ReactNode;
}

const MIN_S = 0.05;
const MAX_S = 8;

/** 화면 좌표에 있는 캐비닛 찾기 (터치에서도 동작하도록 elementFromPoint 사용) */
function cellAt(x: number, y: number): Cell | null {
  const el = document.elementFromPoint(x, y)?.closest('[data-c]');
  if (!el) return null;
  return { c: Number(el.getAttribute('data-c')), r: Number(el.getAttribute('data-r')) };
}

/**
 * 배선도 보기
 * - 휠: 상하 이동 / Shift+휠: 좌우 이동 / Ctrl+휠(노트북 터치패드 확대 동작 포함): 확대·축소
 * - 빈 곳 드래그 또는 가운데 버튼 드래그: 이동
 * - 터치: 두 손가락 벌리기/모으기로 확대·축소
 * - 내용이 화면 안에 다 들어와서 더 이동할 곳이 없으면 휠은 페이지 스크롤로 넘김
 */
export function ZoomPan({ width, height, fitKey, interaction, focus, children }: Props) {
  const box = useRef<HTMLDivElement>(null);
  const [view, setView] = useState({ s: 1, x: 0, y: 0 });
  const viewRef = useRef(view);
  viewRef.current = view;
  const pan = useRef<{ x: number; y: number; vx: number; vy: number } | null>(null);
  const painting = useRef(false);
  const lastCell = useRef<string>('');
  const interactionRef = useRef(interaction);
  interactionRef.current = interaction;

  /** 내용이 화면 밖으로 완전히 사라지지 않도록 위치 제한 */
  const clamp = useCallback((v: { s: number; x: number; y: number }) => {
    const el = box.current;
    if (!el) return v;
    const cw = el.clientWidth;
    const ch = el.clientHeight;
    const w = width * v.s;
    const h = height * v.s;
    const keep = 60;
    const x = Math.min(cw - keep, Math.max(keep - w, v.x));
    const y = Math.min(ch - keep, Math.max(keep - h, v.y));
    return { s: v.s, x, y };
  }, [width, height]);

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

  useEffect(() => {
    const el = box.current;
    if (!el) return;
    const onWheel = (e: WheelEvent) => {
      const v = viewRef.current;
      if (e.ctrlKey || e.metaKey) {
        e.preventDefault();
        const rect = el.getBoundingClientRect();
        const mx = e.clientX - rect.left;
        const my = e.clientY - rect.top;
        const s = Math.min(MAX_S, Math.max(MIN_S, v.s * Math.exp(-e.deltaY * 0.0025)));
        const k = s / v.s;
        setView(clamp({ s, x: mx - (mx - v.x) * k, y: my - (my - v.y) * k }));
        return;
      }
      let dx = e.deltaX;
      let dy = e.deltaY;
      if (e.shiftKey && dx === 0) { dx = dy; dy = 0; }
      const next = clamp({ s: v.s, x: v.x - dx, y: v.y - dy });
      // 더 이동할 수 없으면 페이지 스크롤로 넘김
      if (next.x === v.x && next.y === v.y) return;
      e.preventDefault();
      setView(next);
    };
    el.addEventListener('wheel', onWheel, { passive: false });
    return () => el.removeEventListener('wheel', onWheel);
  }, [clamp]);

  // 커서가 화면 밖으로 나가면 보이는 위치까지만 이동
  const fx = focus?.x;
  const fy = focus?.y;
  useEffect(() => {
    const el = box.current;
    if (!el || !focus) return;
    const v = viewRef.current;
    const m = 40;
    const left = focus.x * v.s + v.x;
    const top = focus.y * v.s + v.y;
    const right = left + focus.w * v.s;
    const bottom = top + focus.h * v.s;
    let dx = 0;
    let dy = 0;
    if (left < m) dx = m - left;
    else if (right > el.clientWidth - m) dx = el.clientWidth - m - right;
    if (top < m) dy = m - top;
    else if (bottom > el.clientHeight - m - 50) dy = el.clientHeight - m - 50 - bottom;
    if (dx || dy) setView(clamp({ s: v.s, x: v.x + dx, y: v.y + dy }));
  }, [fx, fy]); // 커서 위치가 바뀔 때만 실행

  const zoomBy = (k: number) => {
    const el = box.current;
    if (!el) return;
    const cx = el.clientWidth / 2;
    const cy = el.clientHeight / 2;
    setView((v) => {
      const s = Math.min(MAX_S, Math.max(MIN_S, v.s * k));
      const r = s / v.s;
      return clamp({ s, x: cx - (cx - v.x) * r, y: cy - (cy - v.y) * r });
    });
  };

  // 두 손가락 확대: 누른 손가락 위치와 시작 상태
  const touches = useRef(new Map<number, { x: number; y: number }>());
  const pinch = useRef<{ d: number; mx: number; my: number; s: number; x: number; y: number } | null>(null);
  // 터치 한 손가락: 두 번째 손가락(확대)인지 알 수 있을 때까지 연결을 미룸
  const pendingTouch = useRef<{ x: number; y: number; vx: number; vy: number } | null>(null);

  /** 미뤄둔 터치를 실제 동작(연결 또는 이동)으로 시작 */
  const commitTouch = (cx: number, cy: number) => {
    const p = pendingTouch.current;
    pendingTouch.current = null;
    if (!p) return;
    const it = interactionRef.current;
    const cell = it ? cellAt(p.x, p.y) : null;
    if (it && cell && it.onDown(cell)) {
      painting.current = true;
      lastCell.current = `${cell.c},${cell.r}`;
      const now = cellAt(cx, cy);
      if (now && `${now.c},${now.r}` !== lastCell.current) {
        lastCell.current = `${now.c},${now.r}`;
        it.onMove(now);
      }
      return;
    }
    pan.current = { x: p.x, y: p.y, vx: p.vx, vy: p.vy };
  };

  const local = (cx: number, cy: number) => {
    const r = box.current!.getBoundingClientRect();
    return { x: cx - r.left, y: cy - r.top };
  };

  const startPinch = () => {
    const [a, b] = [...touches.current.values()];
    pendingTouch.current = null;
    if (painting.current) interactionRef.current?.onUp();
    painting.current = false;
    pan.current = null;
    const v = viewRef.current;
    pinch.current = { d: Math.hypot(a.x - b.x, a.y - b.y) || 1, mx: (a.x + b.x) / 2, my: (a.y + b.y) / 2, s: v.s, x: v.x, y: v.y };
  };

  const movePinch = () => {
    const p = pinch.current;
    if (!p) return;
    const [a, b] = [...touches.current.values()];
    const d = Math.hypot(a.x - b.x, a.y - b.y) || 1;
    const mx = (a.x + b.x) / 2;
    const my = (a.y + b.y) / 2;
    const s = Math.min(MAX_S, Math.max(MIN_S, p.s * (d / p.d)));
    // 처음 두 손가락 가운데에 있던 지점이 지금 가운데에 오도록
    const cx = (p.mx - p.x) / p.s;
    const cy = (p.my - p.y) / p.s;
    setView(clamp({ s, x: mx - cx * s, y: my - cy * s }));
  };

  /** 포인터 고정 (지원 안 되는 환경에서는 무시) */
  const capture = (e: React.PointerEvent) => {
    try {
      (e.currentTarget as HTMLDivElement).setPointerCapture(e.pointerId);
    } catch {
      /* 무시 */
    }
  };

  const stopAll = (e?: React.PointerEvent) => {
    if (e) touches.current.delete(e.pointerId);
    // 짧게 누르고 뗀 터치 = 탭 → 클릭과 같게 처리
    if (pendingTouch.current && e && touches.current.size === 0) commitTouch(e.clientX, e.clientY);
    if (touches.current.size >= 2) return;
    if (pinch.current) {
      pinch.current = null;
      return;
    }
    if (painting.current) interactionRef.current?.onUp();
    painting.current = false;
    pan.current = null;
    lastCell.current = '';
  };

  return (
    <div
      ref={box}
      className={`zoompan ${interaction ? 'editable' : ''}`}
      onPointerDown={(e) => {
        if ((e.target as HTMLElement).closest('.zoom-tools')) return;
        if (e.pointerType === 'touch') {
          touches.current.set(e.pointerId, local(e.clientX, e.clientY));
          capture(e);
          if (touches.current.size === 2) {
            startPinch();
            return;
          }
          if (touches.current.size > 2) return;
          // 첫 손가락: 바로 연결하지 않고 대기
          pendingTouch.current = { x: e.clientX, y: e.clientY, vx: view.x, vy: view.y };
          return;
        }
        const it = interactionRef.current;
        if (e.button === 0 && it) {
          const cell = cellAt(e.clientX, e.clientY);
          if (cell && it.onDown(cell)) {
            painting.current = true;
            lastCell.current = `${cell.c},${cell.r}`;
            capture(e);
            return;
          }
        }
        if (e.button !== 0 && e.button !== 1) return;
        e.preventDefault();
        capture(e);
        pan.current = { x: e.clientX, y: e.clientY, vx: view.x, vy: view.y };
      }}
      onPointerMove={(e) => {
        if (e.pointerType === 'touch' && touches.current.has(e.pointerId)) {
          touches.current.set(e.pointerId, local(e.clientX, e.clientY));
          if (pinch.current && touches.current.size >= 2) {
            movePinch();
            return;
          }
        }
        if (pinch.current) return;
        if (pendingTouch.current) {
          const p = pendingTouch.current;
          if (Math.hypot(e.clientX - p.x, e.clientY - p.y) < 6) return;
          commitTouch(e.clientX, e.clientY);
        }
        if (painting.current) {
          const cell = cellAt(e.clientX, e.clientY);
          const key = cell ? `${cell.c},${cell.r}` : '';
          if (cell && key !== lastCell.current) {
            lastCell.current = key;
            interactionRef.current?.onMove(cell);
          }
          return;
        }
        const d = pan.current;
        if (d) setView((v) => clamp({ ...v, x: d.vx + e.clientX - d.x, y: d.vy + e.clientY - d.y }));
      }}
      onPointerUp={(e) => stopAll(e)}
      onPointerCancel={(e) => stopAll(e)}
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
