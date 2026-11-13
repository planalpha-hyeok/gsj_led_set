import { useEffect, useState } from 'react';

/**
 * 앱 설치(PWA)
 * - 서비스 워커 등록: 웹 주소(https)로 열었을 때만. 파일로 열면(file://) 등록하지 않음
 * - 브라우저의 설치 요청 이벤트(beforeinstallprompt)를 미리 받아 두었다가 "앱 설치" 버튼에서 사용
 */
interface InstallPromptEvent extends Event {
  prompt: () => Promise<void>;
  userChoice: Promise<{ outcome: 'accepted' | 'dismissed' }>;
}

let deferred: InstallPromptEvent | null = null;
const listeners = new Set<() => void>();
const notify = () => listeners.forEach((fn) => fn());

if (typeof window !== 'undefined') {
  window.addEventListener('beforeinstallprompt', (e) => {
    e.preventDefault();
    deferred = e as InstallPromptEvent;
    notify();
  });
  window.addEventListener('appinstalled', () => {
    deferred = null;
    notify();
  });
}

export function registerServiceWorker(): void {
  if (!import.meta.env.PROD || !('serviceWorker' in navigator) || !location.protocol.startsWith('http')) return;
  window.addEventListener('load', () => {
    navigator.serviceWorker.register('./sw.js').catch(() => undefined);
  });
}

function isStandalone(): boolean {
  return window.matchMedia('(display-mode: standalone)').matches
    || (navigator as Navigator & { standalone?: boolean }).standalone === true;
}

export interface InstallState {
  /** 이미 앱으로 실행 중 */
  installed: boolean;
  /** 브라우저가 바로 설치 창을 띄울 수 있음 */
  canPrompt: boolean;
  /** 설치 창 띄우기. 띄울 수 없으면 false */
  promptInstall: () => Promise<boolean>;
}

export function useInstall(): InstallState {
  const [, setTick] = useState(0);
  useEffect(() => {
    const fn = () => setTick((t) => t + 1);
    listeners.add(fn);
    return () => {
      listeners.delete(fn);
    };
  }, []);
  return {
    installed: isStandalone(),
    canPrompt: deferred !== null,
    promptInstall: async () => {
      if (!deferred) return false;
      const ev = deferred;
      await ev.prompt();
      await ev.userChoice.catch(() => undefined);
      deferred = null;
      notify();
      return true;
    },
  };
}
