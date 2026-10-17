import {AbsoluteFill, interpolate, useCurrentFrame} from 'remotion';
import {Backdrop, Headline, Kicker} from '../components/base';
import {C, E, F, clamp} from '../theme';

const CARDS = [
  {
    name: 'Adb',
    mode: 'scrcpy 原生注入',
    desc: '默认后端，USB 与 WiFi 均可',
    tag: '默认',
    hot: false,
  },
  {
    name: 'UHID',
    mode: '/dev/uhid 虚拟触摸屏',
    desc: '无线场景更稳，16 点触控',
    tag: '推荐 WiFi',
    hot: false,
  },
  {
    name: 'AOA',
    mode: 'USB OTG 直连注入',
    desc: '绕过系统层，链路最短',
    tag: '最低延迟',
    hot: true,
  },
  {
    name: 'ESP32',
    mode: 'USB HID 触摸屏',
    desc: 'QineTouch 外接硬件',
    tag: '外设',
    hot: false,
  },
];

const CARD = {w: 380, h: 300, gap: 36};
const startX = (1920 - (CARD.w * 4 + CARD.gap * 3)) / 2;

export const Backends = () => {
  const frame = useCurrentFrame();

  return (
    <AbsoluteFill>
      <Backdrop drift={10} />

      <div
        style={{
          position: 'absolute',
          left: 0,
          right: 0,
          top: 150,
          display: 'flex',
          flexDirection: 'column',
          alignItems: 'center',
        }}
      >
        <Kicker index="07" label="触控后端" from={4} />
        <div style={{marginTop: 30}}>
          <Headline lines={['四条注入链路，', '各取所需。']} from={14} size={72} gap={2} align="center" />
        </div>
      </div>

      <div
        style={{
          position: 'absolute',
          left: startX,
          top: 496,
          display: 'flex',
          gap: CARD.gap,
        }}
      >
        {CARDS.map((card, i) => {
          const s = 40 + i * 9;
          const selected = card.hot ? interpolate(frame, [118, 140], [0, 1], {...clamp, easing: E.out}) : 0;
          return (
            <div
              key={card.name}
              style={{
                width: CARD.w,
                height: CARD.h,
                borderRadius: 16,
                border: `1px solid ${
                  card.hot ? `rgba(99,102,241,${0.3 + selected * 0.45})` : C.line
                }`,
                backgroundColor: card.hot
                  ? `rgba(24,24,30,${0.85 + selected * 0.1})`
                  : 'rgba(20,20,24,0.72)',
                padding: '30px 30px 26px',
                display: 'flex',
                flexDirection: 'column',
                opacity: interpolate(frame, [s, s + 20], [0, 1], {...clamp, easing: E.out}),
                translate: interpolate(frame, [s, s + 26], ['0px 40px', '0px 0px'], {
                  ...clamp,
                  easing: E.out,
                }),
                boxShadow: card.hot
                  ? `0 30px 70px rgba(0,0,0,0.5), 0 0 ${60 * selected}px rgba(99,102,241,0.16)`
                  : '0 24px 60px rgba(0,0,0,0.35)',
                scale: card.hot ? interpolate(selected, [0, 1], [1, 1.02]) : 1,
                position: 'relative',
                overflow: 'hidden',
              }}
            >
              <div
                style={{
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'space-between',
                }}
              >
                <span
                  style={{
                    fontFamily: F.mono,
                    fontSize: 18,
                    color: C.dim,
                    letterSpacing: '0.14em',
                  }}
                >
                  0{i + 1}
                </span>
                <span
                  style={{
                    fontFamily: F.mono,
                    fontSize: 16,
                    letterSpacing: '0.06em',
                    padding: '5px 12px',
                    borderRadius: 999,
                    border: `1px solid ${card.hot ? 'rgba(99,102,241,0.45)' : C.line}`,
                    color: card.hot ? C.accentSoft : C.muted,
                    backgroundColor: card.hot ? 'rgba(99,102,241,0.10)' : 'transparent',
                  }}
                >
                  {card.tag}
                </span>
              </div>

              <div
                style={{
                  fontFamily: F.sans,
                  fontSize: 46,
                  fontWeight: 600,
                  letterSpacing: '-0.01em',
                  color: C.text,
                  marginTop: 30,
                }}
              >
                {card.name}
              </div>
              <div
                style={{
                  fontFamily: F.mono,
                  fontSize: 19,
                  color: C.accentSoft,
                  opacity: 0.85,
                  marginTop: 10,
                  letterSpacing: '0.02em',
                }}
              >
                {card.mode}
              </div>
              <div
                style={{
                  fontFamily: F.sans,
                  fontSize: 23,
                  color: C.muted,
                  marginTop: 14,
                  lineHeight: 1.5,
                }}
              >
                {card.desc}
              </div>

              {/* link pulse */}
              <div
                style={{
                  position: 'absolute',
                  left: 30,
                  right: 30,
                  bottom: 22,
                  height: 2,
                  backgroundColor: 'rgba(63,63,70,0.55)',
                  overflow: 'hidden',
                }}
              >
                <div
                  style={{
                    position: 'absolute',
                    top: 0,
                    bottom: 0,
                    width: 60,
                    left: `${interpolate((frame + i * 16) % 72, [0, 72], [-20, 110], E.linear)}%`,
                    background: `linear-gradient(90deg, transparent, ${
                      card.hot ? C.accentSoft : '#52525B'
                    }, transparent)`,
                    opacity: 0.9,
                  }}
                />
              </div>
            </div>
          );
        })}
      </div>

      <div
        style={{
          position: 'absolute',
          left: 0,
          right: 0,
          top: 850,
          textAlign: 'center',
          fontFamily: F.mono,
          fontSize: 21,
          letterSpacing: '0.1em',
          color: C.dim,
          opacity: interpolate(frame, [92, 112], [0, 1], {...clamp, easing: E.out}),
        }}
      >
        TouchRouter 统一路由 · 会话中可切换 · 按键 HID 优先
      </div>
    </AbsoluteFill>
  );
};
