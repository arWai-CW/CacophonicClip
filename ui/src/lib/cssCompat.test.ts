/**
 * Gate for the WebKit baseline: the plug-in supports macOS 11.1, whose WebView
 * is WebKit 14 (Safari 14.1). Tailwind v4 has no browser-target setting of its
 * own - it dropped browserslist and autoprefixer in v4 - so nothing in the build
 * stops a modern-only CSS feature from reaching the embedded UI. This test is
 * that stop, and it reads the built stylesheet rather than the sources on
 * purpose: what matters is what lands in Source/ui_dist, not what was typed.
 *
 * Run order: this needs `bun run build` to have produced Source/ui_dist first.
 */
import { describe, expect, test } from 'bun:test';
import { readdirSync, readFileSync, statSync } from 'node:fs';
import { join } from 'node:path';

const distDir = join(import.meta.dir, '..', '..', '..', 'Source', 'ui_dist');

/**
 * At-rules are Safari 16.0, colour functions 15.0 to 16.4, and so on. Each entry
 * pairs the pattern with the version that added it, because a list of
 * "unsupported" features with no versions behind them is folklore, not a gate.
 *
 * The patterns are regexes rather than substrings for two reasons: `lab(` would
 * otherwise match inside `oklab(`, and `@container` would match Tailwind's own
 * `.\@container` utility class, which is a class name and not an at-rule.
 */
const unsupported: Array<[pattern: RegExp, since: string]> = [
	[/(?<!\\)\B@container\s*[({]/g, '16.0'],
	[/(?<!\\)\B@scope\s*[({]/g, '17.4'],
	[/\B:has\(/g, '15.4'],
	[/\btext-wrap\s*:/g, '17.5'],
	[/(?<![\w-])(?:oklch|oklab|lab|lch|hwb)\(/g, '15.0 to 16.4'],
	[/\bcolor-mix\(/g, '16.2'],
	[/(?<![\w-])(?:dvh|svh|lvh)\b/g, '15.4'],
	[/\bcontent-visibility\s*:/g, '18.0'],
	[/\bscrollbar-gutter\s*:/g, '18.2'],
	[/\baccent-color\s*:/g, '15.4'],
	[/\boverflow\s*:\s*clip\b/g, '16.0']
];

/** Index just past the `}` that closes the block opening at `open`. */
function blockEnd(css: string, open: number): number {
	let depth = 0;
	let quote = '';
	for (let i = open; i < css.length; i++) {
		const c = css[i];
		// A quoted string can hold a brace (a data: URI does), and counting it
		// would end the walk early.
		if (quote !== '') {
			if (c === '\\') i++;
			else if (c === quote) quote = '';
			continue;
		}
		if (c === '"' || c === "'") quote = c;
		else if (c === '{') depth++;
		else if (c === '}' && --depth === 0) return i + 1;
	}
	return css.length;
}

/** True when `@name` at `at` is an at-rule and not part of a class or id. */
function isAtRule(css: string, at: number): boolean {
	const prev = at > 0 ? css[at - 1] : '';
	return prev !== '.' && prev !== '\\' && !/[\w-]/.test(prev);
}

/**
 * Delete every `@name` block and statement. Anything inside a feature query is
 * by definition guarded - a browser that cannot parse the condition drops the
 * block - so stripping those first is what makes the blocklist below exact
 * rather than a heuristic. Tailwind leans on this: its preflight ships
 * `::placeholder { color: currentColor }` with the `color-mix()` value behind
 * `@supports (color: color-mix(in lab, red, red))`.
 */
function stripAtRule(css: string, name: string): string {
	const token = '@' + name;
	let out = '';
	let i = 0;
	for (;;) {
		const at = css.indexOf(token, i);
		if (at < 0) return out + css.slice(i);
		if (!isAtRule(css, at)) {
			out += css.slice(i, at + 1);
			i = at + 1;
			continue;
		}
		out += css.slice(i, at);
		const brace = css.indexOf('{', at);
		const semi = css.indexOf(';', at);
		if (brace < 0 || (semi >= 0 && semi < brace)) {
			i = semi < 0 ? css.length : semi + 1;
		} else {
			i = blockEnd(css, brace);
		}
		out += ' ';
	}
}

function stylesheets(dir: string): string[] {
	const found: string[] = [];
	for (const entry of readdirSync(dir)) {
		const path = join(dir, entry);
		if (statSync(path).isDirectory()) found.push(...stylesheets(path));
		else if (path.endsWith('.css')) found.push(path);
	}
	return found;
}

const sheets = stylesheets(distDir);
const built = sheets.map((sheet) => ({ sheet, css: readFileSync(sheet, 'utf8') }));

describe('built CSS stays inside the WebKit 14 baseline', () => {
	test('the build produced stylesheets to check', () => {
		expect(sheets.length).toBeGreaterThan(0);
	});

	test('no CSS feature newer than WebKit 14 outside a feature query', () => {
		const failures: string[] = [];
		for (const { sheet, css } of built) {
			const scannable = stripAtRule(css, 'supports');
			for (const [pattern, since] of unsupported) {
				for (const hit of scannable.matchAll(pattern)) {
					const line = scannable.slice(0, hit.index).split('\n').length;
					failures.push(`${sheet} line ${line}: needs Safari ${since}`);
				}
			}
		}
		expect(failures).toEqual([]);
	});

	test('no @layer, except the one Tailwind emits on its own', () => {
		// A browser that does not understand an at-rule discards the block and
		// everything inside it, so one stray @layer can delete rules wholesale
		// with no error anywhere. layout.css therefore imports Tailwind's
		// theme/preflight/utilities without layer() instead of importing
		// 'tailwindcss'. See the comment at the top of layout.css.
		//
		// The one exception is @layer properties, which Tailwind synthesises
		// for the defaults of its @property registrations. It is not
		// configurable, and it only ever holds --tw-rotate-* / --tw-skew-*
		// resets that this design never reads (no 3D transforms), so letting it
		// go dark on macOS 11 costs nothing.
		const failures: string[] = [];
		for (const { sheet, css } of built) {
			for (const match of css.matchAll(/@layer\s+([a-z0-9, -]*?)\s*\{/g)) {
				if (match[1] === 'properties') continue;
				failures.push(`${sheet}: @layer ${match[1]}`);
			}
		}
		expect(failures).toEqual([]);
	});

	test('@layer properties only carries transform defaults', () => {
		// Pairs with the exception above: rather than trusting that Tailwind
		// keeps this block harmless, assert what is inside it, so a future
		// Tailwind that puts something load-bearing there fails here instead of
		// in someone's face on a Big Sur machine. Every custom property in the
		// block is checked, not just the --tw-* ones: filtering to a prefix
		// would let anything else ride along unnoticed, which is the exact hole
		// this test exists to close.
		const declared = new Set<string>();
		for (const { css } of built) {
			for (const match of css.matchAll(/@layer\s+properties\s*\{/g)) {
				const open = match.index + match[0].length - 1;
				const body = css.slice(open, blockEnd(css, open));
				for (const hit of body.matchAll(/(?:^|[{;\s])(--[\w-]+)\s*:/g)) declared.add(hit[1]);
			}
		}
		expect(declared.size).toBeGreaterThan(0);
		for (const name of declared) {
			expect(name).toMatch(/^--tw-(?:rotate|skew)-[xyz]$/);
		}
	});
});
