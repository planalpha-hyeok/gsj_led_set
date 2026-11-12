import { createContext, useContext, useEffect, useState, type ReactNode } from 'react';

/** 화면 모드 (라이트/다크). 선택은 이 브라우저에 기억한다. */
export type AppTheme = 'light' | 'dark';

const KEY = 'gsj-led:theme';

function readTheme(): AppTheme {
  try {
    return localStorage.getItem(KEY) === 'dark' ? 'dark' : 'light';
  } catch {
    return 'light';
  }
}

interface ThemeStore {
  theme: AppTheme;
  setTheme: (t: AppTheme) => void;
}

const Ctx = createContext<ThemeStore>({ theme: 'light', setTheme: () => undefined });

export function ThemeProvider({ children }: { children: ReactNode }) {
  const [theme, setTheme] = useState<AppTheme>(readTheme);
  useEffect(() => {
    document.documentElement.setAttribute('data-theme', theme);
    try {
      localStorage.setItem(KEY, theme);
    } catch {
      /* 저장 불가 → 무시 */
    }
  }, [theme]);
  return <Ctx.Provider value={{ theme, setTheme }}>{children}</Ctx.Provider>;
}

export function useTheme(): ThemeStore {
  return useContext(Ctx);
}
