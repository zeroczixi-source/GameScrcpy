import {AbsoluteFill, interpolate, useCurrentFrame} from 'remotion';
import {Backdrop, Headline, Kicker} from '../components/base';
import {C, E, F, SAFE, clamp} from '../theme';

/* ── 六类节点 ─────────────────────────────────────────────────────── */
const NODES = [
  {name: 'click', cn: '点按 / 长按', desc: '按键映射到屏幕坐标，单击或按住'},
  {name: 'steerWheel', cn: '方向轮盘', desc: '摇杆区域，摇杆向量直接驱动移动'},
  {name: 'drag', cn: '拖拽拖镜', desc: '起点终点 + 时长，按住期间持续位移'},
  {name: 'freeLook', cn: '自由视角', desc: '鼠标位移映射为视角旋转，灵敏度可调'},
  {name: 'script', cn: '脚本节点', desc: '绑定 QuickJS 脚本，一键连招'},
  {name: 'recoilAssist', cn: '辅助压枪', desc: '引擎原生实现，16 项参数'},
];

const ROW = 76;
const ROW_TOP = 372;
const ROW_START = 40;

/* ── 每个节点对应的小图标，各自带一点微动效 ───────────────────────── */
const Glyph = ({kind}: {kind: string}) => {
  const frame = useCurrentFrame();
  const t = frame / 30;
  const a = C.accentSoft;
  const stroke = {fill: 'none', stroke: a, strokeWidth: 1.6, strokeLinecap: 'round'} as const;

  return (
    <svg width={48} height={48} viewBox="0 0 48 48" style={{overflow: 'visible'}}>
      <rect
        x={0.5}
        y={0.5}
        width={47}
        height={47}
        rx={11}
        fill="rgba(99,102,241,0.08)"
        stroke="rgba(99,102,241,0.32)"
      />
      {kind === 'click' && (
        <>
          <circle cx={24} cy={24} r={7} {...stroke} />
          {[0, 1].map((k) => {
            const p = ((t * 0.9 + k * 0.5) % 1);
            return (
              <circle
                key={k}
                cx={24}
                cy={24}
                r={7 + p * 12}
                fill="none"
                stroke={a}
                strokeWidth={1.2}
                opacity={0.7 * (1 - p)}
              />
            );
          })}
          <circle cx={24} cy={24} r={2.4} fill={a} />
        </>
      )}
      {kind === 'steerWheel' && (
        <>
          <circle cx={24} cy={24} r={14} {...stroke} strokeOpacity={0.45} />
          <circle cx={24} cy={24} r={6} fill="rgba(99,102,241,0.25)" />
          <line
            x1={24}
            y1={24}
            x2={24 + Math.cos(t * 1.4) * 13}
            y2={24 + Math.sin(t * 1.4) * 13}
            {...stroke}
          />
          <circle cx={24 + Math.cos(t * 1.4) * 13} cy={24 + Math.sin(t * 1.4) * 13} r={3} fill={a} />
        </>
      )}
      {kind === 'drag' && (
        <>
          <path d="M 11 34 Q 24 12 37 34" {...stroke} strokeDasharray="3 4" strokeOpacity={0.5} />
          <circle cx={11} cy={34} r={3} fill={a} />
          <circle cx={37} cy={34} r={3} fill={a} />
          <circle
            cx={11 + (((t * 0.55) % 1) * 26)}
            cy={34 - Math.sin(((t * 0.55) % 1) * Math.PI) * 12}
            r={3.4}
            fill={C.text}
          />
        </>
      )}
      {kind === 'freeLook' && (
        <>
          <path d="M 13 24 Q 24 20 35 24" {...stroke} strokeOpacity={0.35} />
          <line x1={24} y1={11} x2={24} y2={37} stroke={a} strokeWidth={1} strokeOpacity={0.4} />
          <line x1={11} y1={24} x2={37} y2={24} stroke={a} strokeWidth={1} strokeOpacity={0.4} />
          <circle cx={24 + Math.sin(t * 1.1) * 9} cy={24 + Math.sin(t * 2.2) * 6} r={3.4} fill={a} />
        </>
      )}
      {kind === 'script' && (
        <>
          <text
            x={24}
            y={30}
            textAnchor="middle"
            fontFamily={F.mono}
            fontSize={17}
            fill={a}
            opacity={0.75 + 0.25 * Math.sin(t * 3)}
          >
            {'</>'}
          </text>
          <line x1={13} y1={37} x2={13 + ((t * 22) % 22)} y2={37} stroke={a} strokeWidth={1.4} />
        </>
      )}
      {kind === 'recoilAssist' && (
        <>
          <line x1={24} y1={10} x2={24} y2={38} stroke="rgba(161,161,170,0.28)" strokeWidth={1} />
          {[0, 1, 2].map((k) => (
            <line
              key={k}
              x1={17}
              y1={14 + k * 10}
              x2={31}
              y2={14 + k * 10}
              stroke="#FB7185"
              strokeWidth={1.4}
              opacity={0.75 - k * 0.18}
            />
          ))}
          <path d="M 14 15 L 24 30 L 34 15" {...stroke} />
          <circle cx={24} cy={30 + Math.sin(t * 2) * 1.6} r={2.6} fill={a} />
        </>
      )}
    </svg>
  );
};

/* ── 压枪曲线 ─────────────────────────────────────────────────────── */
const PLOT = {x: 28, y: 104, w: 784, h: 148};

export const KeymapDeep = () => {
  const frame = useCurrentFrame();

  const mid = PLOT.y + PLOT.h / 2;
  // 无辅助：准心随连发一路向上漂
  const drift = (x: number) => mid - 58 * Math.pow(x, 1.5) - 3 * Math.sin(x * 14);
  // 有辅助：把漂移压回去
  const fixed = (x: number) =>
    mid - 58 * Math.pow(x, 1.5) * (1 - Math.min(1, x * 2.4)) - 2.2 * Math.sin(x * 11);

  const sample = (fn: (x: number) => number) => {
    const pts: string[] = [];
    for (let i = 0; i <= 40; i++) {
      const u = i / 40;
      pts.push(`${(PLOT.x + u * PLOT.w).toFixed(1)},${fn(u).toFixed(1)}`);
    }
    return pts.join(' ');
  };

  const curveIn = interpolate(frame, [120, 168], [0, 1], {...clamp, easing: E.inOut});
  const headX = PLOT.x + curveIn * PLOT.w;
  const headY = fixed(curveIn);

  const ticks = 16;

  return (
    <AbsoluteFill style={{padding: SAFE}}>
      <Backdrop drift={9} />

      {/* ── 标题 ───────────────────────────────────────────────── */}
      <div style={{position: 'absolute', left: 140, top: 148, width: 900}}>
        <Kicker index="03" label="键位映射 · 细节" from={2} />
        <div style={{marginTop: 26}}>
          <Headline lines={['六类节点，', '覆盖每一次操作。']} from={10} size={64} gap={2} />
        </div>
      </div>

      {/* ── 左：节点清单 ─────────────────────────────────────────── */}
      <div style={{position: 'absolute', left: 140, top: ROW_TOP, width: 740}}>
        {NODES.map((n, i) => {
          const s = ROW_START + i * 13;
          return (
            <div
              key={n.name}
              style={{
                height: ROW,
                display: 'flex',
                alignItems: 'center',
                gap: 22,
                borderTop: i === 0 ? 'none' : `1px solid ${C.line}`,
                opacity: interpolate(frame, [s, s + 18], [0, 1], {...clamp, easing: E.out}),
                translate: interpolate(frame, [s, s + 26], ['-24px 0px', '0px 0px'], {
                  ...clamp,
                  easing: E.out,
                }),
              }}
            >
              <Glyph kind={n.name} />
              <div style={{display: 'flex', flexDirection: 'column', gap: 5}}>
                <div style={{display: 'flex', alignItems: 'baseline', gap: 14}}>
                  <span
                    style={{
                      fontFamily: F.mono,
                      fontSize: 24,
                      color: C.accentSoft,
                      letterSpacing: '0.01em',
                    }}
                  >
                    {n.name}
                  </span>
                  <span style={{fontFamily: F.sans, fontSize: 20, color: C.muted}}>{n.cn}</span>
                </div>
                <span style={{fontFamily: F.sans, fontSize: 21, color: C.body}}>{n.desc}</span>
              </div>
            </div>
          );
        })}
      </div>

      {/* ── 右上：辅助压枪 ──────────────────────────────────────── */}
      <div
        style={{
          position: 'absolute',
          left: 940,
          top: 300,
          width: 840,
          height: 388,
          borderRadius: 16,
          border: `1px solid ${C.line}`,
          backgroundColor: 'rgba(20,20,24,0.74)',
          opacity: interpolate(frame, [96, 120], [0, 1], {...clamp, easing: E.out}),
          translate: interpolate(frame, [96, 126], ['0px 26px', '0px 0px'], {
            ...clamp,
            easing: E.out,
          }),
        }}
      >
        <div
          style={{
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'space-between',
            padding: '24px 28px 0',
          }}
        >
          <div style={{display: 'flex', alignItems: 'baseline', gap: 14}}>
            <span style={{fontFamily: F.sans, fontSize: 27, fontWeight: 600, color: C.text}}>
              辅助压枪
            </span>
            <span
              style={{fontFamily: F.mono, fontSize: 17, color: C.dim, letterSpacing: '0.08em'}}
            >
              recoilAssist
            </span>
          </div>
          <span
            style={{
              fontFamily: F.mono,
              fontSize: 16,
              letterSpacing: '0.06em',
              padding: '5px 12px',
              borderRadius: 999,
              border: '1px solid rgba(99,102,241,0.45)',
              color: C.accentSoft,
              backgroundColor: 'rgba(99,102,241,0.1)',
            }}
          >
            16 项参数 · 引擎原生
          </span>
        </div>

        <svg
          width={840}
          height={388}
          style={{position: 'absolute', left: 0, top: 0, overflow: 'visible'}}
        >
          {/* 中线 */}
          <line
            x1={PLOT.x}
            y1={mid}
            x2={PLOT.x + PLOT.w}
            y2={mid}
            stroke="rgba(63,63,70,0.7)"
            strokeWidth={1}
            strokeDasharray="4 6"
          />
          {/* 无辅助 */}
          <polyline
            points={sample(drift)}
            fill="none"
            stroke="rgba(251,113,133,0.55)"
            strokeWidth={1.6}
            strokeDasharray="5 6"
          />
          {/* 有辅助 */}
          <polyline
            points={sample(fixed)}
            fill="none"
            stroke={C.accent}
            strokeWidth={2.2}
            strokeLinecap="round"
          />
          {/* 游标 */}
          {frame > 118 ? (
            <>
              <circle cx={headX} cy={headY} r={9} fill="rgba(99,102,241,0.22)" />
              <circle cx={headX} cy={headY} r={3.6} fill={C.accentSoft} />
            </>
          ) : null}
        </svg>

        <div
          style={{
            position: 'absolute',
            left: 28,
            top: 74,
            display: 'flex',
            gap: 20,
            fontFamily: F.mono,
            fontSize: 14,
            letterSpacing: '0.1em',
          }}
        >
          <span style={{color: 'rgba(251,113,133,0.85)'}}>— 无辅助</span>
          <span style={{color: C.accentSoft}}>— recoilAssist</span>
        </div>

        {/* 16 项参数刻度 */}
        <div
          style={{
            position: 'absolute',
            left: 28,
            bottom: 26,
            display: 'flex',
            gap: 7,
            alignItems: 'center',
          }}
        >
          {Array.from({length: ticks}).map((_, i) => {
            const s = 210 + i * 3.4;
            const on = interpolate(frame, [s, s + 10], [0, 1], clamp);
            return (
              <div
                key={i}
                style={{
                  width: 14,
                  height: 8,
                  borderRadius: 2,
                  backgroundColor: `rgba(99,102,241,${0.14 + on * 0.62})`,
                }}
              />
            );
          })}
          <span
            style={{
              marginLeft: 12,
              fontFamily: F.mono,
              fontSize: 15,
              color: C.dim,
              letterSpacing: '0.06em',
            }}
          >
            强度 0 – 100
          </span>
        </div>
      </div>

      {/* ── 右下：组合键 + 热加载 ────────────────────────────────── */}
      <div
        style={{
          position: 'absolute',
          left: 940,
          top: 718,
          width: 840,
          height: 208,
          borderRadius: 16,
          border: `1px solid ${C.line}`,
          backgroundColor: 'rgba(20,20,24,0.74)',
          opacity: interpolate(frame, [300, 324], [0, 1], {...clamp, easing: E.out}),
          translate: interpolate(frame, [300, 330], ['0px 26px', '0px 0px'], {
            ...clamp,
            easing: E.out,
          }),
          padding: '26px 28px',
        }}
      >
        <div style={{display: 'flex', alignItems: 'center', gap: 16}}>
          {['Shift', 'G'].map((label, i) => {
            const s = 312 + i * 16;
            return (
              <div key={label} style={{display: 'flex', alignItems: 'center', gap: 16}}>
                {i === 1 ? (
                  <span
                    style={{
                      fontFamily: F.mono,
                      fontSize: 22,
                      color: C.dim,
                      opacity: interpolate(frame, [s - 8, s + 4], [0, 1], clamp),
                    }}
                  >
                    +
                  </span>
                ) : null}
                <div
                  style={{
                    minWidth: 84,
                    padding: '10px 16px',
                    borderRadius: 8,
                    border: `1px solid ${C.lineStrong}`,
                    backgroundColor: 'rgba(24,24,27,0.9)',
                    fontFamily: F.mono,
                    fontSize: 22,
                    color: C.text,
                    textAlign: 'center',
                    opacity: interpolate(frame, [s, s + 14], [0, 1], clamp),
                    translate: interpolate(frame, [s, s + 18], ['0px 10px', '0px 0px'], {
                      ...clamp,
                      easing: E.out,
                    }),
                  }}
                >
                  {label}
                </div>
              </div>
            );
          })}
          <span
            style={{
              fontFamily: F.mono,
              fontSize: 20,
              color: C.dim,
              opacity: interpolate(frame, [352, 366], [0, 1], clamp),
            }}
          >
            →
          </span>
          <span
            style={{
              fontFamily: F.sans,
              fontSize: 21,
              color: C.body,
              opacity: interpolate(frame, [352, 368], [0, 1], clamp),
            }}
          >
            组合键触发任意节点
          </span>
        </div>

        <div
          style={{
            marginTop: 26,
            paddingTop: 20,
            borderTop: `1px solid ${C.line}`,
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'space-between',
            fontFamily: F.mono,
            fontSize: 18,
          }}
        >
          <span style={{color: C.dim}}>
            keymap.json · <span style={{color: C.body}}>&quot;recoilStrength&quot;: 46 → 62</span>
          </span>
          <span
            style={{
              color: C.ok,
              opacity: interpolate(frame, [386, 398], [0, 1], clamp),
              letterSpacing: '0.08em',
            }}
          >
            ● 热加载 · 无需重启
          </span>
        </div>
      </div>
    </AbsoluteFill>
  );
};
