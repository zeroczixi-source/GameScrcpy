// Generates a restrained ambient bed for the promo (no external assets, no network).
// Style: slow A-minor pad cycle, soft sub bass, sparse plucked eighths, air layer.
// Output: public/bgm.wav — 44.1kHz / 16-bit / stereo.
import {writeFileSync, mkdirSync} from 'node:fs';
import {dirname, resolve} from 'node:path';
import {fileURLToPath} from 'node:url';

const here = dirname(fileURLToPath(import.meta.url));
const outPath = resolve(here, '../public/bgm.wav');

const SR = 44100;
const DUR = Number(process.argv[2] || 92); // seconds; must exceed the composition length (tail trimmed by Remotion)
const N = Math.floor(SR * DUR);
const BPM = 72;
const BEAT = 60 / BPM;
const BAR = BEAT * 4;
const CHORD_BARS = 2;

const midiToHz = (m) => 440 * Math.pow(2, (m - 69) / 12);

// Am - F - C - G, two bars each
const CHORDS = [
  {name: 'Am', notes: [45, 48, 52], bass: 33},
  {name: 'F', notes: [41, 45, 48], bass: 29},
  {name: 'C', notes: [48, 52, 55], bass: 36},
  {name: 'G', notes: [43, 47, 50], bass: 31},
];

const L = new Float32Array(N);
const R = new Float32Array(N);

const pan = (x) => {
  const p = Math.max(-1, Math.min(1, x));
  const a = (p + 1) * 0.25 * Math.PI;
  return [Math.cos(a), Math.sin(a)];
};

const write = (i, v, l, r) => {
  if (i < 0 || i >= N) return;
  L[i] += v * l;
  R[i] += v * r;
};

/** Sustained pad voice: sine + quiet 2nd/3rd harmonics, slow attack/release. */
const pad = ({start, dur, freq, amp, panPos, detune = 0}) => {
  const f = freq * Math.pow(2, detune / 1200);
  const s0 = Math.floor(start * SR);
  const len = Math.floor(dur * SR);
  const atk = Math.floor(1.1 * SR);
  const rel = Math.floor(1.4 * SR);
  const [gl, gr] = pan(panPos);
  const w1 = (2 * Math.PI * f) / SR;
  const w2 = w1 * 2;
  const w3 = w1 * 3;
  for (let k = 0; k < len; k++) {
    const i = s0 + k;
    if (i >= N) break;
    const env =
      k < atk
        ? Math.sin((k / atk) * Math.PI * 0.5) ** 2
        : Math.min(1, Math.max(0, (len - k) / rel)) ** 1.5;
    if (env <= 0) continue;
    const s =
      Math.sin(w1 * k) * 0.62 +
      Math.sin(w2 * k + 0.6) * 0.16 +
      Math.sin(w3 * k + 1.2) * 0.05;
    write(i, s * env * amp, gl, gr);
  }
};

/** Sub bass: pure sine plus a faint octave, long fade. */
const bass = ({start, dur, freq, amp}) => {
  const s0 = Math.floor(start * SR);
  const len = Math.floor(dur * SR);
  const atk = Math.floor(0.35 * SR);
  const rel = Math.floor(1.6 * SR);
  const w1 = (2 * Math.PI * freq) / SR;
  const w2 = w1 * 2;
  for (let k = 0; k < len; k++) {
    const i = s0 + k;
    if (i >= N) break;
    const env =
      k < atk
        ? k / atk
        : Math.min(1, Math.max(0, (len - k) / rel)) ** 1.4;
    if (env <= 0) continue;
    const s = Math.sin(w1 * k) * 0.8 + Math.sin(w2 * k) * 0.12;
    write(i, s * env * amp, 0.5, 0.5);
  }
};

/** Sparse pluck: fast exponential decay, light stereo placement. */
const pluck = ({start, freq, amp, panPos, decay = 0.42}) => {
  const s0 = Math.floor(start * SR);
  const len = Math.floor(decay * SR * 4);
  const [gl, gr] = pan(panPos);
  const w1 = (2 * Math.PI * freq) / SR;
  const w2 = w1 * 2.004;
  for (let k = 0; k < len; k++) {
    const i = s0 + k;
    if (i >= N) break;
    const t = k / SR;
    const env = Math.exp(-t / decay) * Math.min(1, t / 0.004);
    const s = Math.sin(w1 * k) * 0.8 + Math.sin(w2 * k) * 0.18;
    write(i, s * env * amp, gl, gr);
  }
};

/** Soft low thump marking bar starts — felt more than heard. */
const thump = ({start, amp}) => {
  const s0 = Math.floor(start * SR);
  const len = Math.floor(0.5 * SR);
  const w = (2 * Math.PI * 58) / SR;
  for (let k = 0; k < len; k++) {
    const i = s0 + k;
    if (i >= N) break;
    const t = k / SR;
    const f = 58 * (1 + 1.2 * Math.exp(-t / 0.06));
    const env = Math.exp(-t / 0.15) * Math.min(1, t / 0.006);
    const s = Math.sin(((2 * Math.PI * f) / SR) * k);
    write(i, s * env * amp, 0.5, 0.5);
  }
};

// --- arrangement -----------------------------------------------------------
const totalBars = Math.ceil(DUR / BAR);

for (let bar = 0; bar * BAR < DUR; bar++) {
  const t0 = bar * BAR;
  const chord = CHORDS[Math.floor(bar / CHORD_BARS) % CHORDS.length];

  // pad only on chord changes, held across the two bars + overlap
  if (bar % CHORD_BARS === 0) {
    const dur = BAR * CHORD_BARS + 1.6;
    const gain = 0.085 * (bar === 0 ? 0.8 : 1);
    chord.notes.forEach((n, idx) => {
      pad({start: t0, dur, freq: midiToHz(n), amp: gain, panPos: -0.35 + idx * 0.35, detune: -5});
      pad({start: t0, dur, freq: midiToHz(n), amp: gain * 0.75, panPos: 0.3 - idx * 0.35, detune: 6});
    });
    // octave-down shadow of the root, very quiet
    pad({start: t0, dur, freq: midiToHz(chord.notes[0] - 12), amp: gain * 0.5, panPos: 0});
    bass({start: t0, dur: dur - 0.2, freq: midiToHz(chord.bass), amp: 0.1});
  }

  // sparse plucked eighths, breathing room in every 4th bar
  if (bar % 4 !== 3) {
    const pattern = [0, 1, 2, 1];
    for (let step = 0; step < 4; step++) {
      const t = t0 + step * BEAT;
      if (t >= DUR) break;
      const note = chord.notes[pattern[step]] + 24;
      pluck({
        start: t + 0.01,
        freq: midiToHz(note),
        amp: step % 2 === 0 ? 0.03 : 0.021,
        panPos: step % 2 === 0 ? -0.45 : 0.45,
        decay: 0.38,
      });
    }
  }

  if (bar % CHORD_BARS === 0) thump({start: t0 + 0.02, amp: 0.07});
}

// --- air layer: filtered noise with a slow swell ---------------------------
{
  let lp = 0;
  let lp2 = 0;
  let seed = 1337;
  for (let i = 0; i < N; i++) {
    seed = (seed * 1103515245 + 12345) & 0x7fffffff;
    const white = (seed / 0x7fffffff) * 2 - 1;
    lp += (white - lp) * 0.02;
    lp2 += (lp - lp2) * 0.06;
    const t = i / SR;
    const swell = 0.5 + 0.5 * Math.sin((2 * Math.PI * t) / 21 - 1.2);
    const fadeEdges = Math.min(1, t / 2.5, (DUR - t) / 4);
    const v = lp2 * 0.02 * (0.35 + 0.65 * swell) * fadeEdges;
    L[i] += v * 0.85;
    R[i] += v * 1.0;
  }
}

// --- master: gentle lowpass, soft clip, normalize --------------------------
{
  let lpL = 0;
  let lpR = 0;
  let peak = 0;
  for (let i = 0; i < N; i++) {
    lpL += (L[i] - lpL) * 0.55;
    lpR += (R[i] - lpR) * 0.55;
    L[i] = Math.tanh(lpL * 1.15);
    R[i] = Math.tanh(lpR * 1.15);
    peak = Math.max(peak, Math.abs(L[i]), Math.abs(R[i]));
  }
  const g = 0.72 / (peak || 1);
  for (let i = 0; i < N; i++) {
    const t = i / SR;
    const edge = Math.min(1, t / 0.4, (DUR - t) / 1.2);
    L[i] *= g * Math.max(0, edge);
    R[i] *= g * Math.max(0, edge);
  }
}

// --- write 16-bit PCM WAV --------------------------------------------------
const bytes = 44 + N * 4;
const buf = Buffer.alloc(bytes);
buf.write('RIFF', 0);
buf.writeUInt32LE(bytes - 8, 4);
buf.write('WAVE', 8);
buf.write('fmt ', 12);
buf.writeUInt32LE(16, 16);
buf.writeUInt16LE(1, 20);
buf.writeUInt16LE(2, 22);
buf.writeUInt32LE(SR, 24);
buf.writeUInt32LE(SR * 4, 28);
buf.writeUInt16LE(4, 32);
buf.writeUInt16LE(16, 34);
buf.write('data', 36);
buf.writeUInt32LE(N * 4, 40);
let o = 44;
const clip = (x) => Math.max(-1, Math.min(1, x));
for (let i = 0; i < N; i++) {
  buf.writeInt16LE(Math.round(clip(L[i]) * 32767), o);
  o += 2;
  buf.writeInt16LE(Math.round(clip(R[i]) * 32767), o);
  o += 2;
}

mkdirSync(dirname(outPath), {recursive: true});
writeFileSync(outPath, buf);
console.log(`wrote ${outPath}  ${(bytes / 1024 / 1024).toFixed(2)} MB  ${DUR}s  ${BPM}BPM`);
