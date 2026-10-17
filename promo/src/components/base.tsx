import {AbsoluteFill, interpolate, useCurrentFrame} from 'remotion';
import {C, E, F, clamp} from '../theme';

/** Faint engineering grid + soft accent wash. Shared by every scene. */
export const Backdrop = ({drift = 0}: {drift?: number}) => {
  const frame = useCurrentFrame();
  const x = interpolate(frame, [0, 240], [0, drift], clamp);
  return (
    <AbsoluteFill style={{backgroundColor: C.bg}}>
      <AbsoluteFill
        style={{
          backgroundImage: `linear-gradient(to right, ${C.line} 1px, transparent 1px), linear-gradient(to bottom, ${C.line} 1px, transparent 1px)`,
          backgroundSize: '88px 88px',
          opacity: interpolate(frame, [0, 24], [0, 0.5], {...clamp, easing: E.out}),
          maskImage:
            'radial-gradient(ellipse 72% 66% at 50% 46%, rgba(0,0,0,0.95) 8%, rgba(0,0,0,0.35) 55%, transparent 82%)',
          WebkitMaskImage:
            'radial-gradient(ellipse 72% 66% at 50% 46%, rgba(0,0,0,0.95) 8%, rgba(0,0,0,0.35) 55%, transparent 82%)',
          translate: `${x}px 0px`,
        }}
      />
      {/* one restrained light source, top-left */}
      <AbsoluteFill
        style={{
          background:
            'radial-gradient(circle 900px at 18% -8%, rgba(99,102,241,0.12), transparent 70%)',
        }}
      />
      <AbsoluteFill
        style={{
          background:
            'radial-gradient(circle 1100px at 88% 108%, rgba(129,140,248,0.06), transparent 72%)',
        }}
      />
    </AbsoluteFill>
  );
};

/** Editorial section marker: index, hairline, label. */
export const Kicker = ({
  index,
  label,
  from = 0,
}: {
  index: string;
  label: string;
  from?: number;
}) => {
  const frame = useCurrentFrame();
  return (
    <div
      style={{
        display: 'flex',
        alignItems: 'center',
        gap: 18,
        fontFamily: F.mono,
        fontSize: 22,
        letterSpacing: '0.16em',
        textTransform: 'uppercase',
        opacity: interpolate(frame, [from, from + 16], [0, 1], {...clamp, easing: E.out}),
        translate: interpolate(frame, [from, from + 18], ['0px 14px', '0px 0px'], {
          ...clamp,
          easing: E.out,
        }),
      }}
    >
      <span style={{color: C.accentSoft}}>{index}</span>
      <span
        style={{
          display: 'block',
          width: interpolate(frame, [from + 4, from + 26], [0, 46], {...clamp, easing: E.out}),
          height: 1,
          backgroundColor: C.lineStrong,
        }}
      />
      <span style={{color: C.muted}}>{label}</span>
    </div>
  );
};

/** Masked line-by-line reveal — the main "kinetic" device of the film. */
export const Headline = ({
  lines,
  from = 0,
  size = 86,
  weight = 600,
  gap = 8,
  stagger = 6,
  color = C.text,
  align = 'left' as 'left' | 'center',
}: {
  lines: string[];
  from?: number;
  size?: number;
  weight?: number;
  gap?: number;
  stagger?: number;
  color?: string;
  align?: 'left' | 'center';
}) => {
  const frame = useCurrentFrame();
  return (
    <div style={{display: 'flex', flexDirection: 'column', gap, textAlign: align}}>
      {lines.map((line, i) => {
        const s = from + i * stagger;
        return (
          <div
            key={line}
            style={{overflow: 'hidden', paddingBottom: '0.14em', marginBottom: '-0.14em'}}
          >
            <div
              style={{
                fontFamily: F.sans,
                fontSize: size,
                fontWeight: weight,
                lineHeight: 1.16,
                letterSpacing: '-0.02em',
                color,
                translate: interpolate(
                  frame,
                  [s, s + 26],
                  [`0px ${Math.round(size * 0.85)}px`, '0px 0px'],
                  {...clamp, easing: E.out},
                ),
                opacity: interpolate(frame, [s, s + 12], [0, 1], {...clamp}),
              }}
            >
              {line}
            </div>
          </div>
        );
      })}
    </div>
  );
};

export const Chip = ({
  children,
  tone = 'default',
}: {
  children: React.ReactNode;
  tone?: 'default' | 'accent' | 'ok';
}) => {
  const palette =
    tone === 'accent'
      ? {border: 'rgba(99,102,241,0.5)', color: C.accentSoft, bg: 'rgba(99,102,241,0.10)'}
      : tone === 'ok'
        ? {border: 'rgba(34,197,94,0.45)', color: C.ok, bg: 'rgba(34,197,94,0.10)'}
        : {border: C.line, color: C.muted, bg: 'rgba(24,24,27,0.7)'};
  return (
    <span
      style={{
        display: 'inline-flex',
        alignItems: 'center',
        gap: 10,
        padding: '10px 20px',
        borderRadius: 999,
        border: `1px solid ${palette.border}`,
        backgroundColor: palette.bg,
        color: palette.color,
        fontFamily: F.mono,
        fontSize: 22,
        letterSpacing: '0.02em',
        whiteSpace: 'nowrap',
      }}
    >
      {children}
    </span>
  );
};

export const Bullet = ({
  children,
  from = 0,
  size = 32,
}: {
  children: React.ReactNode;
  from?: number;
  size?: number;
}) => {
  const frame = useCurrentFrame();
  return (
    <div
      style={{
        display: 'flex',
        alignItems: 'center',
        gap: 18,
        opacity: interpolate(frame, [from, from + 14], [0, 1], {...clamp, easing: E.out}),
        translate: interpolate(frame, [from, from + 20], ['0px 16px', '0px 0px'], {
          ...clamp,
          easing: E.out,
        }),
      }}
    >
      <span
        style={{
          display: 'block',
          width: interpolate(frame, [from + 2, from + 20], [0, 16], {...clamp, easing: E.out}),
          height: 2,
          backgroundColor: C.accent,
          flexShrink: 0,
        }}
      />
      <span
        style={{
          fontFamily: F.sans,
          fontSize: size,
          color: C.body,
          lineHeight: 1.5,
          letterSpacing: '0.01em',
        }}
      >
        {children}
      </span>
    </div>
  );
};

/** Consistent product tag, bottom-right, on every scene but the title/outro. */
export const ProductTag = () => (
  <div
    style={{
      position: 'absolute',
      right: 140,
      bottom: 44,
      fontFamily: F.mono,
      fontSize: 18,
      letterSpacing: '0.18em',
      color: C.dim,
      opacity: 0.75,
      textTransform: 'uppercase',
    }}
  >
    GameScrcpy · 1.3.4
  </div>
);

/** 1px accent progress rail along the bottom of the whole film. */
export const ProgressRail = ({total}: {total: number}) => {
  const frame = useCurrentFrame();
  return (
    <div
      style={{
        position: 'absolute',
        left: 0,
        right: 0,
        bottom: 0,
        height: 2,
        backgroundColor: C.line,
        opacity: 0.9 * interpolate(frame, [total - 30, total - 8], [1, 0], {...clamp, easing: E.inOut}),
      }}
    >
      <div
        style={{
          height: '100%',
          width: `${Math.min(100, (frame / Math.max(1, total - 1)) * 100)}%`,
          backgroundColor: C.accent,
          opacity: 0.85,
        }}
      />
    </div>
  );
};

/** Product mark: cast/stream glyph in a rounded tile. */
export const Logo = ({size = 104}: {size?: number}) => (
  <div
    style={{
      width: size,
      height: size,
      borderRadius: size * 0.28,
      border: `1px solid ${C.lineStrong}`,
      background: `linear-gradient(160deg, ${C.panelLift}, ${C.bg})`,
      display: 'flex',
      alignItems: 'center',
      justifyContent: 'center',
      boxShadow: '0 24px 60px rgba(0,0,0,0.45)',
    }}
  >
    <svg width={size * 0.56} height={size * 0.56} viewBox="0 0 48 48" fill="none">
      <rect
        x="11"
        y="9"
        width="27"
        height="18"
        rx="3"
        stroke={C.accentSoft}
        strokeWidth="2.2"
      />
      <path d="M12 38 A6 6 0 0 1 18 44" stroke={C.accentSoft} strokeWidth="2.2" strokeLinecap="round" />
      <path
        d="M12 31.5 A12.5 12.5 0 0 1 24.5 44"
        stroke={C.accent}
        strokeWidth="2.2"
        strokeLinecap="round"
        opacity="0.75"
      />
      <circle cx="12" cy="44" r="2" fill={C.accentSoft} />
    </svg>
  </div>
);
