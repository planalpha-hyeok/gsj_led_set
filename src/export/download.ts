/** 파일 저장 (브라우저 다운로드) */
export function downloadBlob(blob: Blob, filename: string): void {
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = filename;
  document.body.appendChild(a);
  a.click();
  a.remove();
  setTimeout(() => URL.revokeObjectURL(url), 1000);
}

export function downloadDataUrl(dataUrl: string, filename: string): void {
  const a = document.createElement('a');
  a.href = dataUrl;
  a.download = filename;
  document.body.appendChild(a);
  a.click();
  a.remove();
}

/** 파일 이름에 쓸 수 없는 문자 제거 */
export function safeName(name: string): string {
  return (name.trim() || 'project').replace(/[\\/:*?"<>|]+/g, '_');
}
