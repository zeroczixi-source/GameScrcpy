import {Composition, Folder} from 'remotion';
import {Main, SCENES, TOTAL} from './Main';

export const RemotionRoot = () => (
  <>
    <Composition
      id="GameScrcpy"
      component={Main}
      width={1920}
      height={1080}
      fps={30}
      durationInFrames={TOTAL}
    />
    <Folder name="Scenes">
      {SCENES.map((scene) => {
        const Scene = scene.C;
        return (
          <Composition
            key={scene.id}
            id={scene.id}
            component={Scene}
            width={1920}
            height={1080}
            fps={30}
            durationInFrames={scene.dur}
          />
        );
      })}
    </Folder>
  </>
);
