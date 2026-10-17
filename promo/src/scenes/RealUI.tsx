import {AbsoluteFill, Img, interpolate, staticFile, useCurrentFrame} from 'remotion';
import {Backdrop, Headline, Kicker} from '../components/base';
import {C, E, F, clamp} from '../theme';

const Shot = ({
  src,
  width,
  height,
  x,
  y,
  from,
  scaleTo = 1,
}: {
  src: string;
  width: number;
  height: number;
  x: number;
  y: number;
  from: number;
  scaleTo?: number;
}) => {
  const frame = useCurrentFrame();
  const push = interpolate(frame, [from, from + 195], [1.045, 1], {
    easing: E.linear,
  });
  return (
    <div
      style={{
        position: 'absolute',
        left: x,
        top: y,
        width,
        height,
        borderRadius: 12,
        overflow: 'hidden',
        border: `1px solid ${C.lineStrong}`,
        boxShadow: '0 40px 90px rgba(0,0,0,0.55)',
        opacity: interpolate(frame, [from, from + 22], [0, 1], {...clamp, easing: E.out}),
        translate: interpolate(frame, [from, from + 30], ['0px 30px', '0px 0px'], {
          ...clamp,
          easing: E.out,
        }),
      }}
    >
      <Img
        src={staticFile(src)}
        style={{
          width: '100%',
          height: '100%',
          objectFit: 'cover',
          scale: push * scaleTo,
        }}
      />
    </div>
  );
};

export const RealUI = () => {
  const frame = useCurrentFrame();

  return (
    <AbsoluteFill>
      <Backdrop drift={-12} />

      <div style={{position: 'absolute', left: 140, top: 140}}>
        <Kicker index="06" label="实机界面" from={2} />
      </div>
      <div style={{position: 'absolute', left: 140, top: 196}}>
        <Headline lines={['不是效果图，是实际界面。']} from={10} size={58} />
      </div>

      <Shot
        src="screenshot_main.png"
        width={660}
        height={487}
        x={140}
        y={330}
        from={24}
        scaleTo={1}
      />
      <Shot
        src="screenshot_keymap.png"
        width={880}
        height={405}
        x={856}
        y={366}
        from={34}
        scaleTo={1}
      />

      <div
        style={{
          position: 'absolute',
          left: 140,
          top: 836,
          fontFamily: F.mono,
          fontSize: 19,
          letterSpacing: '0.08em',
          color: C.dim,
          opacity: interpolate(frame, [50, 70], [0, 1], {...clamp, easing: E.out}),
        }}
      >
        主窗口 · 设备发现 / 投屏 / 性能监控
      </div>
      <div
        style={{
          position: 'absolute',
          left: 856,
          top: 792,
          fontFamily: F.mono,
          fontSize: 19,
          letterSpacing: '0.08em',
          color: C.dim,
          opacity: interpolate(frame, [58, 78], [0, 1], {...clamp, easing: E.out}),
        }}
      >
        键位映射编辑器 · 拖拽即改，JSON 热加载
      </div>
    </AbsoluteFill>
  );
};
