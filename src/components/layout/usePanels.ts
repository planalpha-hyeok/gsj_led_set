import { useEffect, useState } from 'react';

/** 넓은 화면 기준 (이상이면 설정·결과 패널을 옆에 펼쳐 둠) */
export const WIDE_QUERY = '(min-width: 1400px)';

export function useMediaQuery(query: string): boolean {
  const [match, setMatch] = useState(() => typeof window !== 'undefined' && window.matchMedia(query).matches);
  useEffect(() => {
    const mq = window.matchMedia(query);
    const on = () => setMatch(mq.matches);
    on();
    mq.addEventListener('change', on);
    return () => mq.removeEventListener('change', on);
  }, [query]);
  return match;
}

export interface Panels {
  wide: boolean;
  left: boolean;
  right: boolean;
  toggleLeft: () => void;
  toggleRight: () => void;
  closeAll: () => void;
}

/**
 * 설정(왼쪽)·결과(오른쪽) 패널 열림 상태
 * - 넓은 화면: 기본 펼침, 옆에 고정 (접을 수 있음)
 * - 좁은 화면: 기본 접힘, 열면 배선도 위에 덮는 패널 (하나만 열림)
 */
export function usePanels(): Panels {
  const wide = useMediaQuery(WIDE_QUERY);
  const [left, setLeft] = useState(wide);
  const [right, setRight] = useState(wide);

  useEffect(() => {
    setLeft(wide);
    setRight(wide);
  }, [wide]);

  return {
    wide,
    left,
    right,
    toggleLeft: () => {
      setLeft((v) => !v);
      if (!wide) setRight(false);
    },
    toggleRight: () => {
      setRight((v) => !v);
      if (!wide) setLeft(false);
    },
    closeAll: () => {
      setLeft(false);
      setRight(false);
    },
  };
}
