import {Easing} from 'remotion';

/**
 * Palette is taken from the app's own Qt theme
 * (client/src/ui/resources/style/modern_dark.qss): zinc neutrals + indigo accent.
 * Keeping the video on the product's real palette keeps it sober instead of
 * "rainbow promo" — a single accent colour carries every highlight.
 */
export const C = {
  bg: '#09090B',
  bgLift: '#0F0F12',
  panel: '#18181B',
  panelLift: '#1F1F24',
  line: '#27272A',
  lineStrong: '#3F3F46',
  text: '#FAFAFA',
  body: '#D4D4D8',
  muted: '#A1A1AA',
  dim: '#71717A',
  accent: '#6366F1',
  accentSoft: '#818CF8',
  accentDeep: '#4F46E5',
  ok: '#22C55E',
  warn: '#F59E0B',
};

export const F = {
  sans: '"Segoe UI", "Microsoft YaHei UI", "Microsoft YaHei", system-ui, sans-serif',
  mono: '"Cascadia Mono", "Consolas", "Microsoft YaHei UI", "Microsoft YaHei", monospace',
};

export const E = {
  /** Primary entrance curve — fast out, long settle. Calm, no overshoot. */
  out: Easing.bezier(0.16, 1, 0.3, 1),
  /** Symmetrical, for continuous motion. */
  inOut: Easing.bezier(0.65, 0, 0.35, 1),
  /** Slightly snappier, for small UI pops. */
  snap: Easing.bezier(0.22, 1, 0.36, 1),
  linear: Easing.linear,
};

/** Generous safe area for 1080p: 140px sides, 96px top/bottom. */
export const SAFE = '96px 140px';

export const clamp = {extrapolateLeft: 'clamp', extrapolateRight: 'clamp'} as const;
