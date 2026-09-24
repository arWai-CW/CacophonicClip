import tailwindcss from '@tailwindcss/vite';
import { sveltekit } from '@sveltejs/kit/vite';
import { defineConfig } from 'vite';

export default defineConfig({
	plugins: [tailwindcss(), sveltekit()],
	build: {
		outDir: '../Source/ui_dist', // 打包到 C++ 源碼目錄旁
		emptyOutDir: true
	}
});
