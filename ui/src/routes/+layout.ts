// The plugin UI is a client-only SPA: the production build ships as the
// adapter-static fallback shell (index.html) and renders entirely in the
// WebView, where `@juce-framework/webview` reads `window.__JUCE__` at module
// load. Without this flag the dev server tries to SSR the page and dies with
// "window is not defined" before anything reaches the browser.
export const ssr = false;
