import {Img, staticFile, interpolate, useCurrentFrame} from 'remotion';
import {C, E, F, clamp} from '../theme';

export const WIN = {w: 1300, h: 740};
/** Sidebar is chrome: it must stay put while the content pane scrolls. */
export const SIDEBAR = 108;
/** How far the settings pane was scrolled between the two captured shots. */
export const SETTINGS_SCROLL = 480;

const PAGES: Record<string, {a: string; b?: string; shift?: number}> = {
  home: {a: 'ui/page_home.png'},
  settings: {a: 'ui/page_settings.png', b: 'ui/page_settings_scrolled.png', shift: SETTINGS_SCROLL},
  terminal: {a: 'ui/page_terminal.png'},
};

const Src = ({src, left, top, w = WIN.w, h = WIN.h}: {src: string; left: number; top: number; w?: number; h?: number}) => (
  <Img
    src={staticFile(src)}
    style={{position: 'absolute', left, top, width: w, height: h, display: 'block'}}
  />
);

/**
 * The real application window, rebuilt so its content pane can scroll
 * exactly like the app does (sidebar stays fixed, pane moves).
 */
export const AppWindow = ({
  page,
  scroll = 0,
  scale = 1,
  radius = 16,
  glow = 1,
}: {
  page: keyof typeof PAGES | string;
  scroll?: number;
  scale?: number;
  radius?: number;
  glow?: number;
}) => {
  const spec = PAGES[page] ?? PAGES.home;
  const contentW = WIN.w - SIDEBAR;
  return (
    <div
      style={{
        width: WIN.w,
        height: WIN.h,
        borderRadius: radius,
        overflow: 'hidden',
        backgroundColor: C.bg,
        border: '1px solid rgba(63,63,70,0.9)',
        boxShadow: `0 ${34 * scale}px ${90 * scale}px rgba(0,0,0,0.62), 0 0 0 1px rgba(0,0,0,0.5), 0 0 ${
          120 * glow
        }px rgba(99,102,241,0.10)`,
        scale: `${scale}`,
      }}
    >
      {/* fixed sidebar */}
      <div
        style={{
          position: 'absolute',
          left: 0,
          top: 0,
          width: SIDEBAR,
          height: WIN.h,
          overflow: 'hidden',
          borderRight: '1px solid rgba(39,39,42,0.9)',
        }}
      >
        <Src src={spec.a} left={0} top={0} />
      </div>
      {/* scrolling content pane */}
      <div
        style={{
          position: 'absolute',
          left: SIDEBAR,
          top: 0,
          width: contentW,
          height: WIN.h,
          overflow: 'hidden',
        }}
      >
        <div style={{position: 'absolute', inset: 0, translate: `0px ${-scroll}px`}}>
          <Src src={spec.a} left={-SIDEBAR} top={0} />
          {spec.b ? <Src src={spec.b} left={-SIDEBAR} top={spec.shift ?? 0} /> : null}
        </div>
      </div>
    </div>
  );
};

/** Numbered legend that explains the spots on the real UI. */
export const Legend = ({
  items,
  start = 0,
  stagger = 9,
  width = 404,
  out = 99999,
}: {
  items: {n: number; title: string; detail: string}[];
  start?: number;
  stagger?: number;
  width?: number;
  out?: number;
}) => {
  const frame = useCurrentFrame();
  const fadeOut = interpolate(frame, [out, out + 12], [1, 0], clamp);
  return (
    <div style={{display: 'flex', flexDirection: 'column', gap: 22, width, opacity: fadeOut}}>
      {items.map((it, i) => {
        const s = start + i * stagger;
        const t = interpolate(frame, [s, s + 18], [0, 1], {...clamp, easing: E.out});
        if (t <= 0) return null;
        return (
          <div
            key={it.n}
            style={{
              display: 'flex',
              gap: 16,
              alignItems: 'flex-start',
              opacity: t,
              translate: interpolate(frame, [s, s + 20], ['0px 14px', '0px 0px'], {
                ...clamp,
                easing: E.out,
              }),
            }}
          >
            <span
              style={{
                flexShrink: 0,
                width: 26,
                height: 26,
                marginTop: 4,
                borderRadius: 999,
                border: `1px solid rgba(99,102,241,0.55)`,
                backgroundColor: 'rgba(99,102,241,0.12)',
                color: C.accentSoft,
                fontFamily: F.mono,
                fontSize: 15,
                fontWeight: 700,
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
              }}
            >
              {it.n}
            </span>
            <div style={{display: 'flex', flexDirection: 'column', gap: 6}}>
              <span
                style={{
                  fontFamily: F.sans,
                  fontSize: 26,
                  fontWeight: 600,
                  color: C.text,
                  letterSpacing: '-0.01em',
                  lineHeight: 1.25,
                }}
              >
                {it.title}
              </span>
              <span
                style={{
                  fontFamily: F.sans,
                  fontSize: 19,
                  color: C.muted,
                  lineHeight: 1.45,
                  letterSpacing: '0.01em',
                }}
              >
                {it.detail}
              </span>
            </div>
          </div>
        );
      })}
    </div>
  );
};

/** Horizontal wipe: the incoming page slides in behind a thin accent edge. */
export const WipeLayer = ({reveal, children}: {reveal: number; children: React.ReactNode}) => {
  if (reveal <= 0) return null;
  return (
    <div
      style={{
        position: 'absolute',
        inset: 0,
        clipPath: `inset(-40px ${(1 - reveal) * 100}% -40px -40px)`,
      }}
    >
      {children}
      <div
        style={{
          position: 'absolute',
          left: `${reveal * 100}%`,
          top: -40,
          bottom: -40,
          width: 2,
          background: `linear-gradient(to bottom, transparent, ${C.accentSoft}, transparent)`,
          opacity: reveal > 0.02 && reveal < 0.99 ? 0.85 : 0,
        }}
      />
    </div>
  );
};
