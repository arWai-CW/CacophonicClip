import { describe, expect, test } from 'bun:test';
import { effectiveTrimDb, readClipActive } from './uiState';

describe('effectiveTrimDb', () => {
	test('keeps the manual output trim when auto-gain is off', () => {
		expect(effectiveTrimDb(-7.5, 12, false)).toBe(-7.5);
	});

	test('mirrors Drive inversely while auto-gain is on', () => {
		expect(effectiveTrimDb(-3, 12, true)).toBe(-12);
		expect(effectiveTrimDb(0, 24, true)).toBe(-24);
	});
});

describe('readClipActive', () => {
	test('ignores clipping retained in the five-second waveform history', () => {
		expect(
			readClipActive({
				drivenPeak: 0.2,
				drivenWaveformMin: [-1, -3],
				drivenWaveformMax: [1, 3]
			})
		).toBe(false);
	});

	test('lights only while the current driven block exceeds the clip ceiling', () => {
		expect(readClipActive({ drivenPeak: 1 })).toBe(false);
		expect(readClipActive({ drivenPeak: 0.9999 })).toBe(false);
		expect(readClipActive({ drivenPeak: 1.0001 })).toBe(true);
		expect(readClipActive({ drivenPeak: 4 })).toBe(true);
	});
});
