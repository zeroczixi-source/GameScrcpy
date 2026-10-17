import {AbsoluteFill, interpolate, useCurrentFrame} from 'remotion';
import {Backdrop, Bullet, Chip, Headline, Kicker} from '../components/base';
import {GameHud, PhoneFrame, WindowFrame} from '../components/mocks';
import {Counter, Sparkline} from '../components/kit';
import {C, E, F, SAFE, clamp} from '../theme';

const PHONE = {w: 176, h: 336, x: 872, y: 372};
const WIN = {w: 620, h: 384, x: 1148, y: 344};

export const Stream = () => {
  const frame = useCurrentFrame();
  const framesSeen = Math.floor(Math.max(0, frame - 40) / 6);
  // live latency oscilloscope: the trace scrolls, so it never freezes
  const tracePts: number[][] = [];
  for (let i = 0; i <= 40; i++) {
    const t = frame * 0.42 + i * 0.85;
    const v =
      0.52 +
      0.2 * Math.sin(t * 0.37) +
      0.12 * Math.sin(t * 0.93 + 1.3) +
      0.06 * Math.sin(t * 2.11 + 2.4);
    tracePts.push([(i / 40) * 196, 40 - Math.max(0.08, Math.min(0.94, v)) * 34]);
  }
  const liveMs = 18 + (1 - (tracePts[tracePts.length - 1][1] - 6) / 34) * 26;
  const arcPhase = (o: number) => {
    const t = (((frame - 44 + o) % 36) + 36) % 36;
    return t / 36;
  };

  return (
    <AbsoluteFill style={{padding: SAFE}}>
      <Backdrop drift={12} />

      {/* ── copy column ───────────────────────────────────────────── */}
      <div
        style={{
          position: 'absolute',
          left: 140,
          top: 206,
          width: 660,
          display: 'flex',
          flexDirection: 'column',
        }}
      >
        <Kicker index="04" label="高清低延迟投屏" from={6} />

        <div style={{marginTop: 34}}>
          <Headline lines={['毫秒之间，', '画面已经到位。']} from={18} size={80} gap={4} />
        </div>

        {/* stat */}
        <div
          style={{
            marginTop: 56,
            display: 'flex',
            alignItems: 'flex-end',
            gap: 26,
            opacity: interpolate(frame, [46, 66], [0, 1], {...clamp, easing: E.out}),
            translate: interpolate(frame, [46, 72], ['0px 20px', '0px 0px'], {
              ...clamp,
              easing: E.out,
            }),
          }}
        >
          <div style={{display: 'flex', alignItems: 'baseline', gap: 8}}>
            <span
              style={{
                fontFamily: F.mono,
                fontSize: 96,
                fontWeight: 600,
                letterSpacing: '-0.03em',
                color: C.text,
              }}
            >
              &lt;50
            </span>
            <span style={{fontFamily: F.mono, fontSize: 42, color: C.muted}}>ms</span>
          </div>
          <div
            style={{
              height: 56,
              width: 1,
              backgroundColor: C.lineStrong,
              marginBottom: 10,
            }}
          />
          <div style={{display: 'flex', flexDirection: 'column', gap: 6, paddingBottom: 12}}>
            <span style={{fontFamily: F.sans, fontSize: 27, color: C.body}}>端到端延迟</span>
            <span
              style={{fontFamily: F.mono, fontSize: 20, color: C.dim, letterSpacing: '0.1em'}}
            >
              0 – 999 FPS 可调
            </span>
          </div>
        </div>

        <div style={{display: 'flex', flexDirection: 'column', gap: 18, marginTop: 52}}>
          <Bullet from={64}>H.264 / H.265 硬件解码</Bullet>
          <Bullet from={74}>USB 与 WiFi 双通道</Bullet>
          <Bullet from={84}>
            FEC 前向纠错 · 组内丢包可恢复
          </Bullet>
        </div>
      </div>

      {/* ── mirroring diagram ─────────────────────────────────────── */}
      <div
        style={{
          position: 'absolute',
          left: PHONE.x,
          top: PHONE.y,
          opacity: interpolate(frame, [14, 34], [0, 1], {...clamp, easing: E.out}),
          translate: interpolate(frame, [14, 44], ['-46px 24px', '0px 0px'], {
            ...clamp,
            easing: E.out,
          }),
        }}
      >
        <PhoneFrame width={PHONE.w} height={PHONE.h} accent>
          <GameHud w={PHONE.w} h={PHONE.h} detail={0.55} />
        </PhoneFrame>
      </div>

      {/* signal arcs between phone and window */}
      <svg
        width={150}
        height={200}
        style={{position: 'absolute', left: 1042, top: 446, overflow: 'visible'}}
      >
        {[0, 12, 24].map((offset, i) => {
          const t = arcPhase(offset);
          return (
            <path
              key={i}
              d={`M 6 ${100 - 30} A 30 30 0 0 1 6 ${100 + 30}`}
              stroke={C.accentSoft}
              strokeWidth={2}
              fill="none"
              style={{
                opacity: interpolate(t, [0, 0.25, 1], [0, 0.75, 0], clamp) * 0.9,
                scale: interpolate(t, [0, 1], [0.55, 1.5], {easing: E.out}),
                transformOrigin: '6px 100px',
              }}
            />
          );
        })}
      </svg>

      <div
        style={{
          position: 'absolute',
          left: WIN.x,
          top: WIN.y,
          opacity: interpolate(frame, [22, 42], [0, 1], {...clamp, easing: E.out}),
          translate: interpolate(frame, [22, 52], ['46px -16px', '0px 0px'], {
            ...clamp,
            easing: E.out,
          }),
        }}
      >
        <WindowFrame
          width={WIN.w}
          height={WIN.h}
          title="GameScrcpy — Pixel 8  ·  1920×1080"
          accent
          right={
            <span
              style={{
                fontFamily: F.mono,
                fontSize: 16,
                letterSpacing: '0.08em',
                color: C.ok,
                opacity: 0.9,
              }}
            >
              ● LIVE
            </span>
          }
        >
          <GameHud w={WIN.w} h={WIN.h - 44} detail={1} />
          <div
            style={{
              position: 'absolute',
              right: 18,
              top: 14,
              fontFamily: F.mono,
              fontSize: 17,
              letterSpacing: '0.08em',
              color: C.muted,
              backgroundColor: 'rgba(9,9,11,0.6)',
              border: `1px solid ${C.line}`,
              borderRadius: 6,
              padding: '4px 10px',
            }}
          >
            FRAME {String(framesSeen).padStart(4, '0')}
          </div>
          {/* live latency readout */}
          <div
            style={{
              position: 'absolute',
              left: 16,
              bottom: 12,
              width: 224,
              padding: '9px 12px 10px',
              borderRadius: 8,
              border: `1px solid ${C.line}`,
              backgroundColor: 'rgba(9,9,11,0.72)',
              opacity: interpolate(frame, [62, 84], [0, 1], {...clamp, easing: E.out}),
            }}
          >
            <div
              style={{
                display: 'flex',
                alignItems: 'baseline',
                justifyContent: 'space-between',
                marginBottom: 4,
              }}
            >
              <span
                style={{
                  fontFamily: F.mono,
                  fontSize: 13,
                  letterSpacing: '0.16em',
                  color: C.dim,
                }}
              >
                LATENCY
              </span>
              <Counter
                to={liveMs}
                from={liveMs}
                start={0}
                dur={1}
                digits={1}
                suffix=" ms"
                size={16}
              />
            </div>
            <Sparkline
              points={tracePts}
              w={196}
              h={40}
              start={0}
              dur={1}
              baseline={false}
              head
            />
          </div>
        </WindowFrame>
      </div>

      {/* transport chips under the window */}
      <div
        style={{
          position: 'absolute',
          left: WIN.x,
          top: WIN.y + WIN.h + 30,
          width: WIN.w,
          display: 'flex',
          gap: 12,
          alignItems: 'center',
        }}
      >
        {['KCP + 裸 UDP', 'FEC 10:1', '自适应码率 ABR'].map((label, i) => {
          const s = 64 + i * 10;
          return (
            <div
              key={label}
              style={{
                opacity: interpolate(frame, [s, s + 18], [0, 1], {...clamp, easing: E.out}),
                translate: interpolate(frame, [s, s + 24], ['0px 12px', '0px 0px'], {
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
    </AbsoluteFill>
  );
};
