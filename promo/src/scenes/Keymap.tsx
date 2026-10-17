import {AbsoluteFill, interpolate, useCurrentFrame} from 'remotion';
import {Backdrop, Bullet, Headline, Kicker} from '../components/base';
import {GameHud, WindowFrame} from '../components/mocks';
import {C, E, F, SAFE, clamp} from '../theme';

/* 横屏镜像窗口（键位映射编辑器实际工作在 16:9 画面上） */
const W = {w: 940, h: 564, x: 838, y: 296};
const CW = W.w;
const CH = W.h - 44;

const KEY = 54;
const KEY_GAP = 8;

const keys = [
  {label: 'W', col: 1, row: 0, start: 42},
  {label: 'A', col: 0, row: 1, start: 62},
  {label: 'S', col: 1, row: 1, start: 82},
  {label: 'D', col: 2, row: 1, start: 102},
];

const LOOK = {x: 640, y: 56, w: 232, h: 250};

export const Keymap = () => {
  const frame = useCurrentFrame();

  // steering stick：连续绕圈，慢到能看清
  const theta = (frame * 2.1 + 40) * (Math.PI / 180);
  const wheel = {cx: 100, cy: 100, r: 62};
  const knob = {
    x: wheel.cx + Math.cos(theta) * wheel.r,
    y: wheel.cy + Math.sin(theta) * wheel.r,
  };

  // freeLook：在选区里做李萨如漂移
  const t = frame / 30;
  const look = {
    x: LOOK.x + LOOK.w / 2 + Math.sin(t * 1.15) * 58,
    y: LOOK.y + LOOK.h / 2 + Math.sin(t * 2.3) * 72,
  };

  // 连发节奏：亮 76 帧、歇 24 帧
  const fp = frame - 132;
  const firePhase = fp >= 0 ? fp % 100 : -1;
  const fireDown = firePhase >= 0 && firePhase < 76;
  const dotY = fireDown
    ? interpolate(firePhase, [0, 56, 76, 100], [0, 62, 62, 0], {...clamp, easing: E.inOut})
    : 0;

  // drag 节点：沿四个路点往复滑行
  const WP = [[300, 112], [396, 88], [492, 114], [578, 88]];
  const du = (frame % 96) / 96;
  const seg = Math.min(WP.length - 2, Math.floor(du * (WP.length - 1)));
  const lu = du * (WP.length - 1) - seg;
  const dragPt = [
    WP[seg][0] + (WP[seg + 1][0] - WP[seg][0]) * lu,
    WP[seg][1] + (WP[seg + 1][1] - WP[seg][1]) * lu,
  ];

  return (
    <AbsoluteFill style={{padding: SAFE}}>
      <Backdrop drift={-10} />

      {/* ── copy ─────────────────────────────────────────────────── */}
      <div
        style={{
          position: 'absolute',
          left: 140,
          top: 240,
          width: 660,
          display: 'flex',
          flexDirection: 'column',
        }}
      >
        <Kicker index="02" label="游戏键位映射" from={4} />
        <div style={{marginTop: 30}}>
          <Headline lines={['端游的操作手感，', '原样搬进手机。']} from={16} size={68} gap={4} />
        </div>

        <div style={{display: 'flex', flexDirection: 'column', gap: 18, marginTop: 52}}>
          <Bullet from={48} size={28}>
            拖拽式可视化编辑器，所见即所得
          </Bullet>
          <Bullet from={58} size={28}>
            click · steerWheel · drag · freeLook · script
          </Bullet>
          <Bullet from={68} size={28}>
            组合键 Shift + G / Ctrl + A
          </Bullet>
          <Bullet from={78} size={28}>
            辅助压枪：16 项参数，引擎原生实现
          </Bullet>
        </div>
      </div>

      {/* ── 横屏编辑器窗口 ───────────────────────────────────────── */}
      <div
        style={{
          position: 'absolute',
          left: W.x,
          top: W.y,
          opacity: interpolate(frame, [10, 30], [0, 1], {...clamp, easing: E.out}),
          translate: interpolate(frame, [10, 40], ['40px 20px', '0px 0px'], {
            ...clamp,
            easing: E.out,
          }),
        }}
      >
        <WindowFrame
          width={W.w}
          height={W.h}
          title="GameScrcpy — 键位映射 · 1920 × 1080"
          accent
          right={
            <span
              style={{
                fontFamily: F.mono,
                fontSize: 15,
                letterSpacing: '0.08em',
                color: C.accentSoft,
                opacity: 0.9,
              }}
            >
              ● 编辑中
            </span>
          }
        >
          <GameHud w={CW} h={CH} detail={0.85} />

          {/* 左侧节点面板 */}
          <div
            style={{
              position: 'absolute',
              left: 0,
              top: 0,
              width: 200,
              height: CH,
              backgroundColor: 'rgba(9,9,11,0.62)',
              borderRight: `1px solid rgba(39,39,42,0.9)`,
              padding: '14px 14px',
              opacity: interpolate(frame, [24, 44], [0, 1], {...clamp, easing: E.out}),
            }}
          >
            <div
              style={{
                fontFamily: F.mono,
                fontSize: 11.5,
                letterSpacing: '0.16em',
                color: C.dim,
                marginBottom: 12,
              }}
            >
              NODES
            </div>
            {['click', 'steerWheel', 'drag', 'freeLook', 'script', 'recoilAssist'].map((n, i) => {
              const s0 = 30 + i * 8;
              const live = n === 'steerWheel' || n === 'freeLook' || n === 'recoilAssist';
              const blink = live ? 0.55 + 0.45 * Math.sin(frame / 9 + i) : 0.35;
              return (
                <div
                  key={n}
                  style={{
                    display: 'flex',
                    alignItems: 'center',
                    gap: 9,
                    height: 30,
                    opacity:
                      interpolate(frame, [s0, s0 + 14], [0, 1], {...clamp, easing: E.out}) * 0.95,
                  }}
                >
                  <div
                    style={{
                      width: 5,
                      height: 5,
                      borderRadius: 99,
                      backgroundColor: live ? C.accentSoft : 'rgba(113,113,122,0.7)',
                      opacity: blink,
                    }}
                  />
                  <span
                    style={{
                      fontFamily: F.mono,
                      fontSize: 13,
                      letterSpacing: '0.02em',
                      color: live ? C.body : C.muted,
                    }}
                  >
                    {n}
                  </span>
                </div>
              );
            })}
            <div
              style={{
                marginTop: 16,
                paddingTop: 12,
                borderTop: '1px solid rgba(39,39,42,0.9)',
                fontFamily: F.mono,
                fontSize: 11.5,
                letterSpacing: '0.06em',
                color: C.ok,
                opacity: interpolate(frame, [86, 104], [0, 0.85], clamp),
              }}
            >
              ● 已保存
            </div>
          </div>

          {/* EDIT MODE 角标 */}
          <div
            style={{
              position: 'absolute',
              left: 216,
              top: 18,
              fontFamily: F.mono,
              fontSize: 13,
              letterSpacing: '0.12em',
              color: 'rgba(161,161,170,0.7)',
              opacity: interpolate(frame, [36, 56], [0, 1], {...clamp, easing: E.out}),
            }}
          >
            EDIT MODE
          </div>

          {/* click 节点 */}
          <div style={{position: 'absolute', left: 214, top: 128, width: 48, height: 48}}>
            {[0, 1].map((k) => {
              const p = ((((frame - 88) / 46 + k * 0.5) % 1) + 1) % 1;
              const on = interpolate(frame, [88, 104], [0, 1], clamp);
              return (
                <div
                  key={k}
                  style={{
                    position: 'absolute',
                    left: 24 - (24 + p * 22) / 2,
                    top: 24 - (24 + p * 22) / 2,
                    width: 24 + p * 22,
                    height: 24 + p * 22,
                    borderRadius: 99,
                    border: `1px solid rgba(129,140,248,${0.55 * (1 - p) * on})`,
                  }}
                />
              );
            })}
            <div
              style={{
                position: 'absolute',
                left: 16,
                top: 16,
                width: 16,
                height: 16,
                borderRadius: 99,
                border: `1px solid ${C.accentSoft}`,
                backgroundColor: 'rgba(99,102,241,0.22)',
                opacity: interpolate(frame, [88, 104], [0, 1], clamp),
              }}
            />
            <div
              style={{
                position: 'absolute',
                left: 50,
                top: 15,
                fontFamily: F.mono,
                fontSize: 13,
                letterSpacing: '0.08em',
                color: C.accentSoft,
                opacity: interpolate(frame, [92, 108], [0, 0.9], clamp),
              }}
            >
              click
            </div>
          </div>

          {/* drag 节点 */}
          <svg
            width={300}
            height={130}
            style={{
              position: 'absolute',
              left: 290,
              top: 46,
              overflow: 'visible',
              opacity: interpolate(frame, [100, 118], [0, 1], {...clamp, easing: E.out}),
            }}
          >
            <polyline
              points={WP.map((p) => [p[0] - 290, p[1] - 46].join(',')).join(' ')}
              fill="none"
              stroke="rgba(129,140,248,0.4)"
              strokeWidth={1.4}
              strokeDasharray="4 5"
            />
            <circle cx={dragPt[0] - 290} cy={dragPt[1] - 46} r={10} fill="rgba(99,102,241,0.18)" />
            <circle cx={dragPt[0] - 290} cy={dragPt[1] - 46} r={4.6} fill={C.accentSoft} />
            <text
              x={292}
              y={24}
              fontFamily={F.mono}
              fontSize={13}
              fill={C.accentSoft}
              fillOpacity={0.9}
              letterSpacing="0.08em"
            >
              drag
            </text>
          </svg>

          {/* freeLook 选区 */}
          <div
            style={{
              position: 'absolute',
              left: LOOK.x,
              top: LOOK.y,
              width: LOOK.w,
              height: LOOK.h,
              borderRadius: 12,
              border: `1px dashed rgba(161,161,170,${interpolate(frame, [56, 76], [0, 0.35], clamp)})`,
              opacity: interpolate(frame, [56, 76], [0, 1], {...clamp, easing: E.out}),
            }}
          />
          <div
            style={{
              position: 'absolute',
              left: LOOK.x + 12,
              top: LOOK.y - 26,
              fontFamily: F.mono,
              fontSize: 13,
              letterSpacing: '0.08em',
              color: C.accentSoft,
              opacity: interpolate(frame, [66, 86], [0, 0.9], {...clamp, easing: E.out}),
            }}
          >
            freeLook
          </div>
          <div
            style={{
              position: 'absolute',
              left: look.x - 11,
              top: look.y - 11,
              width: 22,
              height: 22,
              borderRadius: 99,
              border: `1px solid ${C.accentSoft}`,
              backgroundColor: 'rgba(99,102,241,0.22)',
              boxShadow: '0 0 16px rgba(99,102,241,0.35)',
              opacity: interpolate(frame, [62, 82], [0, 1], {...clamp, easing: E.out}),
            }}
          />

          {/* steerWheel 摇杆 */}
          <svg
            width={200}
            height={200}
            style={{
              position: 'absolute',
              left: 208,
              top: 306,
              overflow: 'visible',
              opacity: interpolate(frame, [46, 70], [0, 1], {...clamp, easing: E.out}),
            }}
          >
            {[70, 44].map((r, i) => (
              <circle
                key={r}
                cx={wheel.cx}
                cy={wheel.cy}
                r={r}
                fill="none"
                stroke={i === 0 ? 'rgba(129,140,248,0.5)' : 'rgba(161,161,170,0.18)'}
                strokeWidth={i === 0 ? 1.5 : 1}
                strokeDasharray={i === 0 ? '6 8' : undefined}
              />
            ))}
            <line
              x1={wheel.cx}
              y1={wheel.cy}
              x2={knob.x}
              y2={knob.y}
              stroke="rgba(129,140,248,0.55)"
              strokeWidth={1.5}
            />
            <circle cx={knob.x} cy={knob.y} r={13} fill="rgba(99,102,241,0.28)" />
            <circle cx={knob.x} cy={knob.y} r={6.5} fill={C.accentSoft} />
            <text
              x={wheel.cx}
              y={wheel.cy + 4}
              textAnchor="middle"
              fontSize={13}
              fontFamily={F.mono}
              fill="rgba(161,161,170,0.8)"
              letterSpacing="0.06em"
            >
              steerWheel
            </text>
          </svg>

          {/* WASD */}
          {keys.map((k) => {
            const cycle = 84;
            const phase = (((frame - k.start) % cycle) + cycle) % cycle;
            const down = phase < 9 && frame > k.start;
            return (
              <div
                key={k.label}
                style={{
                  position: 'absolute',
                  left: 262 + k.col * (KEY + KEY_GAP),
                  top: 300 + k.row * (KEY + KEY_GAP),
                  width: KEY,
                  height: KEY,
                  borderRadius: 9,
                  border: `1px solid ${down ? 'rgba(99,102,241,0.8)' : C.lineStrong}`,
                  backgroundColor: down ? 'rgba(99,102,241,0.22)' : 'rgba(24,24,27,0.72)',
                  color: down ? C.text : C.muted,
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'center',
                  fontFamily: F.mono,
                  fontSize: 21,
                  translate: down ? '0px 3px' : '0px 0px',
                  boxShadow: down ? '0 0 22px rgba(99,102,241,0.3)' : 'none',
                  opacity: interpolate(frame, [26 + k.col * 4, 44 + k.col * 4], [0, 1], {
                    ...clamp,
                    easing: E.out,
                  }),
                }}
              >
                {k.label}
              </div>
            );
          })}

          {/* FIRE + recoilAssist */}
          <div
            style={{
              position: 'absolute',
              left: 800,
              top: 392,
              width: 72,
              height: 72,
              borderRadius: 99,
              border: `1px solid ${fireDown ? 'rgba(244,63,94,0.7)' : C.lineStrong}`,
              backgroundColor: fireDown ? 'rgba(244,63,94,0.18)' : 'rgba(24,24,27,0.7)',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              fontFamily: F.mono,
              fontSize: 15,
              letterSpacing: '0.1em',
              color: fireDown ? '#FDA4AF' : C.muted,
              opacity: interpolate(frame, [118, 136], [0, 1], {...clamp, easing: E.out}),
            }}
          >
            FIRE
          </div>

          <div
            style={{
              position: 'absolute',
              left: 834,
              top: 312,
              width: 2,
              height: 68,
              backgroundColor: 'rgba(161,161,170,0.18)',
              opacity: interpolate(frame, [124, 142], [0, 1], {...clamp, easing: E.out}),
            }}
          >
            <div
              style={{
                position: 'absolute',
                left: -4,
                top: dotY,
                width: 10,
                height: 10,
                borderRadius: 99,
                backgroundColor: '#FB7185',
              }}
            />
          </div>
          <div
            style={{
              position: 'absolute',
              left: 736,
              top: 340,
              fontFamily: F.mono,
              fontSize: 12.5,
              letterSpacing: '0.06em',
              color: '#FB7185',
              opacity: fireDown
                ? interpolate(frame, [136, 148], [0, 0.85], {...clamp, easing: E.out})
                : 0,
            }}
          >
            recoilAssist
          </div>
        </WindowFrame>
      </div>
    </AbsoluteFill>
  );
};
