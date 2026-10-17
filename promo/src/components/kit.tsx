import {interpolate, useCurrentFrame} from 'remotion';
import {C, E, F, clamp} from '../theme';

/** Animated numeric readout. Mono, tabular, no layout jump. */
export const Counter = ({
  to,
  from = 0,
  start = 0,
  dur = 36,
  digits = 0,
  suffix = '',
  color = C.accentSoft,
  size = 28,
  weight = 600,
  prefix = '',
}: {
  to: number;
  from?: number;
  start?: number;
  dur?: number;
  digits?: number;
  suffix?: string;
  color?: string;
  size?: number;
  weight?: number;
  prefix?: string;
}) => {
  const frame = useCurrentFrame();
  const v = interpolate(frame, [start, start + dur], [from, to], {...clamp, easing: E.out});
  return (
    <span
      style={{
        fontFamily: F.mono,
        fontSize: size,
        fontWeight: weight,
        color,
        fontVariantNumeric: 'tabular-nums',
        letterSpacing: '-0.01em',
      }}
    >
      {prefix}
      {v.toFixed(digits)}
      {suffix}
    </span>
  );
};

/** Progressive polyline draw with a travelling head dot. */
export const Sparkline = ({
  points,
  w,
  h,
  start = 0,
  dur = 40,
  stroke = C.accent,
  fill = 'rgba(99,102,241,0.12)',
  head = true,
  baseline = true,
}: {
  points: number[][];
  w: number;
  h: number;
  start?: number;
  dur?: number;
  stroke?: string;
  fill?: string;
  head?: boolean;
  baseline?: boolean;
}) => {
  const frame = useCurrentFrame();
  const d = points.map((p, i) => `${i === 0 ? 'M' : 'L'}${p[0]} ${p[1]}`).join(' ');
  let total = 0;
  for (let i = 1; i < points.length; i++) {
    total += Math.hypot(points[i][0] - points[i - 1][0], points[i][1] - points[i - 1][1]);
  }
  const p = interpolate(frame, [start, start + dur], [0, 1], {...clamp, easing: E.inOut});
  // position of the head along the path
  let acc = 0;
  let hx = points[0][0];
  let hy = points[0][1];
  for (let i = 1; i < points.length; i++) {
    const seg = Math.hypot(points[i][0] - points[i - 1][0], points[i][1] - points[i - 1][1]);
    if (acc + seg >= p * total) {
      const t = seg === 0 ? 0 : (p * total - acc) / seg;
      hx = points[i - 1][0] + (points[i][0] - points[i - 1][0]) * t;
      hy = points[i - 1][1] + (points[i][1] - points[i - 1][1]) * t;
      break;
    }
    acc += seg;
    hx = points[i][0];
    hy = points[i][1];
  }
  return (
    <svg width={w} height={h} style={{overflow: 'visible'}}>
      {baseline ? (
        <line x1={0} y1={h - 1} x2={w} y2={h - 1} stroke={C.line} strokeWidth={1} />
      ) : null}
      <path d={`${d} L${points[points.length - 1][0]} ${h} L${points[0][0]} ${h} Z`} fill={fill} opacity={p * 0.9} />
      <path
        d={d}
        fill="none"
        stroke={stroke}
        strokeWidth={2}
        strokeLinecap="round"
        strokeLinejoin="round"
        strokeDasharray={total}
        strokeDashoffset={total * (1 - p)}
      />
      {head && p > 0.02 ? (
        <>
          <circle cx={hx} cy={hy} r={7} fill={stroke} opacity={0.18} />
          <circle cx={hx} cy={hy} r={3} fill={stroke} />
        </>
      ) : null}
    </svg>
  );
};

/** Expanding ring used for clicks. */
export const Ripple = ({
  x,
  y,
  start,
  size = 84,
  color = C.accentSoft,
  dur = 22,
}: {
  x: number;
  y: number;
  start: number;
  size?: number;
  color?: string;
  dur?: number;
}) => {
  const frame = useCurrentFrame();
  const t = interpolate(frame, [start, start + dur], [0, 1], clamp);
  if (t <= 0 || t >= 1) return null;
  return (
    <div
      style={{
        position: 'absolute',
        left: x,
        top: y,
        width: size,
        height: size,
        marginLeft: -size / 2,
        marginTop: -size / 2,
        borderRadius: 999,
        border: `1.5px solid ${color}`,
        opacity: (1 - t) * 0.7,
        scale: `${0.25 + t * 0.95}`,
      }}
    />
  );
};

const CURSOR_PATH = 'M0 0 L0 15.5 L4 11.6 L6.7 17.4 L9.6 16.1 L7 10.4 L12.4 10.2 Z';

/** Pointer that glides through waypoints; optional click pulses at given stops. */
export const Cursor = ({
  waypoints,
  start = 0,
  seg = 18,
  size = 30,
  clicks = [],
}: {
  waypoints: number[][];
  start?: number;
  seg?: number;
  size?: number;
  clicks?: number[];
}) => {
  const frame = useCurrentFrame();
  const n = waypoints.length;
  const idx = Math.min(n - 2, Math.max(0, Math.floor((frame - start) / seg)));
  const local = interpolate(frame, [start + idx * seg, start + (idx + 1) * seg], [0, 1], {
    ...clamp,
    easing: E.inOut,
  });
  const a = waypoints[Math.max(0, Math.min(n - 1, idx))];
  const b = waypoints[Math.max(0, Math.min(n - 1, idx + 1))];
  const x = a[0] + (b[0] - a[0]) * local;
  const y = a[1] + (b[1] - a[1]) * local;
  const appearing = interpolate(frame, [start - 6, start + 2], [0, 1], clamp);
  // press effect: quick dip of the pointer whenever a click is registered
  let press = 1;
  for (const c of clicks) {
    const d = Math.abs(frame - c);
    if (d < 6) press = Math.min(press, 0.86 + 0.14 * (d / 6));
  }
  return (
    <div
      style={{
        position: 'absolute',
        left: 0,
        top: 0,
        translate: `${x}px ${y}px`,
        opacity: appearing,
        scale: `${press}`,
        transformOrigin: '0 0',
        filter: 'drop-shadow(0 4px 10px rgba(0,0,0,0.55))',
      }}
    >
      <svg width={size} height={size * 1.15} viewBox="0 0 14 19">
        <path d={CURSOR_PATH} fill="#FAFAFA" stroke="#09090B" strokeWidth="1.3" />
      </svg>
    </div>
  );
};

/** Slow breathing dot — liveliness without noise. */
export const PulseDot = ({
  size = 10,
  color = C.accent,
  period = 40,
  phase = 0,
}: {
  size?: number;
  color?: string;
  period?: number;
  phase?: number;
}) => {
  const frame = useCurrentFrame();
  const t = (Math.sin(((frame + phase) / period) * Math.PI * 2) + 1) / 2;
  return (
    <span style={{position: 'relative', display: 'inline-block', width: size, height: size}}>
      <span
        style={{
          position: 'absolute',
          inset: 0,
          borderRadius: 999,
          backgroundColor: color,
          opacity: 0.16 + t * 0.14,
          scale: `${1 + t * 0.9}`,
        }}
      />
      <span
        style={{
          position: 'absolute',
          inset: size * 0.22,
          borderRadius: 999,
          backgroundColor: color,
          opacity: 0.55 + t * 0.4,
        }}
      />
    </span>
  );
};

/** Thin accent ring highlighting a region of the real UI (source coords). */
export const Spot = ({
  x,
  y,
  w,
  h,
  start,
  index,
  radius = 12,
}: {
  x: number;
  y: number;
  w: number;
  h: number;
  start: number;
  index?: number;
  radius?: number;
}) => {
  const frame = useCurrentFrame();
  const t = interpolate(frame, [start, start + 18], [0, 1], {...clamp, easing: E.out});
  if (t <= 0) return null;
  const pad = 6;
  return (
    <div style={{position: 'absolute', left: x - pad, top: y - pad, pointerEvents: 'none'}}>
      <div
        style={{
          width: (w + pad * 2) * (0.82 + t * 0.18),
          height: (h + pad * 2) * (0.82 + t * 0.18),
          borderRadius: radius,
          border: `2px solid rgba(129,140,248,${0.75 * t})`,
          boxShadow: `0 0 0 1px rgba(9,9,11,0.55), 0 0 26px rgba(99,102,241,${0.22 * t})`,
          opacity: t,
        }}
      />
      {index ? (
        <div
          style={{
            position: 'absolute',
            left: -14,
            top: -14,
            width: 26,
            height: 26,
            borderRadius: 999,
            backgroundColor: C.accent,
            color: '#0B0B12',
            fontFamily: F.mono,
            fontSize: 15,
            fontWeight: 700,
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            opacity: t,
            scale: `${0.6 + t * 0.4}`,
          }}
        >
          {index}
        </div>
      ) : null}
    </div>
  );
};
