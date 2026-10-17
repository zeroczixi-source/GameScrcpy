import {C, F} from '../theme';

/** Abstract shooter-ish HUD — stands in for "a game on screen" without copying art. */
export const GameHud = ({w, h, detail = 1}: {w: number; h: number; detail?: number}) => {
  const u = Math.min(w, h) / 100;
  return (
    <div
      style={{
        position: 'absolute',
        inset: 0,
        overflow: 'hidden',
        background: 'linear-gradient(180deg, #101016 0%, #0A0A0E 62%, #08080B 100%)',
      }}
    >
      {/* horizon band */}
      <div
        style={{
          position: 'absolute',
          left: 0,
          right: 0,
          top: h * 0.52,
          height: 1,
          background: `linear-gradient(90deg, transparent, rgba(129,140,248,0.35), transparent)`,
        }}
      />
      {/* terrain silhouettes */}
      {[0.62, 0.73, 0.86].map((t, i) => (
        <div
          key={t}
          style={{
            position: 'absolute',
            left: -w * 0.05,
            right: -w * 0.05,
            top: h * t,
            height: h * 0.16,
            borderRadius: `${w * 0.18}px ${w * 0.18}px 0 0`,
            background: `rgba(${i === 0 ? '30,30,38' : i === 1 ? '22,22,28' : '16,16,20'},1)`,
            opacity: 0.9,
          }}
        />
      ))}
      {/* minimap */}
      <div
        style={{
          position: 'absolute',
          left: 10 * u,
          top: 8 * u,
          width: 20 * u,
          height: 20 * u,
          borderRadius: 4 * u,
          border: `1px solid rgba(161,161,170,0.28)`,
          background: 'rgba(9,9,11,0.55)',
        }}
      >
        <div
          style={{
            position: 'absolute',
            left: '30%',
            top: '42%',
            width: 3 * u,
            height: 3 * u,
            borderRadius: 99,
            background: C.accentSoft,
          }}
        />
        <div
          style={{
            position: 'absolute',
            left: '66%',
            top: '62%',
            width: 3 * u,
            height: 3 * u,
            borderRadius: 99,
            background: 'rgba(250,250,250,0.4)',
          }}
        />
      </div>
      {/* crosshair */}
      <div
        style={{
          position: 'absolute',
          left: '50%',
          top: '48%',
          translate: '-50% -50%',
          width: 9 * u,
          height: 9 * u,
        }}
      >
        {[
          {left: '50%', top: 0, w: 1, h: '32%'},
          {left: '50%', bottom: 0, w: 1, h: '32%'},
          {top: '50%', left: 0, h: 1, w: '32%'},
          {top: '50%', right: 0, h: 1, w: '32%'},
        ].map((s, i) => (
          <div
            key={i}
            style={{
              position: 'absolute',
              background: 'rgba(250,250,250,0.55)',
              width: s.w === 1 ? 1 : (s.w as number) * u,
              height: s.h === 1 ? 1 : (s.h as number) * u,
              left: s.left as string | number,
              right: (s as {right?: string | number}).right,
              top: s.top as string | number,
              bottom: (s as {bottom?: string | number}).bottom,
            }}
          />
        ))}
        <div
          style={{
            position: 'absolute',
            inset: '34%',
            borderRadius: 99,
            border: '1px solid rgba(250,250,250,0.35)',
          }}
        />
      </div>
      {/* health */}
      <div
        style={{
          position: 'absolute',
          left: 10 * u,
          bottom: 9 * u,
          width: 40 * u,
          height: 2.4 * u,
          borderRadius: 99,
          background: 'rgba(250,250,250,0.12)',
        }}
      >
        <div
          style={{
            width: '68%',
            height: '100%',
            borderRadius: 99,
            background: 'rgba(34,197,94,0.85)',
          }}
        />
      </div>
      {/* ability buttons */}
      {detail > 0.5 &&
        [
          {r: 8 * u, left: '78%', bottom: '7%'},
          {r: 5.5 * u, left: '63%', bottom: '15%'},
        ].map((b, i) => (
          <div
            key={i}
            style={{
              position: 'absolute',
              width: b.r * 2,
              height: b.r * 2,
              left: b.left,
              bottom: b.bottom,
              borderRadius: 99,
              border: '1px solid rgba(161,161,170,0.3)',
              background: 'rgba(9,9,11,0.35)',
            }}
          />
        ))}
    </div>
  );
};

/** Phone body: bezel, rounded screen, notch-free modern look. */
export const PhoneFrame = ({
  width,
  height,
  children,
  accent = false,
}: {
  width: number;
  height: number;
  children?: React.ReactNode;
  accent?: boolean;
}) => (
  <div
    style={{
      width,
      height,
      borderRadius: width * 0.11,
      padding: width * 0.028,
      backgroundColor: '#0C0C10',
      border: `1px solid ${accent ? 'rgba(99,102,241,0.55)' : C.lineStrong}`,
      boxShadow: `0 40px 90px rgba(0,0,0,0.6), 0 0 0 1px rgba(255,255,255,0.03) inset${
        accent ? ', 0 0 60px rgba(99,102,241,0.18)' : ''
      }`,
      position: 'relative',
    }}
  >
    <div
      style={{
        width: '100%',
        height: '100%',
        borderRadius: width * 0.085,
        overflow: 'hidden',
        position: 'relative',
        backgroundColor: '#08080B',
      }}
    >
      {children}
    </div>
  </div>
);

/** Desktop window chrome for mirrored output / editors. */
export const WindowFrame = ({
  width,
  height,
  title,
  right,
  children,
  accent = false,
}: {
  width: number;
  height: number;
  title: string;
  right?: React.ReactNode;
  children?: React.ReactNode;
  accent?: boolean;
}) => (
  <div
    style={{
      width,
      height,
      borderRadius: 14,
      border: `1px solid ${accent ? 'rgba(99,102,241,0.5)' : C.lineStrong}`,
      backgroundColor: C.panel,
      overflow: 'hidden',
      boxShadow: `0 40px 90px rgba(0,0,0,0.55)${
        accent ? ', 0 0 70px rgba(99,102,241,0.14)' : ''
      }`,
      display: 'flex',
      flexDirection: 'column',
    }}
  >
    <div
      style={{
        height: 44,
        flexShrink: 0,
        display: 'flex',
        alignItems: 'center',
        gap: 12,
        padding: '0 18px',
        borderBottom: `1px solid ${C.line}`,
        backgroundColor: '#131317',
      }}
    >
      {['#3F3F46', '#3F3F46', '#3F3F46'].map((dot, i) => (
        <div
          key={i}
          style={{width: 10, height: 10, borderRadius: 99, backgroundColor: dot, opacity: 0.9}}
        />
      ))}
      <div
        style={{
          marginLeft: 10,
          fontFamily: F.mono,
          fontSize: 17,
          color: C.muted,
          letterSpacing: '0.04em',
        }}
      >
        {title}
      </div>
      <div style={{marginLeft: 'auto'}}>{right}</div>
    </div>
    <div style={{flex: 1, position: 'relative', overflow: 'hidden'}}>{children}</div>
  </div>
);
