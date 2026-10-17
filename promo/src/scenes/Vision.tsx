import {AbsoluteFill, interpolate, useCurrentFrame} from 'remotion';
import {Backdrop, Bullet, Headline, Kicker} from '../components/base';
import {GameHud, WindowFrame} from '../components/mocks';
import {C, E, F, SAFE, clamp} from '../theme';

const W = {w: 900, h: 520, x: 140, y: 262};

const REGION = {x: 54, y: 84, w: 336, h: 268};
const TARGET = {x: 214, y: 172, size: 62};

export const Vision = () => {
  const frame = useCurrentFrame();

  const scanY = REGION.y + (((frame - 20) % 56) / 56) * REGION.h;
  const matched = frame >= 88;

  // composite pseudo-human swipe trace
  const nums: number[][] = [];
  const pts: string[] = [];
  for (let i = 0; i <= 72; i++) {
    const x = i * 4.4;
    const y =
      46 +
      9 * Math.sin(i * 0.26 + frame * 0.06) +
      4.5 * Math.sin(i * 0.74 + frame * 0.11) +
      1.6 * Math.sin(i * 1.9 + frame * 0.2);
    nums.push([x, y]);
    pts.push(`${x.toFixed(1)},${y.toFixed(1)}`);
  }
  const traceLen = nums
    .slice(1)
    .reduce((a, p, i) => a + Math.hypot(p[0] - nums[i][0], p[1] - nums[i][1]), 0);
  const drawP = interpolate(frame, [84, 152], [0, 1], {...clamp, easing: E.inOut});
  const head = nums[Math.min(nums.length - 1, Math.floor(drawP * (nums.length - 1)))];

  return (
    <AbsoluteFill style={{padding: SAFE}}>
      <Backdrop drift={-8} />

      {/* detection window */}
      <div
        style={{
          position: 'absolute',
          left: W.x,
          top: W.y,
          opacity: interpolate(frame, [6, 26], [0, 1], {...clamp, easing: E.out}),
          translate: interpolate(frame, [6, 36], ['-34px 18px', '0px 0px'], {
            ...clamp,
            easing: E.out,
          }),
        }}
      >
        <WindowFrame
          width={W.w}
          height={W.h}
          title="selection · enemy 模板匹配"
          accent={matched}
          right={
            <span
              style={{
                fontFamily: F.mono,
                fontSize: 15,
                letterSpacing: '0.1em',
                color: matched ? C.ok : C.dim,
              }}
            >
              {matched ? '● MATCH' : '○ SCANNING'}
            </span>
          }
        >
          <GameHud w={W.w} h={W.h - 44} detail={1} />

          {/* search region */}
          <div
            style={{
              position: 'absolute',
              left: REGION.x,
              top: REGION.y,
              width: REGION.w,
              height: REGION.h,
              border: '1px dashed rgba(129,140,248,0.5)',
              borderRadius: 4,
              overflow: 'hidden',
              opacity: interpolate(frame, [20, 40], [0, 1], {...clamp, easing: E.out}),
            }}
          >
            <div
              style={{
                position: 'absolute',
                left: 0,
                right: 0,
                top: scanY - REGION.y,
                height: 2,
                background:
                  'linear-gradient(90deg, transparent, rgba(129,140,248,0.85), transparent)',
                opacity: matched ? 0 : 1,
              }}
            />
            <div
              style={{
                position: 'absolute',
                left: 10,
                top: 8,
                fontFamily: F.mono,
                fontSize: 14,
                letterSpacing: '0.1em',
                color: 'rgba(161,161,170,0.7)',
              }}
            >
              REGION #3
            </div>
          </div>

          {/* match brackets */}
          <MatchBrackets frame={frame} matched={matched} />

          {matched && (
            <div
              style={{
                position: 'absolute',
                left: TARGET.x + TARGET.size / 2 + 16,
                top: TARGET.y - 16,
                display: 'flex',
                alignItems: 'center',
                gap: 10,
                padding: '6px 12px',
                borderRadius: 6,
                border: '1px solid rgba(34,197,94,0.5)',
                backgroundColor: 'rgba(9,9,11,0.75)',
                fontFamily: F.mono,
                fontSize: 17,
                color: C.ok,
                letterSpacing: '0.06em',
                opacity: interpolate(frame, [92, 108], [0, 1], {...clamp, easing: E.out}),
                translate: interpolate(frame, [92, 112], ['-8px 0px', '0px 0px'], {
                  ...clamp,
                  easing: E.out,
                }),
              }}
            >
              enemy · 0.93
            </div>
          )}
        </WindowFrame>
      </div>

      <div
        style={{
          position: 'absolute',
          left: W.x,
          top: W.y + W.h + 26,
          fontFamily: F.mono,
          fontSize: 20,
          color: C.dim,
          letterSpacing: '0.06em',
          opacity: interpolate(frame, [46, 66], [0, 1], {...clamp, easing: E.out}),
        }}
      >
        mapi.findImage(&quot;enemy&quot;, 0.85) → found · x / y
      </div>

      {/* copy */}
      <div
        style={{
          position: 'absolute',
          left: 1130,
          top: 268,
          width: 650,
          display: 'flex',
          flexDirection: 'column',
        }}
      >
        <Kicker index="06" label="图像识别 · 拟人化" from={12} />
        <div style={{marginTop: 32}}>
          <Headline lines={['看得懂画面，', '也懂得像人。']} from={22} size={72} gap={4} />
        </div>
        <div style={{display: 'flex', flexDirection: 'column', gap: 16, marginTop: 48}}>
          <Bullet from={50} size={28}>
            OpenCV 模板匹配 + 置信度阈值
          </Bullet>
          <Bullet from={60} size={28}>
            坐标区域 / 自定义选区限定检索
          </Bullet>
          <Bullet from={70} size={28}>
            触摸随机偏移，轨迹不平直
          </Bullet>
        </div>

        {/* humanised swipe trace */}
        <div
          style={{
            marginTop: 34,
            opacity: interpolate(frame, [78, 100], [0, 1], {...clamp, easing: E.out}),
          }}
        >
          <svg width={330} height={78} style={{overflow: 'visible'}}>
            <polyline
              points={pts.join(' ')}
              fill="none"
              stroke="rgba(99,102,241,0.75)"
              strokeWidth={1.6}
              strokeDasharray={traceLen}
              strokeDashoffset={traceLen * (1 - drawP)}
              strokeLinecap="round"
            />
            <circle cx={head[0]} cy={head[1]} r={6} fill={C.accentSoft} opacity={0.2} />
            <circle cx={head[0]} cy={head[1]} r={2.6} fill={C.accentSoft} />
          </svg>
          <div
            style={{
              fontFamily: F.mono,
              fontSize: 16,
              letterSpacing: '0.12em',
              color: C.dim,
              marginTop: 6,
            }}
          >
            三层正弦叠加 · 拟人滑动轨迹
          </div>
        </div>
      </div>
    </AbsoluteFill>
  );
};

const MatchBrackets = ({frame, matched}: {frame: number; matched: boolean}) => {
  const s = TARGET.size / 2;
  const corners = [
    {x: TARGET.x - s, y: TARGET.y - s, dir: 'tl'},
    {x: TARGET.x + s, y: TARGET.y - s, dir: 'tr'},
    {x: TARGET.x - s, y: TARGET.y + s, dir: 'bl'},
    {x: TARGET.x + s, y: TARGET.y + s, dir: 'br'},
  ];
  const prog = interpolate(frame, [88, 106], [0, 1], {...clamp, easing: E.snap});
  return (
    <>
      {corners.map((c) => {
        const h = c.dir.includes('l') ? {left: 0} : {right: 0};
        const v = c.dir.includes('t') ? {top: 0} : {bottom: 0};
        return (
          <div
            key={c.dir}
            style={{
              position: 'absolute',
              left: c.x - 16,
              top: c.y - 16,
              width: 32,
              height: 32,
              scale: interpolate(prog, [0, 1], [1.7, 1]),
              opacity: prog,
            }}
          >
            <div
              style={{
                position: 'absolute',
                width: 18,
                height: 2,
                backgroundColor: C.ok,
                ...h,
                ...v,
              }}
            />
            <div
              style={{
                position: 'absolute',
                width: 2,
                height: 18,
                backgroundColor: C.ok,
                ...h,
                ...v,
              }}
            />
          </div>
        );
      })}
      {matched && (
        <div
          style={{
            position: 'absolute',
            left: TARGET.x - 2,
            top: TARGET.y - 2,
            width: 4,
            height: 4,
            borderRadius: 99,
            backgroundColor: C.ok,
            opacity: interpolate(frame, [96, 104], [0, 1], clamp),
          }}
        />
      )}
    </>
  );
};
