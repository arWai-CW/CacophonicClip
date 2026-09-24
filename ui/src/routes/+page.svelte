<script lang="ts">
	import { getComboBoxState, getSliderState, getToggleState } from '@juce-framework/webview';
	import { EMPHASIS_MODES, chainDb, emphasisCaption, emphasisChain } from '$lib/emphasisEq';
	import type { Biquad } from '$lib/emphasisEq';

	type KnobName = 'trim' | 'drive' | 'mix' | 'shape' | 'emphasis' | 'asym';
	interface Column {
		kind: 'knob' | 'switch';
		name: KnobName | null;
		label: string;
		sub: string;
		width: number;
		size: number;
		degraded?: boolean;
	}

	const trimState = getSliderState('trim');
	const driveState = getSliderState('drive');
	const mixState = getSliderState('mix');
	const shapeState = getSliderState('shape');
	const emphasisState = getSliderState('emphasis');
	const asymState = getSliderState('asym');
	const boostState = getToggleState('boost2x');

	// The bypass relay is added on the C++ side in a parallel change. Until the
	// engine registers it, degrade to a disabled switch instead of throwing and
	// blanking the whole page.
	function resolveBypassState(): ReturnType<typeof getToggleState> | null {
		if (typeof window === 'undefined') return null;
		try {
			const registered: string[] = window.__JUCE__?.initialisationData?.__juce__toggles ?? [];
			if (!registered.includes('bypass')) return null;
			return getToggleState('bypass');
		} catch {
			return null;
		}
	}
	const bypassState = resolveBypassState();

	// Same guard for the four-position oversampling relay: if the engine has not
	// registered `oversampling` yet, hand back null and render the group disabled
	// instead of warning and desynchronising.
	function resolveOversamplingState(): ReturnType<typeof getComboBoxState> | null {
		if (typeof window === 'undefined') return null;
		try {
			const registered: string[] = window.__JUCE__?.initialisationData?.__juce__comboBoxes ?? [];
			if (!registered.includes('oversampling')) return null;
			return getComboBoxState('oversampling');
		} catch {
			return null;
		}
	}
	const oversamplingState = resolveOversamplingState();

	// Same guard again for the three-position emphasis mode relay: unregistered
	// means null, and the MODE group renders disabled until the engine lands.
	function resolveEmphasisModeState(): ReturnType<typeof getComboBoxState> | null {
		if (typeof window === 'undefined') return null;
		try {
			const registered: string[] = window.__JUCE__?.initialisationData?.__juce__comboBoxes ?? [];
			if (!registered.includes('emphasisMode')) return null;
			return getComboBoxState('emphasisMode');
		} catch {
			return null;
		}
	}
	const emphasisModeState = resolveEmphasisModeState();

	const columns: Column[] = [
		{ kind: 'knob', name: 'drive', label: 'DRIVE', sub: 'CLIPPING', width: 180, size: 112 },
		{ kind: 'switch', name: null, label: '2x', sub: 'INPUT GAIN', width: 156, size: 0 },
		{ kind: 'knob', name: 'mix', label: 'MIX', sub: 'DRY / WET', width: 144, size: 92 },
		{ kind: 'knob', name: 'shape', label: 'SHAPE', sub: 'SOFT / HARD', width: 144, size: 92 },
		{
			kind: 'knob',
			name: 'emphasis',
			label: 'EMPHASIS',
			sub: 'PRE-EQ',
			width: 144,
			size: 92
		},
		{
			kind: 'knob',
			name: 'asym',
			label: 'ASYMMETRY',
			sub: 'EVEN / ODD',
			width: 144,
			size: 92
		},
		{
			kind: 'knob',
			name: 'trim',
			label: 'TRIM',
			sub: 'OUTPUT',
			width: 144,
			size: 92,
			degraded: true
		}
	];

	const meterSegments = Array.from({ length: 24 }, (_, index) => index);
	const knobTicks = Array.from({ length: 9 }, (_, index) => -135 + index * 33.75);
	const oversamplingChoices = ['1x', '2x', '4x', '8x'];
	// Engine default for `oversampling` is index 2 (4x), so the degraded state
	// still reads as a plausible setting rather than an arbitrary highlight.
	const oversamplingFallbackIndex = 2;
	// Engine default for `emphasisMode` is index 0 (OFF), spec section 12.1.
	const emphasisModeFallbackIndex = 0;

	function readOversamplingIndex(state: ReturnType<typeof getComboBoxState>): number {
		try {
			// Until the relay has pushed its `choices`, getChoiceIndex() would
			// divide by an empty array; fall back to the engine default (4x).
			return state.properties.choices.length === 0
				? oversamplingFallbackIndex
				: state.getChoiceIndex();
		} catch {
			return oversamplingFallbackIndex;
		}
	}

	function readEmphasisModeIndex(state: ReturnType<typeof getComboBoxState>): number {
		try {
			// Same empty `choices` guard as oversampling: without it
			// getChoiceIndex() would round against a zero-length array.
			return state.properties.choices.length === 0
				? emphasisModeFallbackIndex
				: state.getChoiceIndex();
		} catch {
			return emphasisModeFallbackIndex;
		}
	}

	let trimValue = $state(trimState.getScaledValue());
	let driveValue = $state(driveState.getScaledValue());
	let mixValue = $state(mixState.getScaledValue());
	let shapeValue = $state(shapeState.getScaledValue());
	let emphasisValue = $state(emphasisState.getScaledValue());
	let asymValue = $state(asymState.getScaledValue());
	let boostValue = $state(boostState.getValue());
	let oversamplingIndex = $state(
		oversamplingState ? readOversamplingIndex(oversamplingState) : oversamplingFallbackIndex
	);
	let emphasisModeIndex = $state(
		emphasisModeState ? readEmphasisModeIndex(emphasisModeState) : emphasisModeFallbackIndex
	);
	let view = $state<'wave' | 'eq'>('wave');
	let bypassValue = $state(bypassState ? bypassState.getValue() : false);
	let meterLeft = $state(0);
	let meterRight = $state(0);
	let overActive = $state(false);
	let inputWaveformMin = $state<number[]>([]);
	let inputWaveformMax = $state<number[]>([]);
	let drivenWaveformMin = $state<number[]>([]);
	let drivenWaveformMax = $state<number[]>([]);
	let outputWaveformMin = $state<number[]>([]);
	let outputWaveformMax = $state<number[]>([]);
	let activeKnob = $state<KnobName | null>(null);
	let canvasScale = $state(1);
	let dragStartY = 0;
	let dragStartValue = 0;
	let lastMeterAt = 0;
	let overFrames = 0;
	let overReleaseTimer: ReturnType<typeof setTimeout> | null = null;

	const reducedMotionQuery =
		typeof window !== 'undefined' ? window.matchMedia('(prefers-reduced-motion: reduce)') : null;

	$effect(() => {
		updateScale();
		const trimListener = trimState.valueChangedEvent.addListener(
			() => (trimValue = trimState.getScaledValue())
		);
		const driveListener = driveState.valueChangedEvent.addListener(
			() => (driveValue = driveState.getScaledValue())
		);
		const mixListener = mixState.valueChangedEvent.addListener(
			() => (mixValue = mixState.getScaledValue())
		);
		const shapeListener = shapeState.valueChangedEvent.addListener(
			() => (shapeValue = shapeState.getScaledValue())
		);
		const emphasisListener = emphasisState.valueChangedEvent.addListener(
			() => (emphasisValue = emphasisState.getScaledValue())
		);
		const asymListener = asymState.valueChangedEvent.addListener(
			() => (asymValue = asymState.getScaledValue())
		);
		const boostListener = boostState.valueChangedEvent.addListener(
			() => (boostValue = boostState.getValue())
		);
		const bypassListener = bypassState?.valueChangedEvent.addListener(
			() => (bypassValue = bypassState ? bypassState.getValue() : false)
		);
		const oversamplingListener = oversamplingState?.valueChangedEvent.addListener(
			() =>
				(oversamplingIndex = oversamplingState
					? readOversamplingIndex(oversamplingState)
					: oversamplingFallbackIndex)
		);
		const oversamplingPropertiesListener = oversamplingState?.propertiesChangedEvent.addListener(
			() =>
				(oversamplingIndex = oversamplingState
					? readOversamplingIndex(oversamplingState)
					: oversamplingFallbackIndex)
		);
		const emphasisModeListener = emphasisModeState?.valueChangedEvent.addListener(
			() =>
				(emphasisModeIndex = emphasisModeState
					? readEmphasisModeIndex(emphasisModeState)
					: emphasisModeFallbackIndex)
		);
		const emphasisModePropertiesListener = emphasisModeState?.propertiesChangedEvent.addListener(
			() =>
				(emphasisModeIndex = emphasisModeState
					? readEmphasisModeIndex(emphasisModeState)
					: emphasisModeFallbackIndex)
		);
		const meterListener = window.__JUCE__.backend.addEventListener('meterData', (payload) => {
			const data = payload as {
				outputLeft: number;
				outputRight: number;
				inputWaveformMin: number[];
				inputWaveformMax: number[];
				drivenWaveformMin: number[];
				drivenWaveformMax: number[];
				outputWaveformMin: number[];
				outputWaveformMax: number[];
			};
			const now = performance.now();
			const dt = lastMeterAt === 0 ? 16 : Math.min(250, now - lastMeterAt);
			lastMeterAt = now;
			meterLeft = ballistics(meterLeft, data.outputLeft ?? 0, dt);
			meterRight = ballistics(meterRight, data.outputRight ?? 0, dt);
			inputWaveformMin = data.inputWaveformMin ?? [];
			inputWaveformMax = data.inputWaveformMax ?? [];
			drivenWaveformMin = data.drivenWaveformMin ?? [];
			drivenWaveformMax = data.drivenWaveformMax ?? [];
			outputWaveformMin = data.outputWaveformMin ?? [];
			outputWaveformMax = data.outputWaveformMax ?? [];
			updateOver(peakOf(drivenWaveformMin, drivenWaveformMax));
		});
		return () => {
			window.__JUCE__.backend.removeEventListener(meterListener);
			trimState.valueChangedEvent.removeListener(trimListener);
			driveState.valueChangedEvent.removeListener(driveListener);
			mixState.valueChangedEvent.removeListener(mixListener);
			shapeState.valueChangedEvent.removeListener(shapeListener);
			emphasisState.valueChangedEvent.removeListener(emphasisListener);
			asymState.valueChangedEvent.removeListener(asymListener);
			boostState.valueChangedEvent.removeListener(boostListener);
			if (bypassListener !== undefined)
				bypassState?.valueChangedEvent.removeListener(bypassListener);
			if (oversamplingListener !== undefined)
				oversamplingState?.valueChangedEvent.removeListener(oversamplingListener);
			if (oversamplingPropertiesListener !== undefined)
				oversamplingState?.propertiesChangedEvent.removeListener(oversamplingPropertiesListener);
			if (emphasisModeListener !== undefined)
				emphasisModeState?.valueChangedEvent.removeListener(emphasisModeListener);
			if (emphasisModePropertiesListener !== undefined)
				emphasisModeState?.propertiesChangedEvent.removeListener(emphasisModePropertiesListener);
		};
	});

	function updateScale() {
		const k = Math.min(window.innerWidth / 1120, window.innerHeight / 560);
		canvasScale = Math.min(1.5, Math.max(0.8, k));
	}

	function ballistics(previous: number, next: number, dt: number) {
		if (next >= previous) return next;
		if (reducedMotionQuery?.matches) return next;
		return next + (previous - next) * Math.exp(-dt / 300);
	}

	function peakOf(mins: number[], maxs: number[]) {
		let peak = 0;
		for (const value of maxs) peak = Math.max(peak, Math.abs(value));
		for (const value of mins) peak = Math.max(peak, Math.abs(value));
		return peak;
	}

	// Semantic: "being clipped right now": the driven signal crossed the ±1
	// clip ceiling. Comparing driven against output no longer works, since
	// output is now post-Trim and sits below the ceiling whenever Trim < 0 dB.
	// Three frames to light, a lower threshold to release, 200ms release tail.
	function updateOver(drivenPeak: number) {
		const cutting = drivenPeak > 1.002;
		const released = drivenPeak <= 1.0;
		if (cutting) {
			overFrames += 1;
			if (overFrames >= 3 && !overActive) {
				if (overReleaseTimer !== null) {
					clearTimeout(overReleaseTimer);
					overReleaseTimer = null;
				}
				overActive = true;
			}
			return;
		}
		overFrames = 0;
		if (overActive && released && overReleaseTimer === null) {
			overReleaseTimer = setTimeout(() => {
				overActive = false;
				overReleaseTimer = null;
			}, 200);
		}
	}

	function getState(name: KnobName) {
		if (name === 'trim') return trimState;
		if (name === 'drive') return driveState;
		if (name === 'shape') return shapeState;
		if (name === 'emphasis') return emphasisState;
		if (name === 'asym') return asymState;
		return mixState;
	}
	function getValue(name: KnobName) {
		if (name === 'trim') return trimValue;
		if (name === 'drive') return driveValue;
		if (name === 'shape') return shapeValue;
		if (name === 'emphasis') return emphasisValue;
		if (name === 'asym') return asymValue;
		return mixValue;
	}
	function getNormalised(name: KnobName) {
		return getState(name).getNormalisedValue();
	}
	function tickLine(angle: number) {
		const radians = (angle * Math.PI) / 180;
		return {
			x1: 50 + Math.sin(radians) * 47.5,
			y1: 50 - Math.cos(radians) * 47.5,
			x2: 50 + Math.sin(radians) * 50,
			y2: 50 - Math.cos(radians) * 50
		};
	}
	// The band is 65px tall and ±1 (the clip ceiling) is what the red overshoot
	// has to sit above, so ±1 only reaches partway out. Without this headroom
	// the red band collapses to a 3px sliver welded to the border.
	const WAVE_AMP = 21;
	function makeWaveformPath(mins: number[], maxs: number[]) {
		// DAW-style min/max envelope: draw the top edge forward and the
		// bottom edge backward to create a closed filled waveform shape.
		if (maxs.length === 0) return '';
		const toX = (index: number) => (index / Math.max(1, maxs.length - 1)) * 980;
		const toY = (value: number) => 32.5 - Math.max(-1, Math.min(1, value)) * WAVE_AMP;
		const top = maxs.map((value, index) => `${toX(index)},${toY(value)}`).join(' L ');
		const bottom = mins
			.map((value, index) => ({ index, value }))
			.reverse()
			.map(({ index, value }) => `${toX(index)},${toY(value)}`)
			.join(' L ');
		return `M ${top} L ${bottom} Z`;
	}
	function makeClippedPath(drivenMin: number[], drivenMax: number[]) {
		// Red overlay: the part of the driven signal (post 2x/Drive, pre-clip)
		// that went past the clip ceiling, measured against the ceiling itself.
		// Trim is an output control now, so the output envelope can no longer be
		// used as the inner boundary: at Trim < 0 dB it sits well below the
		// ceiling and the band would swallow half the waveform.
		if (drivenMax.length === 0) return '';
		const toX = (index: number) => (index / Math.max(1, drivenMax.length - 1)) * 980;
		const toY = (value: number) => 32.5 - Math.max(-1, Math.min(1, value)) * WAVE_AMP;
		// The driven signal can far exceed 1 (that IS the clipping); map it
		// logarithmically into the remaining headroom so the red band keeps its
		// shape instead of collapsing into a flat bar welded to the border.
		const toYOuter = (value: number) => {
			const headroom = 1.39;
			const magnitude = Math.abs(value);
			const compressed =
				magnitude <= 1 ? magnitude : 1 + (headroom - 1) * (1 - Math.exp(-(magnitude - 1)));
			return 32.5 - Math.sign(value) * compressed * WAVE_AMP;
		};
		const epsilon = 0.01;
		const parts: string[] = [];

		const buildRuns = (
			isClipped: (index: number) => boolean,
			outer: (index: number) => number,
			ceiling: number
		) => {
			let runStart = -1;
			for (let i = 0; i <= drivenMax.length; i++) {
				const clipped = i < drivenMax.length && isClipped(i);
				if (clipped && runStart < 0) runStart = i;
				if (!clipped && runStart >= 0) {
					const end = i - 1;
					const forward: string[] = [];
					const backward: string[] = [];
					for (let j = runStart; j <= end; j++) {
						forward.push(`${toX(j)},${toYOuter(outer(j))}`);
						backward.unshift(`${toX(j)},${toY(ceiling)}`);
					}
					parts.push(`M ${forward.join(' L ')} L ${backward.join(' L ')} Z`);
					runStart = -1;
				}
			}
		};

		// Top side: driven peak above the +1 ceiling.
		buildRuns(
			(i) => drivenMax[i] > 1 + epsilon,
			(i) => drivenMax[i],
			1
		);
		// Bottom side: driven trough below the -1 ceiling.
		buildRuns(
			(i) => drivenMin[i] < -1 - epsilon,
			(i) => drivenMin[i],
			-1
		);

		return parts.join(' ');
	}
	function resetKnob(event: MouseEvent, name: KnobName) {
		event.preventDefault();
		event.stopPropagation();
		const defaults: Record<KnobName, number> = {
			trim: 1,
			drive: 0,
			mix: 1,
			shape: 0,
			emphasis: 0,
			asym: 0
		};
		setValue(name, defaults[name]);
	}

	function setValue(name: KnobName, normalised: number) {
		getState(name).setNormalisedValue(Math.max(0, Math.min(1, normalised)));
		if (name === 'trim') trimValue = trimState.getScaledValue();
		if (name === 'drive') driveValue = driveState.getScaledValue();
		if (name === 'mix') mixValue = mixState.getScaledValue();
		if (name === 'shape') shapeValue = shapeState.getScaledValue();
		if (name === 'emphasis') emphasisValue = emphasisState.getScaledValue();
		if (name === 'asym') asymValue = asymState.getScaledValue();
	}
	function beginKnob(event: PointerEvent, name: KnobName) {
		event.preventDefault();
		activeKnob = name;
		dragStartY = event.clientY;
		dragStartValue = getNormalised(name);
		getState(name).sliderDragStarted();
		(event.currentTarget as HTMLElement).setPointerCapture(event.pointerId);
	}
	function moveKnob(event: PointerEvent) {
		if (activeKnob === null) return;
		const sensitivity = event.shiftKey ? 0.0015 : 0.005;
		setValue(activeKnob, dragStartValue + (dragStartY - event.clientY) * sensitivity);
	}
	function endKnob() {
		if (activeKnob !== null) getState(activeKnob).sliderDragEnded();
		activeKnob = null;
	}
	// Value arc geometry in the knob's 0..100 viewBox units. Angles are degrees
	// clockwise from 12 o'clock, matching tickLine() and the pointer rotation.
	//
	// This is an explicit A-command arc, not a dashed circle: with
	// vector-effect: non-scaling-stroke Chrome measures stroke-dasharray in
	// screen space, so the sweep came out divided by the viewBox scale factor
	// (Ø92 rendered 293deg, Ø112 rendered 241deg, both wrong by ~11%).
	function arcPath(radius: number, startDeg: number, endDeg: number) {
		const sweep = endDeg - startDeg;
		if (sweep <= 0.0001) return '';
		const toPoint = (deg: number) => {
			const rad = (deg * Math.PI) / 180;
			return [50 + radius * Math.sin(rad), 50 - radius * Math.cos(rad)];
		};
		const [x0, y0] = toPoint(startDeg);
		const [x1, y1] = toPoint(endDeg);
		const largeArc = Math.abs(sweep) > 180 ? 1 : 0;
		const direction = sweep > 0 ? 1 : 0;
		return `M ${x0.toFixed(3)} ${y0.toFixed(3)} A ${radius} ${radius} 0 ${largeArc} ${direction} ${x1.toFixed(3)} ${y1.toFixed(3)}`;
	}

	function getVisualNormalised(name: KnobName) {
		const value = getValue(name);
		if (name === 'trim') return (value + 12.0) / 12.0;
		if (name === 'drive') return value / 24.0;
		if (name === 'emphasis') return value / 100;
		return value;
	}

	function getVisualAngle(name: KnobName) {
		return -135 + getVisualNormalised(name) * 270;
	}

	function ariaRange(name: KnobName) {
		if (name === 'drive') return { min: 0, max: 24, now: driveValue };
		if (name === 'trim') return { min: -12, max: 0, now: trimValue };
		if (name === 'emphasis') return { min: 0, max: 100, now: emphasisValue };
		if (name === 'asym') return { min: 0, max: 100, now: asymValue * 100 };
		return { min: 0, max: 100, now: getValue(name) * 100 };
	}

	function litSegments(level: number) {
		const db = level > 0.00001 ? 20 * Math.log10(level) : -60;
		const normalised = Math.max(0, Math.min(1, (db + 60) / 60));
		return Math.round(normalised * meterSegments.length);
	}

	function formatDb(value: number) {
		return `${value.toFixed(1)} dB`;
	}
	function formatPercent(value: number) {
		return `${Math.round(value * 100)} %`;
	}
	// Must match StringFromValue in PluginProcessor.cpp exactly.
	function formatShape(value: number) {
		if (value <= 0.02) return 'SOFT';
		if (value >= 0.98) return 'HARD';
		return `${Math.round(value * 100)} %`;
	}
	function formatKnob(name: KnobName) {
		if (name === 'mix') return formatPercent(mixValue);
		if (name === 'shape') return formatShape(shapeValue);
		if (name === 'emphasis') return `${Math.round(emphasisValue)} %`;
		if (name === 'asym') return formatPercent(asymValue);
		return formatDb(getValue(name));
	}

	// Transfer curve, spec section 11.3 (supersedes 7.4): bias in, normalised
	// back out, trim last. No mix, no emphasis, no AC-coupling: this draws the
	// waveshaper's static transfer characteristic, and the DC blocker would
	// flatten a DC curve to zero.
	const curveWidth = 980;
	const curveHeight = 168;
	const curveYMax = 1.12;
	function toCurveX(input: number) {
		return ((input + 1) / 2) * curveWidth;
	}
	function toCurveY(output: number) {
		return ((curveYMax - output) / (2 * curveYMax)) * curveHeight;
	}
	function clipFn(value: number) {
		const soft = Math.tanh(value);
		const hard = Math.max(-1, Math.min(1, value));
		return soft + shapeValue * (hard - soft);
	}
	function curveOutput(input: number) {
		const bias = asymValue * 0.2;
		const t = clipFn(bias);
		const gain = Math.pow(10, driveValue / 20) * (boostValue ? 2 : 1);
		const clipped = (clipFn(input * gain + bias) - t) / (1 + t);
		return clipped * Math.pow(10, trimValue / 20);
	}
	const curvePath = $derived.by(() => {
		const points: string[] = [];
		for (let i = 0; i <= 256; i++) {
			const input = -1 + (i / 256) * 2;
			points.push(`${toCurveX(input).toFixed(2)},${toCurveY(curveOutput(input)).toFixed(2)}`);
		}
		return `M ${points.join(' L ')}`;
	});
	const ceilingY = $derived(toCurveY(Math.pow(10, trimValue / 20)));
	// Asymmetry only ever lowers the positive ceiling, so the negative ceiling
	// is the one that stays put: draw it whenever the bias is doing anything.
	const ceilingNegativeY = $derived(toCurveY(-Math.pow(10, trimValue / 20)));
	const ceilingLabel = $derived(`${trimValue.toFixed(1)} dBFS`);

	// EQ view, spec section 12.6 (supersedes 11.7): the mode's JUCE-designed
	// pre and post filters over the same log frequency axis, +/-12 dB.
	function eqX(frequency: number) {
		return ((Math.log10(frequency) - Math.log10(20)) / 3) * 980;
	}
	// The tube high pass sits near -25 dB at 20 Hz, well outside the +/-12 dB
	// window: clamp so no path point ever leaves the 0..65 viewBox.
	function eqY(db: number) {
		return Math.max(0, Math.min(65, 32.5 - db * (65 / 24)));
	}
	function makeEqPath(stages: Biquad[]) {
		const points: string[] = [];
		const steps = 128;
		for (let i = 0; i <= steps; i++) {
			const frequency = Math.pow(10, Math.log10(20) + (i / steps) * 3);
			points.push(`${eqX(frequency).toFixed(2)},${eqY(chainDb(stages, frequency)).toFixed(2)}`);
		}
		return `M ${points.join(' L ')}`;
	}
	const emphasisMode = $derived(EMPHASIS_MODES[emphasisModeIndex] ?? 'OFF');
	const emphasisRatio = $derived(emphasisValue / 100);
	const eqChains = $derived(emphasisChain(emphasisMode, emphasisRatio));
	const eqPrePath = $derived(makeEqPath(eqChains.pre));
	const eqPostPath = $derived(makeEqPath(eqChains.post));
	// Micro strip caption, spec 12.6: shelf/bell frequency and gain only.
	const eqCaption = $derived(emphasisCaption(emphasisMode, emphasisRatio));
	const eqGraticule = `M ${eqX(100).toFixed(2)} 0 V 65 M ${eqX(1000).toFixed(2)} 0 V 65 M ${eqX(
		10000
	).toFixed(2)} 0 V 65 M 0 32.5 H 980`;

	const inputWaveformPath = $derived(makeWaveformPath(inputWaveformMin, inputWaveformMax));
	const outputWaveformPath = $derived(makeWaveformPath(outputWaveformMin, outputWaveformMax));
	// Red = driven (recorded in the audio thread with the parameters that were
	// actually active at that moment) beyond the clip ceiling. Trim and output
	// are deliberately not inputs to this: they scale what leaves, not what was cut.
	const clippedPath = $derived(makeClippedPath(drivenWaveformMin, drivenWaveformMax));

	function toggleBoost() {
		boostValue = !boostValue;
		boostState.setValue(boostValue);
	}
	function toggleBypass() {
		if (!bypassState) return;
		bypassState.setValue(!bypassValue);
		bypassValue = bypassState.getValue();
	}
	function selectOversampling(index: number) {
		if (!oversamplingState) return;
		// Optimistic local highlight: the relay echoes back through
		// valueChangedEvent, but if its `choices` have not arrived yet the
		// index would snap to 0 without this.
		oversamplingIndex = index;
		try {
			oversamplingState.setChoiceIndex(index);
		} catch {
			oversamplingIndex = oversamplingFallbackIndex;
		}
	}
	function selectEmphasisMode(index: number) {
		if (!emphasisModeState) return;
		// Optimistic local highlight, same as oversampling: the relay echoes
		// back through valueChangedEvent, but without this the highlight would
		// wait a round trip (or stay put if `choices` has not arrived yet).
		emphasisModeIndex = index;
		try {
			emphasisModeState.setChoiceIndex(index);
		} catch {
			emphasisModeIndex = emphasisModeFallbackIndex;
		}
	}
	function selectView(next: 'wave' | 'eq') {
		view = next;
	}
</script>

<svelte:window
	onresize={updateScale}
	onpointermove={moveKnob}
	onpointerup={endKnob}
	onpointercancel={endKnob}
/>

<div class="stage">
	<main class="panel" style={`transform: scale(${canvasScale})`}>
		<div class="texture texture-brush"></div>
		<div class="texture texture-grain"></div>

		<i class="screw screw-tl"></i>
		<i class="screw screw-tr"></i>
		<i class="screw screw-bl"></i>
		<i class="screw screw-br"></i>

		<h1 class="brand">CACOPHONIC CLIP</h1>

		<div class="top-right">
			<div class="mode">
				<span class="switch-label">MODE</span>
				<div class="os-switch mode-switch" role="radiogroup" aria-label="MODE">
					{#each EMPHASIS_MODES as choice, index (choice)}
						<button
							class="os-seg"
							class:on={index === emphasisModeIndex}
							type="button"
							role="radio"
							aria-checked={index === emphasisModeIndex}
							disabled={!emphasisModeState}
							onclick={() => selectEmphasisMode(index)}
							><span class="os-seg-label">{choice}</span></button
						>
					{/each}
				</div>
			</div>

			<div class="oversampling">
				<span class="switch-label oversampling-label">OVERSAMPLING</span>
				<div class="os-switch" role="radiogroup" aria-label="OVERSAMPLING">
					{#each oversamplingChoices as choice, index (choice)}
						<button
							class="os-seg"
							class:on={index === oversamplingIndex}
							type="button"
							role="radio"
							aria-checked={index === oversamplingIndex}
							disabled={!oversamplingState}
							onclick={() => selectOversampling(index)}
							><span class="os-seg-label">{choice}</span></button
						>
					{/each}
				</div>
			</div>

			<div class="bypass">
				<span class="lamp" class:lit={bypassValue}></span>
				<span class="switch-label">BYPASS</span>
				<button
					class="rocker rocker-top"
					class:on={bypassValue}
					type="button"
					aria-label="Bypass"
					aria-pressed={bypassValue}
					disabled={!bypassState}
					onclick={toggleBypass}
				></button>
			</div>
		</div>

		<div class="seam seam-one"></div>

		<section class="window">
			<div class="win-body">
				<div class="ladder-col">
					<div class="ladder" aria-hidden="true">
						{#each meterSegments as index (index)}
							<i
								class:on={index < litSegments(meterLeft)}
								class:hot={index >= meterSegments.length - 2}
							></i>
						{/each}
					</div>
				</div>

				<div class="win-center">
					<div class="micro-top">
						<div class="view-switch">
							<button
								class="view-btn"
								class:active={view === 'wave'}
								type="button"
								aria-pressed={view === 'wave'}
								onclick={() => selectView('wave')}>WAVE</button
							>
							<button
								class="view-btn"
								class:active={view === 'eq'}
								type="button"
								aria-pressed={view === 'eq'}
								onclick={() => selectView('eq')}>EQ</button
							>
						</div>
						<div class="micro-status">
							<span class="lamp lamp-over" class:lit={overActive}></span>
							<span class="micro over-label" class:lit={overActive}>OVER</span>
						</div>
					</div>

					<svg class="curve-svg" viewBox="0 0 980 168" aria-hidden="true">
						<path class="graticule" d="M 490 0 V 168 M 0 84 H 980" />
						<line class="ceiling-line" x1="0" y1={ceilingY} x2="980" y2={ceilingY} />
						{#if asymValue > 0}
							<line
								class="ceiling-line"
								x1="0"
								y1={ceilingNegativeY}
								x2="980"
								y2={ceilingNegativeY}
							/>
						{/if}
						<path class="curve-line" d={curvePath} />
					</svg>

					<div class="divider"></div>

					{#if view === 'eq'}
						<svg class="wave-svg" viewBox="0 0 980 65" aria-hidden="true">
							<path class="graticule" d={eqGraticule} />
							<path class="eq-post" d={eqPostPath} />
							<path class="curve-line" d={eqPrePath} />
						</svg>
					{:else}
						<svg class="wave-svg" viewBox="0 0 980 65" aria-hidden="true">
							<path class="wave-input" d={inputWaveformPath} />
							<path class="wave-output-fill" d={outputWaveformPath} />
							<path class="wave-clipped" d={clippedPath} />
							<path class="wave-output-stroke" d={outputWaveformPath} />
						</svg>
					{/if}

					<div class="micro-bottom">
						{#if view === 'eq'}
							<span class="micro emph-label">{eqCaption}</span>
						{/if}
						<span class="micro ceiling-label">{ceilingLabel}</span>
					</div>
				</div>

				<div class="ladder-col">
					<div class="ladder" aria-hidden="true">
						{#each meterSegments as index (index)}
							<i
								class:on={index < litSegments(meterRight)}
								class:hot={index >= meterSegments.length - 2}
							></i>
						{/each}
					</div>
				</div>
			</div>
		</section>

		<div class="seam seam-two"></div>

		<div class="controls">
			{#each columns as col (col.label)}
				<div class="col" style={`width: ${col.width}px`}>
					{#if col.kind === 'knob' && col.name}
						<div class="knob-slot" style={`--knob-size: ${col.size}px`}>
							<svg
								class="knob-svg"
								viewBox="0 0 100 100"
								role="slider"
								tabindex="0"
								aria-label={col.label}
								aria-valuemin={ariaRange(col.name as KnobName).min}
								aria-valuemax={ariaRange(col.name as KnobName).max}
								aria-valuenow={ariaRange(col.name as KnobName).now}
								ondblclick={(event) => resetKnob(event, col.name as KnobName)}
								onpointerdown={(event) => beginKnob(event, col.name as KnobName)}
							>
								<path class="knob-groove" d={arcPath(45, -135, 135)} />
								<path
									class="knob-arc"
									d={arcPath(45, -135, -135 + getVisualNormalised(col.name as KnobName) * 270)}
								/>
								{#each knobTicks as angle (angle)}
									<line class="knob-tick" {...tickLine(angle)} />
								{/each}
								<circle class="knob-cap" cx="50" cy="50" r="36" />
								<g transform={`rotate(${getVisualAngle(col.name as KnobName)} 50 50)`}>
									<line class="knob-pointer" x1="50" y1="50" x2="50" y2="7.5" />
								</g>
							</svg>
						</div>
					{:else}
						<div class="knob-slot">
							<button
								class="rocker rocker-lg"
								class:on={boostValue}
								type="button"
								aria-label="2x input gain"
								aria-pressed={boostValue}
								onclick={toggleBoost}
							>
								<span class="lamp" class:lit={boostValue}></span>
							</button>
						</div>
					{/if}

					<div class="row readout" class:readout-lg={col.size === 112}>
						{col.name ? formatKnob(col.name) : ''}
					</div>
					<div class="row main-label" class:degraded={col.degraded}>{col.label}</div>
					<div class="row sub-label">{col.sub}</div>
				</div>
			{/each}
		</div>
	</main>
</div>
