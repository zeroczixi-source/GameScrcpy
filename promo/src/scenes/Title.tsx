import {AbsoluteFill, interpolate, useCurrentFrame} from 'remotion';
import {Backdrop, Chip, Logo} from '../components/base';
import {C, E, F, SAFE, clamp} from '../theme';

export const Title = () => {
  const frame = useCurrentFrame();

  return (
    <AbsoluteFill style={{padding: SAFE}}>
      <Backdrop drift={-14} />

      <div
        style={{
          position: 'absolute',
          inset: 0,
          display: 'flex',
          flexDirection: 'column',
          alignItems: 'center',
          justifyContent: 'center',
          scale: interpolate(frame, [0, 165], [1, 1.025], {easing: E.linear}),
        }}
      >
        <div
          style={{
            opacity: interpolate(frame, [4, 26], [0, 1], {...clamp, easing: E.out}),
            scale: interpolate(frame, [4, 34], [0.86, 1], {...clamp, easing: E.out}),
            marginBottom: 46,
          }}
        >
          <Logo size={112} />
        </div>

        <div style={{overflow: 'hidden', paddingBottom: '0.12em', marginBottom: '-0.06em'}}>
          <div
            style={{
              fontFamily: F.sans,
              fontSize: 132,
              fontWeight: 600,
              letterSpacing: interpolate(frame, [10, 62], ['0.12em', '-0.022em'], {
                ...clamp,
                easing: E.out,
              }),
              color: C.text,
              lineHeight: 1.05,
              translate: interpolate(frame, [10, 60], ['0px 92px', '0px 0px'], {
                ...clamp,
                easing: E.out,
              }),
              opacity: interpolate(frame, [10, 30], [0, 1], {...clamp}),
            }}
          >
            GameScrcpy
          </div>
        </div>

        <div
          style={{
            width: interpolate(frame, [42, 78], [0, 420], {...clamp, easing: E.out}),
            height: 2,
            backgroundColor: C.accent,
            marginTop: 40,
            opacity: 0.9,
          }}
        />

        <div
          style={{
            marginTop: 34,
            fontFamily: F.sans,
            fontSize: 46,
            letterSpacing: '0.10em',
            color: C.muted,
            opacity: interpolate(frame, [58, 80], [0, 1], {...clamp, easing: E.out}),
            translate: interpolate(frame, [58, 84], ['0px 18px', '0px 0px'], {
              ...clamp,
              easing: E.out,
            }),
          }}
        >
          Android 投屏 · 为游戏而生
        </div>

        <div style={{display: 'flex', gap: 16, marginTop: 62}}>
          {['v1.3.4', 'Windows 10 / 11', 'Android 5.0+', 'Apache-2.0'].map((label, i) => {
            const s = 76 + i * 8;
            return (
              <div
                key={label}
                style={{
                  opacity: interpolate(frame, [s, s + 18], [0, 1], {...clamp, easing: E.out}),
                  translate: interpolate(frame, [s, s + 22], ['0px 14px', '0px 0px'], {
                    ...clamp,
                    easing: E.out,
                  }),
                }}
              >
                <Chip tone={i === 0 ? 'accent' : 'default'}>{label}</Chip>
              </div>
            );
          })}
        </div>
      </div>
    </AbsoluteFill>
  );
};
