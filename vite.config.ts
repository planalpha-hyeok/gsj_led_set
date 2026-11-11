import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';
import { viteSingleFile } from 'vite-plugin-singlefile';

// 빌드 결과를 HTML 파일 하나로 만든다 (인터넷 없이 파일을 더블클릭해서 실행 가능)
export default defineConfig({
  plugins: [react(), viteSingleFile()],
  base: './',
});
