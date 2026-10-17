import {AbsoluteFill, interpolate, useCurrentFrame} from 'remotion';
import {Backdrop, Headline, Kicker} from '../components/base';
import {Counter, Cursor, Ripple, Spot} from '../components/kit';
import {AppWindow, Legend, SETTINGS_SCROLL, WIN, WipeLayer} from '../components/ui';
import {C, E, F, clamp} from '../theme';

const CARD_LEFT = 110;
const CARD_TOP = 300;

/** camera: [frame, scale, translateX, translateY] */
const CAM: number[][] = [
  [0, 0.86, 0, 26],
  [34, 0.9, 0, 0],
  [96, 0.92, -6, -8],
  [116, 0.875, 0, 0],
  [150, 0.905, -4, -4],
  [212, 0.905, -4, -4],
  [240, 0.9, -2, 0],
  [298, 0.885, 0, 0],
  [318, 0.905, -4, -4],
  [356, 0.9, -4, -4],
  [372, 0.855, -8, 10],
];

const cam = (frame: number) => {
  const track = (i: number) =>
    interpolate(
      frame,
      CAM.map((k) => k[0]),
      CAM.map((k) => k[i]),
      {...clamp, easing: E.inOut},
    );
  return {scale: track(1), tx: track(2), ty: track(3)};
};

const PAGE_LABEL: Record<string, string> = {
  home: '首页 · 快速连接',
  settings: '设置 · 视频参数 / 通道 / 触控',
  terminal: '终端 · ADB 命令直通',
};

export const UiTour = () => {
  const frame = useCurrentFrame();
  const {scale, tx, ty} = cam(frame);

  const wipe1 = interpolate(frame, [104, 122], [0, 1], {...clamp, easing: E.inOut});
  const wipe2 = interpolate(frame, [300, 318], [0, 1], {...clamp, easing: E.inOut});
  const scroll = interpolate(frame, [170, 206], [0, SETTINGS_SCROLL], {...clamp, easing: E.inOut});

  const page = frame < 112 ? 'home' : frame < 308 ? 'settings' : 'terminal';
  const inSpan = (f: number) => frame >= f;

  return (
    <AbsoluteFill>
      <Backdrop drift={-14} />

      <div style={{position: 'absolute', left: 140, top: 118}}>
        <Kicker index="01" label="真机界面" from={2} />
      </div>
      <div style={{position: 'absolute', left: 140, top: 172}}>
        <Headline lines={['真机界面，一次看全。']} from={8} size={56} />
      </div>

      {/* ---------- the real app window ---------- */}
      <div
        style={{
          position: 'absolute',
          left: CARD_LEFT,
          top: CARD_TOP,
          width: WIN.w,
          height: WIN.h,
          scale: `${scale}`,
          translate: `${tx}px ${ty}px`,
          opacity: interpolate(frame, [0, 16], [0, 1], {...clamp, easing: E.out}),
          transformOrigin: 'center center',
        }}
      >
        <AppWindow page="home" />
        <WipeLayer reveal={wipe1}>
          <AppWindow page="settings" scroll={scroll} />
        </WipeLayer>
        <WipeLayer reveal={wipe2}>
          <AppWindow page="terminal" />
        </WipeLayer>

        {/* --- home: connect row + refresh --- */}
        {page === 'home' ? (
          <>
            {inSpan(34) && frame < 110 ? (
              <>
                <Spot x={155} y={160} w={543} h={60} start={34} index={1} radius={10} />
                <Spot x={702} y={160} w={543} h={60} start={40} radius={10} />
                <Spot x={1173} y={274} w={72} h={46} start={62} index={2} radius={10} />
              </>
            ) : null}
            {/* refresh feedback: a hairline that sweeps across the list head */}
            {frame > 90 && frame < 116 ? (
              <div
                style={{
                  position: 'absolute',
                  left: 161 + interpolate(frame, [90, 114], [0, 990], clamp),
                  top: 328,
                  width: 88,
                  height: 363,
                  borderRadius: 8,
                  background:
                    'linear-gradient(90deg, rgba(129,140,248,0) 0%, rgba(129,140,248,0.14) 50%, rgba(129,140,248,0) 100%)',
                  opacity: interpolate(frame, [90, 96, 114], [0, 0.9, 0], clamp),
                  pointerEvents: 'none',
                }}
              />
            ) : null}
          </>
        ) : null}

        {/* --- settings: top sections --- */}
        {page === 'settings' ? (
          <>
            {frame > 124 && frame < 174 ? (
              <>
                <Spot x={140} y={50} w={1120} h={152} start={126} index={1} radius={14} />
                <Spot x={140} y={248} w={1120} h={248} start={140} index={2} radius={14} />
              </>
            ) : null}
            {frame > 208 && frame < 302 ? (
              <>
                <Spot
                  x={140}
                  y={550 - scroll}
                  w={1120}
                  h={300}
                  start={210}
                  index={1}
                  radius={14}
                />
                <Spot
                  x={140}
                  y={892 - scroll}
                  w={1120}
                  h={160}
                  start={220}
                  index={2}
                  radius={14}
                />
                <Spot
                  x={140}
                  y={1091 - scroll}
                  w={1120}
                  h={112}
                  start={232}
                  index={3}
                  radius={14}
                />
              </>
            ) : null}
          </>
        ) : null}

        {/* --- terminal --- */}
        {page === 'terminal' ? (
          <>
            <Spot x={140} y={40} w={1120} h={200} start={322} index={1} radius={14} />
            <Spot x={1096} y={100} w={80} h={50} start={334} radius={10} />
          </>
        ) : null}

        {/* --- pointer --- */}
        {frame > 58 && frame < 112 ? (
          <Cursor
            start={62}
            seg={13}
            waypoints={[
              [1250, 690],
              [1170, 560],
              [1209, 297],
            ]}
            clicks={[90]}
          />
        ) : null}
        <Ripple x={1209} y={297} start={90} />
        {frame > 322 && frame < 372 ? (
          <Cursor
            start={326}
            seg={13}
            waypoints={[
              [1240, 620],
              [1180, 300],
              [1136, 125],
            ]}
            clicks={[352]}
          />
        ) : null}
        <Ripple x={1136} y={125} start={352} />
      </div>

      {/* ---------- legend ---------- */}
      <div style={{position: 'absolute', left: 1372, top: 352}}>
        {page === 'home' ? (
          <Legend
            start={44}
            out={104}
            items={[
              {n: 1, title: 'USB / WiFi 双通道', detail: '免安装，插线即投；同一网段免数据线'},
              {n: 2, title: '设备自动发现', detail: '双击列表直接投屏，一键手动刷新'},
              {n: 3, title: '毫秒级链路', detail: 'KCP 可靠传输与裸 UDP 双模可切'},
            ]}
          />
        ) : null}
        {page === 'settings' && frame < 180 ? (
          <Legend
            start={128}
            out={168}
            items={[
              {n: 1, title: '帧率与码率自由调', detail: '0–999 FPS，自适应码率 ABR 兜底'},
              {n: 2, title: '显示与窗口', detail: '无边框、工具栏、反向连接、FPS 角标'},
              {n: 3, title: '通道分离', detail: '视频 / 音频 / 控制 / 辅助 四路独立'},
            ]}
          />
        ) : null}
        {page === 'settings' && frame >= 172 ? (
          <Legend
            start={198}
            out={302}
            items={[
              {n: 1, title: '四路通道独立开关', detail: '丢了辅助通道，画面与触控照常工作'},
              {n: 2, title: '四种触控后端', detail: 'ADB / UHID / AOA / ESP32 一键切换'},
              {n: 3, title: '深色主题', detail: '强调色可换，长时间使用不刺眼'},
            ]}
          />
        ) : null}
        {page === 'terminal' ? (
          <Legend
            start={324}
            items={[
              {n: 1, title: '内置 ADB 终端', detail: '常用命令现成可用，不必另开命令行'},
              {n: 2, title: '输出留档', detail: '命令与脚本共用同一 adb 通道'},
            ]}
          />
        ) : null}
      </div>

      {/* ---------- caption under the window ---------- */}
      <div
        style={{
          position: 'absolute',
          left: 118,
          top: 1012,
          fontFamily: F.mono,
          fontSize: 18,
          letterSpacing: '0.1em',
          color: C.dim,
          opacity: interpolate(frame, [24, 44], [0, 0.9], {...clamp, easing: E.out}),
        }}
      >
        {PAGE_LABEL[page]}
      </div>
      <div
        style={{
          position: 'absolute',
          left: 1560,
          top: 1012,
          fontFamily: F.mono,
          fontSize: 18,
          letterSpacing: '0.1em',
          color: C.dim,
          opacity: interpolate(frame, [30, 50], [0, 0.8], {...clamp, easing: E.out}),
        }}
      >
        v1.3.4 · Windows
      </div>
    </AbsoluteFill>
  );
};
