export const trimMinimumDb = -24;

export function effectiveTrimDb(trimDb: number, driveDb: number, autoGain: boolean) {
	if (!autoGain) return trimDb;
	return Math.max(trimMinimumDb, Math.min(0, -driveDb));
}

interface ClipMeterPayload {
	drivenPeak?: number;
	drivenWaveformMin?: number[];
	drivenWaveformMax?: number[];
}

export function readClipActive(payload: ClipMeterPayload) {
	const peak = payload.drivenPeak;
	return typeof peak === 'number' && Number.isFinite(peak) && peak > 1;
}
