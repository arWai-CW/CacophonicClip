import adapter from '@sveltejs/adapter-static';

/** @type {import('@sveltejs/kit').Config} */
const config = {
	kit: {
		adapter: adapter({
			pages: '../Source/ui_dist',
			assets: '../Source/ui_dist',
			fallback: 'index.html'
		})
	}
};

export default config;
