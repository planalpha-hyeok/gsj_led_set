import { describe, expect, it } from 'vitest';
import type { Cell } from '../types';
import { pressArrow, pressBackspace, pressSpace, type KbState } from './keyboardWiring';

type R = { id: string; cells: Cell[] };
let n = 0;
const make = (): R => ({ id: `new${++n}`, cells: [] });
const st = (c: number, r: number, drawing = false, activeId?: string): KbState => ({ cursor: { c, r }, drawing, activeId });
const C = (c: number, r: number) => ({ c, r });

describe('스페이스', () => {
  it('빈 캐비닛 + 선택된 배선이 비어 있음 → 그 배선에서 시작', () => {
    const res = pressSpace([{ id: 'a', cells: [] }], st(1, 1, false, 'a'), make);
    expect(res.runs[0].cells).toEqual([C(1, 1)]);
    expect(res.state).toMatchObject({ drawing: true, activeId: 'a' });
  });
  it('빈 캐비닛 + 선택된 배선에 이미 연결 있음 → 새 배선 생성', () => {
    const res = pressSpace([{ id: 'a', cells: [C(0, 0)] }], st(2, 2, false, 'a'), make);
    expect(res.runs).toHaveLength(2);
    expect(res.runs[1].cells).toEqual([C(2, 2)]);
    expect(res.state.activeId).toBe(res.runs[1].id);
  });
  it('배선 중 → 끝', () => {
    const runs = [{ id: 'a', cells: [C(0, 0)] }];
    const res = pressSpace(runs, st(0, 0, true, 'a'), make);
    expect(res.runs).toBe(runs);
    expect(res.state.drawing).toBe(false);
  });
  const three = [{ id: 'a', cells: [C(0, 0), C(1, 0), C(2, 0)] }];
  it('시작 캐비닛 → 전체 삭제 (배선은 남김)', () => {
    const res = pressSpace(three, st(0, 0), make);
    expect(res.runs).toEqual([{ id: 'a', cells: [] }]);
  });
  it('중간 캐비닛 → 거기부터 끝까지 삭제', () => {
    expect(pressSpace(three, st(1, 0), make).runs[0].cells).toEqual([C(0, 0)]);
  });
  it('마지막 캐비닛 → 이어서 그리기', () => {
    const res = pressSpace(three, st(2, 0), make);
    expect(res.runs).toBe(three);
    expect(res.state).toMatchObject({ drawing: true, activeId: 'a' });
  });
});

describe('방향키', () => {
  it('대기 중에는 커서만 이동, 화면 밖으로는 안 감', () => {
    const runs = [{ id: 'a', cells: [] }];
    expect(pressArrow(runs, st(0, 0), 1, 0, false, 3, 3).state.cursor).toEqual(C(1, 0));
    expect(pressArrow(runs, st(0, 0), -1, 0, false, 3, 3).state.cursor).toEqual(C(0, 0));
  });
  it('배선 중 이동 → 연결, 되돌아가면 취소', () => {
    let res = pressArrow([{ id: 'a', cells: [C(0, 0)] }], st(0, 0, true, 'a'), 0, 1, false, 3, 3);
    expect(res.runs[0].cells).toEqual([C(0, 0), C(0, 1)]);
    res = pressArrow(res.runs, res.state, 0, -1, false, 3, 3);
    expect(res.runs[0].cells).toEqual([C(0, 0)]);
    expect(res.state.cursor).toEqual(C(0, 0));
  });
  it('Shift: 끝까지, 다른 배선을 만나면 멈춤', () => {
    const runs = [{ id: 'a', cells: [C(0, 0)] }, { id: 'b', cells: [C(3, 0)] }];
    const res = pressArrow(runs, st(0, 0, true, 'a'), 1, 0, true, 5, 1);
    expect(res.runs[0].cells).toEqual([C(0, 0), C(1, 0), C(2, 0)]);
    expect(res.state.cursor).toEqual(C(2, 0));
  });
});

describe('Backspace', () => {
  it('마지막 1개 취소, 커서는 새 끝으로', () => {
    const res = pressBackspace([{ id: 'a', cells: [C(0, 0), C(1, 0)] }], st(1, 0, true, 'a'));
    expect(res.runs[0].cells).toEqual([C(0, 0)]);
    expect(res.state).toMatchObject({ cursor: C(0, 0), drawing: true });
  });
});
