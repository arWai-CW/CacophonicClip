/*
	Emphasis mode EQ voicing for the EQ view.

	Every design formula below is a line-for-line port of juce::dsp::ArrayCoefficients
	(JUCE/modules/juce_dsp/processors/juce_IIRFilter.cpp): makeLowPass, makeHighPass,
	makeLowShelf, makeHighShelf and makePeakFilter. Coefficients keep JUCE's returned
	order { b0, b1, b2, a0, a1, a2 }. Do not rewrite these as textbook RBJ biquads:
	the engine runs the JUCE originals, and the UI has to draw the same poles.
*/

export interface Biquad {
	b0: number;
	b1: number;
	b2: number;
	a0: number;
	a1: number;
	a2: number;
}

/**
 * juce::dsp::ArrayCoefficients<float>::inverseRootTwo, written there as
 * 0.70710678118654752440. A double only keeps 16 digits, so this is that same
 * value written as the shortest literal that round-trips to it.
 */
const INVERSE_ROOT_TWO = 0.7071067811865476;

/** juce::dsp::IIR::minimumDecibels, the floor of Decibels::gainWithLowerBound(). */
const MINIMUM_GAIN = Math.pow(10, -300 / 20);

function gainWithLowerBound(gainFactor: number) {
	return Math.max(gainFactor, MINIMUM_GAIN);
}

export function makeLowPass(sampleRate: number, frequency: number, Q = INVERSE_ROOT_TWO): Biquad {
	const n = 1 / Math.tan((Math.PI * frequency) / sampleRate);
	const nSquared = n * n;
	const invQ = 1 / Q;
	const c1 = 1 / (1 + invQ * n + nSquared);

	return {
		b0: c1,
		b1: c1 * 2,
		b2: c1,
		a0: 1,
		a1: c1 * 2 * (1 - nSquared),
		a2: c1 * (1 - invQ * n + nSquared)
	};
}

export function makeHighPass(sampleRate: number, frequency: number, Q = INVERSE_ROOT_TWO): Biquad {
	const n = Math.tan((Math.PI * frequency) / sampleRate);
	const nSquared = n * n;
	const invQ = 1 / Q;
	const c1 = 1 / (1 + invQ * n + nSquared);

	return {
		b0: c1,
		b1: c1 * -2,
		b2: c1,
		a0: 1,
		a1: c1 * 2 * (nSquared - 1),
		a2: c1 * (1 - invQ * n + nSquared)
	};
}

export function makeLowShelf(
	sampleRate: number,
	cutOffFrequency: number,
	Q: number,
	gainFactor: number
): Biquad {
	const A = Math.sqrt(gainWithLowerBound(gainFactor));
	const aminus1 = A - 1;
	const aplus1 = A + 1;
	const omega = (2 * Math.PI * Math.max(cutOffFrequency, 2)) / sampleRate;
	const coso = Math.cos(omega);
	const beta = (Math.sin(omega) * Math.sqrt(A)) / Q;
	const aminus1TimesCoso = aminus1 * coso;

	return {
		b0: A * (aplus1 - aminus1TimesCoso + beta),
		b1: A * 2 * (aminus1 - aplus1 * coso),
		b2: A * (aplus1 - aminus1TimesCoso - beta),
		a0: aplus1 + aminus1TimesCoso + beta,
		a1: -2 * (aminus1 + aplus1 * coso),
		a2: aplus1 + aminus1TimesCoso - beta
	};
}

export function makeHighShelf(
	sampleRate: number,
	cutOffFrequency: number,
	Q: number,
	gainFactor: number
): Biquad {
	const A = Math.sqrt(gainWithLowerBound(gainFactor));
	const aminus1 = A - 1;
	const aplus1 = A + 1;
	const omega = (2 * Math.PI * Math.max(cutOffFrequency, 2)) / sampleRate;
	const coso = Math.cos(omega);
	const beta = (Math.sin(omega) * Math.sqrt(A)) / Q;
	const aminus1TimesCoso = aminus1 * coso;

	return {
		b0: A * (aplus1 + aminus1TimesCoso + beta),
		b1: A * -2 * (aminus1 + aplus1 * coso),
		b2: A * (aplus1 + aminus1TimesCoso - beta),
		a0: aplus1 - aminus1TimesCoso + beta,
		a1: 2 * (aminus1 - aplus1 * coso),
		a2: aplus1 - aminus1TimesCoso - beta
	};
}

export function makePeakFilter(
	sampleRate: number,
	frequency: number,
	Q: number,
	gainFactor: number
): Biquad {
	const A = Math.sqrt(gainWithLowerBound(gainFactor));
	const omega = (2 * Math.PI * Math.max(frequency, 2)) / sampleRate;
	const alpha = Math.sin(omega) / (Q * 2);
	const c2 = -2 * Math.cos(omega);
	const alphaTimesA = alpha * A;
	const alphaOverA = alpha / A;

	return {
		b0: 1 + alphaTimesA,
		b1: c2,
		b2: 1 - alphaTimesA,
		a0: 1 + alphaOverA,
		a1: c2,
		a2: 1 - alphaOverA
	};
}

/**
 * Exact inverse of a pre stage, spec section 12.3: swap numerator and
 * denominator, then normalise the new denominator to a0 = 1.
 */
export function invertBiquad({ b0, b1, b2, a0, a1, a2 }: Biquad): Biquad {
	return { b0: a0 / b0, b1: a1 / b0, b2: a2 / b0, a0: 1, a1: b1 / b0, a2: b2 / b0 };
}

/** |H(e^jw)| in dB for one biquad, coefficients in JUCE order. */
export function magnitudeDb(coefficients: Biquad, omega: number): number {
	const { b0, b1, b2, a0, a1, a2 } = coefficients;
	const twiceOmega = omega * 2;
	const numRe = b0 + b1 * Math.cos(omega) + b2 * Math.cos(twiceOmega);
	const numIm = -(b1 * Math.sin(omega) + b2 * Math.sin(twiceOmega));
	const denRe = a0 + a1 * Math.cos(omega) + a2 * Math.cos(twiceOmega);
	const denIm = -(a1 * Math.sin(omega) + a2 * Math.sin(twiceOmega));
	return 20 * Math.log10(Math.hypot(numRe, numIm) / Math.hypot(denRe, denIm));
}

/** The UI cannot see the host sample rate, so the traces use 48 kHz (spec 12.6). */
export const FS_REF = 48000;

export type EmphasisMode = 'OFF' | 'TAPE' | 'TUBE';

export const EMPHASIS_MODES: readonly EmphasisMode[] = ['OFF', 'TAPE', 'TUBE'];

// Mode constants, spec section 12.2: midpoints of the agreed ranges, never retuned.
const TAPE_HP_HZ = 25;
const TAPE_BUMP_HZ = 75;
const TAPE_BUMP_DB = 2.25;
const TAPE_SHELF_MIN_HZ = 2500;
const TAPE_SHELF_SPAN_HZ = 1000;
const TAPE_MAX_DB = 8;

const TUBE_HP_HZ = 115;
const TUBE_LP_HZ = 14000;
const TUBE_LOW_SHELF_HZ = 100;
const TUBE_LOW_SHELF_DB = 3;
const TUBE_BELL_MIN_HZ = 1500;
const TUBE_BELL_SPAN_HZ = 1500;
const TUBE_BELL_Q_MIN = 0.7;
const TUBE_BELL_Q_SPAN = 0.3;
const TUBE_MAX_DB = 6;

/** Below this, the shelf/bell pair on both sides is skipped entirely. */
const SKIPPING_THRESHOLD_DB = 0.01;

export interface EmphasisChain {
	pre: Biquad[];
	post: Biquad[];
}

function decibelsToGain(decibels: number) {
	return Math.pow(10, decibels / 20);
}

/**
 * Pre and post stage lists for one mode, spec section 12.2. `p` is the emphasis
 * value in 0..1; HP, LP and low shelf stay voiced even at `p = 0`.
 */
export function emphasisChain(mode: EmphasisMode, p: number): EmphasisChain {
	if (mode === 'OFF') return { pre: [], post: [] };

	const pre: Biquad[] = [];
	const post: Biquad[] = [];

	if (mode === 'TAPE') {
		pre.push(makeHighPass(FS_REF, TAPE_HP_HZ));
		const gainDb = p * TAPE_MAX_DB;
		if (gainDb >= SKIPPING_THRESHOLD_DB) {
			const shelf = makeHighShelf(
				FS_REF,
				TAPE_SHELF_MIN_HZ + TAPE_SHELF_SPAN_HZ * p,
				INVERSE_ROOT_TWO,
				decibelsToGain(gainDb)
			);
			pre.push(shelf);
			post.push(invertBiquad(shelf));
		}
		post.push(makeLowShelf(FS_REF, TAPE_BUMP_HZ, INVERSE_ROOT_TWO, decibelsToGain(TAPE_BUMP_DB)));
		return { pre, post };
	}

	pre.push(makeHighPass(FS_REF, TUBE_HP_HZ));
	const gainDb = p * TUBE_MAX_DB;
	if (gainDb >= SKIPPING_THRESHOLD_DB) {
		const bell = makePeakFilter(
			FS_REF,
			TUBE_BELL_MIN_HZ + TUBE_BELL_SPAN_HZ * p,
			TUBE_BELL_Q_MIN + TUBE_BELL_Q_SPAN * p,
			decibelsToGain(gainDb)
		);
		pre.push(bell);
		post.push(invertBiquad(bell));
	}
	post.push(makeLowPass(FS_REF, TUBE_LP_HZ));
	post.push(
		makeLowShelf(FS_REF, TUBE_LOW_SHELF_HZ, INVERSE_ROOT_TWO, decibelsToGain(TUBE_LOW_SHELF_DB))
	);
	return { pre, post };
}

/** Summed dB response of a stage list at one frequency (order is irrelevant). */
export function chainDb(stages: Biquad[], frequency: number): number {
	const omega = (2 * Math.PI * frequency) / FS_REF;
	let total = 0;
	for (const stage of stages) total += magnitudeDb(stage, omega);
	return total;
}

/** Shelf/bell frequency and gain shown in the micro strip caption, spec 12.6. */
export function emphasisCaption(mode: EmphasisMode, p: number): string {
	if (mode === 'OFF') return 'EMPH OFF';
	if (mode === 'TAPE') {
		return `TAPE ${formatHz(TAPE_SHELF_MIN_HZ + TAPE_SHELF_SPAN_HZ * p)} +${(p * TAPE_MAX_DB).toFixed(1)} dB`;
	}
	return `TUBE ${formatHz(TUBE_BELL_MIN_HZ + TUBE_BELL_SPAN_HZ * p)} +${(p * TUBE_MAX_DB).toFixed(1)} dB`;
}

function formatHz(frequency: number) {
	return frequency >= 1000 ? `${(frequency / 1000).toFixed(1)} kHz` : `${frequency.toFixed(0)} Hz`;
}
