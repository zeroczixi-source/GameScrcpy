import {AbsoluteFill, Audio, Sequence, interpolate, staticFile, useCurrentFrame} from 'remotion';
import {C, E, clamp} from './theme';
import {ProgressRail} from './components/base';
import {Title} from './scenes/Title';
import {Stream} from './scenes/Stream';
import {Keymap} from './scenes/Keymap';
import {KeymapDeep} from './scenes/KeymapDeep';
import {Script} from './scenes/Script';
import {Vision} from './scenes/Vision';
import {Backends} from './scenes/Backends';
import {UiTour} from './scenes/UiTour';
import {Outro} from './scenes/Outro';

export const SCENES = [
  {id: 'Title', C: Title, dur: 165},
  {id: 'UiTour', C: UiTour, dur: 372},
  {id: 'Keymap', C: Keymap, dur: 300},
  {id: 'KeymapDeep', C: KeymapDeep, dur: 470},
  {id: 'Stream', C: Stream, dur: 225},
  {id: 'Script', C: Script, dur: 210},
  {id: 'Vision', C: Vision, dur: 200},
  {id: 'Backends', C: Backends, dur: 195},
  {id: 'Outro', C: Outro, dur: 175},
] as const;

const FADE = 12;

const starts: number[] = [];
let cursor = 0;
SCENES.forEach((scene, i) => {
  starts.push(cursor - i * FADE);
  cursor += scene.dur;
});

export const TOTAL = cursor - (SCENES.length - 1) * FADE;

/** Crossfade wrapper: incoming scene fades in over the previous one. */
const Fader = ({children, index}: {children: React.ReactNode; index: number}) => {
  const frame = useCurrentFrame();
  return (
    <AbsoluteFill
      style={{
        opacity:
          index === 0
            ? interpolate(frame, [0, 10], [0, 1], {...clamp, easing: E.out})
            : interpolate(frame, [0, FADE], [0, 1], {...clamp, easing: E.inOut}),
      }}
    >
      {children}
    </AbsoluteFill>
  );
};

export const Main = () => (
  <AbsoluteFill style={{backgroundColor: C.bg}}>
    <Audio
      src={staticFile('bgm.wav')}
      volume={(f) =>
        Math.min(
          interpolate(f, [0, 42], [0, 0.52], clamp),
          interpolate(f, [TOTAL - 90, TOTAL - 16], [0.52, 0], clamp),
        )
      }
    />
    {SCENES.map((scene, i) => {
      const Scene = scene.C;
      return (
        <Sequence
          key={scene.id}
          name={scene.id}
          from={starts[i]}
          durationInFrames={scene.dur}
        >
          <Fader index={i}>
            <Scene />
          </Fader>
        </Sequence>
      );
    })}
    <ProgressRail total={TOTAL} />
  </AbsoluteFill>
);
