import tailwindcss from '@tailwindcss/vite';
import { sveltekit } from '@sveltejs/kit/vite';
import { defineConfig } from 'vite';

export default defineConfig({
	plugins: [tailwindcss(), sveltekit()],
	// Source/PluginEditor.cpp probes this exact address to decide whether the
	// WebView should load the UI from here (hot reload) or from the embedded
	// zip, so the port may never drift and the host has to be the IPv4
	// loopback that the probe connects to.
	server: {
		host: '127.0.0.1',
		port: 5173,
		strictPort: true
	},
	build: {
		outDir: '../Source/ui_dist', // 打包到 C++ 源碼目錄旁
		emptyOutDir: true
	}
});
