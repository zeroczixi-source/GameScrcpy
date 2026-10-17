import {AbsoluteFill, interpolate, useCurrentFrame} from 'remotion';
import {Backdrop, Bullet, Headline, Kicker} from '../components/base';
import {C, E, F, SAFE, clamp} from '../theme';

type Tok = {t: string; c: string};

const kw = C.accentSoft;
const txt = '#D4D4D8';
const str = '#4ADE80';
const num = '#F59E0B';

const CODE: Tok[][] = [
  [{t: '// @autoStart', c: C.dim}],
  [
    {t: 'while (', c: kw},
    {t: '!', c: txt},
    {t: 'mapi', c: txt},
    {t: '.isInterrupted()', c: txt},
    {t: ') {', c: txt},
  ],
  [
    {t: '  const ', c: kw},
    {t: 'hit', c: txt},
    {t: ' = ', c: txt},
    {t: 'mapi', c: txt},
    {t: '.findImage(', c: txt},
    {t: '"enemy"', c: str},
    {t: ', ', c: txt},
    {t: '0.85', c: num},
    {t: ');', c: txt},
  ],
  [
    {t: '  if (', c: kw},
    {t: 'hit', c: txt},
    {t: '.found', c: txt},
    {t: ') {', c: txt},
  ],
  [
    {t: '    mapi', c: txt},
    {t: '.click(', c: txt},
    {t: 'hit', c: txt},
    {t: '.x, ', c: txt},
    {t: 'hit', c: txt},
    {t: '.y', c: txt},
    {t: ');', c: txt},
  ],
  [
    {t: '    mapi', c: txt},
    {t: '.toast(', c: txt},
    {t: '"已点击目标"', c: str},
    {t: ', ', c: txt},
    {t: '1200', c: num},
    {t: ');', c: txt},
  ],
  [{t: '  }', c: txt}],
  [
    {t: '  mapi', c: txt},
    {t: '.sleep(', c: txt},
    {t: '120', c: num},
    {t: ');', c: txt},
  ],
  [{t: '}', c: txt}],
];

const LINE_H = 46;
const FONT = 26;

const flat = CODE.map((line) => line.reduce((n, tk) => n + tk.t.length, 0));
const totalChars = flat.reduce((a, b) => a + b, 0);

export const Script = () => {
  const frame = useCurrentFrame();

  const typed = interpolate(frame, [30, 152], [0, totalChars], {
    ...clamp,
    easing: E.inOut,
  });

  let consumed = 0;
  let activeLine = 0;
  const rendered = CODE.map((line, li) => {
    const budget = Math.max(0, typed - consumed);
    consumed += flat[li];
    if (budget > 0) activeLine = li;
    return {line, budget};
  });

  return (
    <AbsoluteFill style={{padding: SAFE}}>
      <Backdrop drift={8} />

      {/* editor */}
      <div
        style={{
          position: 'absolute',
          left: 140,
          top: 236,
          width: 880,
          borderRadius: 14,
          border: `1px solid ${C.lineStrong}`,
          backgroundColor: '#101014',
          overflow: 'hidden',
          boxShadow: '0 40px 90px rgba(0,0,0,0.5)',
          opacity: interpolate(frame, [8, 28], [0, 1], {...clamp, easing: E.out}),
          translate: interpolate(frame, [8, 38], ['-30px 16px', '0px 0px'], {
            ...clamp,
            easing: E.out,
          }),
        }}
      >
        <div
          style={{
            height: 46,
            display: 'flex',
            alignItems: 'center',
            gap: 12,
            padding: '0 18px',
            borderBottom: `1px solid ${C.line}`,
            backgroundColor: '#16161A',
          }}
        >
          {[0, 1, 2].map((i) => (
            <div
              key={i}
              style={{width: 10, height: 10, borderRadius: 99, backgroundColor: C.lineStrong}}
            />
          ))}
          <span
            style={{
              marginLeft: 10,
              fontFamily: F.mono,
              fontSize: 17,
              color: C.body,
              letterSpacing: '0.04em',
            }}
          >
            auto_farm.js
          </span>
          <span
            style={{
              marginLeft: 'auto',
              fontFamily: F.mono,
              fontSize: 15,
              color: C.dim,
              letterSpacing: '0.1em',
            }}
          >
            QUICKJS · SANDBOX
          </span>
        </div>

        <div style={{padding: '26px 0 30px', position: 'relative'}}>
          {rendered.map(({line, budget}, li) => {
            const isActive = li === activeLine && typed < totalChars;
            return (
              <div
                key={li}
                style={{
                  display: 'flex',
                  alignItems: 'center',
                  height: LINE_H,
                  backgroundColor: isActive ? 'rgba(99,102,241,0.07)' : 'transparent',
                  boxShadow: isActive ? `inset 3px 0 0 ${C.accent}` : 'none',
                }}
              >
                <span
                  style={{
                    width: 62,
                    textAlign: 'right',
                    paddingRight: 22,
                    fontFamily: F.mono,
                    fontSize: 20,
                    color: isActive ? C.muted : '#3F3F46',
                    flexShrink: 0,
                  }}
                >
                  {li + 1}
                </span>
                <span style={{fontFamily: F.mono, fontSize: FONT, whiteSpace: 'pre'}}>
                  {line.map((tk, ti) => {
                    const before = line.slice(0, ti).reduce((n, x) => n + x.t.length, 0);
                    const shown = Math.max(0, Math.min(tk.t.length, budget - before));
                    if (shown === 0) return null;
                    return (
                      <span key={ti} style={{color: tk.c}}>
                        {tk.t.slice(0, shown)}
                      </span>
                    );
                  })}
                  {isActive && (
                    <span
                      style={{
                        display: 'inline-block',
                        width: 10,
                        height: 26,
                        marginLeft: 2,
                        verticalAlign: 'text-bottom',
                        backgroundColor: C.accentSoft,
                        opacity: frame % 18 < 9 ? 1 : 0,
                      }}
                    />
                  )}
                </span>
              </div>
            );
          })}
        </div>

        {/* toast demo, mirrors mapi.toast() */}
        <div
          style={{
            position: 'absolute',
            right: 22,
            bottom: 22,
            display: 'flex',
            alignItems: 'center',
            gap: 12,
            padding: '12px 20px',
            borderRadius: 10,
            border: `1px solid ${C.line}`,
            backgroundColor: 'rgba(24,24,27,0.95)',
            opacity: interpolate(frame, [158, 174], [0, 1], {...clamp, easing: E.out}),
            translate: interpolate(frame, [158, 178], ['0px 14px', '0px 0px'], {
              ...clamp,
              easing: E.out,
            }),
            boxShadow: '0 20px 40px rgba(0,0,0,0.45)',
          }}
        >
          <div
            style={{width: 8, height: 8, borderRadius: 99, backgroundColor: C.ok}}
          />
          <span style={{fontFamily: F.sans, fontSize: 22, color: C.body}}>
            已点击目标 · 1200ms
          </span>
        </div>
      </div>

      {/* copy */}
      <div
        style={{
          position: 'absolute',
          left: 1090,
          top: 268,
          width: 690,
          display: 'flex',
          flexDirection: 'column',
        }}
      >
        <Kicker index="05" label="脚本系统" from={14} />
        <div style={{marginTop: 32}}>
          <Headline lines={['31 个 API，', '把重复交给脚本。']} from={24} size={72} gap={4} />
        </div>
        <div style={{display: 'flex', flexDirection: 'column', gap: 18, marginTop: 54}}>
          <Bullet from={58} size={29}>
            QuickJS 沙箱，多脚本并行
          </Bullet>
          <Bullet from={68} size={29}>
            触摸 / 滑动 / 缩放 / 图像识别
          </Bullet>
          <Bullet from={78} size={29}>
            内置编辑器 + 片段插入 + 自动补全
          </Bullet>
          <Bullet from={88} size={29}>
            投屏即运行（// @autoStart）
          </Bullet>
        </div>
      </div>
    </AbsoluteFill>
  );
};
