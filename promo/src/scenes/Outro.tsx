import {AbsoluteFill, interpolate, useCurrentFrame} from 'remotion';
import {Backdrop, Chip, Headline, Logo} from '../components/base';
import {C, E, F, clamp} from '../theme';

export const Outro = () => {
  const frame = useCurrentFrame();

  return (
    <AbsoluteFill>
      <Backdrop drift={14} />

      <div
        style={{
          position: 'absolute',
          inset: 0,
          display: 'flex',
          flexDirection: 'column',
          alignItems: 'center',
          justifyContent: 'center',
          scale: interpolate(frame, [0, 170], [1, 1.03], {easing: E.linear}),
        }}
      >
        <div
          style={{
            fontFamily: F.mono,
            fontSize: 20,
            letterSpacing: '0.28em',
            color: C.dim,
            opacity: interpolate(frame, [2, 20], [0, 1], {...clamp, easing: E.out}),
          }}
        >
          READY · WINDOWS 10 / 11
        </div>

        <div style={{marginTop: 34}}>
          <Headline lines={['连上，即玩。']} from={10} size={104} align="center" />
        </div>

        <div
          style={{
            marginTop: 26,
            fontFamily: F.sans,
            fontSize: 34,
            color: C.muted,
            letterSpacing: '0.06em',
            opacity: interpolate(frame, [36, 56], [0, 1], {...clamp, easing: E.out}),
            translate: interpolate(frame, [36, 62], ['0px 14px', '0px 0px'], {
              ...clamp,
              easing: E.out,
            }),
          }}
        >
          USB 或 WiFi · 无需 ROOT
        </div>

        <div
          style={{
            width: interpolate(frame, [52, 84], [0, 300], {...clamp, easing: E.out}),
            height: 2,
            backgroundColor: C.accent,
            marginTop: 44,
            opacity: 0.9,
          }}
        />

        <div
          style={{
            display: 'flex',
            alignItems: 'center',
            gap: 26,
            marginTop: 44,
            opacity: interpolate(frame, [62, 82], [0, 1], {...clamp, easing: E.out}),
            translate: interpolate(frame, [62, 88], ['0px 18px', '0px 0px'], {
              ...clamp,
              easing: E.out,
            }),
          }}
        >
          <Logo size={68} />
          <span
            style={{
              fontFamily: F.sans,
              fontSize: 62,
              fontWeight: 600,
              letterSpacing: '-0.02em',
              color: C.text,
            }}
          >
            GameScrcpy
          </span>
        </div>

        <div style={{display: 'flex', gap: 14, marginTop: 38}}>
          {['v1.3.4', 'Android 5.0+', 'Apache-2.0', '18 种映射参数'].map((label, i) => {
            const s = 82 + i * 7;
            return (
              <div
                key={label}
                style={{
                  opacity: interpolate(frame, [s, s + 18], [0, 1], {...clamp, easing: E.out}),
                  translate: interpolate(frame, [s, s + 22], ['0px 12px', '0px 0px'], {
                    ...clamp,
                    easing: E.out,
                  }),
                }}
              >
                <Chip>{label}</Chip>
              </div>
            );
          })}
        </div>

        <div
          style={{
            marginTop: 46,
            fontFamily: F.mono,
            fontSize: 20,
            letterSpacing: '0.12em',
            color: C.dim,
            opacity: interpolate(frame, [100, 120], [0, 0.9], {...clamp, easing: E.out}),
          }}
        >
          github.com/NuoRan/GameScrcpy
        </div>
      </div>

      {/* final fade to black */}
      <AbsoluteFill
        style={{
          backgroundColor: '#000',
          opacity: interpolate(frame, [148, 170], [0, 1], {...clamp, easing: E.linear}),
        }}
      />
    </AbsoluteFill>
  );
};
