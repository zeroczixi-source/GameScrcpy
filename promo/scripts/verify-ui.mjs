import {decodePng} from './png.mjs';

const files = process.argv.slice(2);
const grids = [];
for (const file of files) {
  const {w, h, data} = decodePng(file);
  let magenta = 0;
  let sum = 0;
  for (let y = 0; y < h; y++) {
    for (let x = 0; x < w; x++) {
      const i = (y * w + x) * 3;
      const r = data[i], g = data[i + 1], b = data[i + 2];
      if (r > 180 && b > 180 && g < 90) magenta++;
      sum += 0.2126 * r + 0.7152 * g + 0.0722 * b;
    }
  }
  // 16x12 luminance signature for page-to-page comparison
  const gx = 16, gy = 12;
  const sig = [];
  for (let by = 0; by < gy; by++) {
    for (let bx = 0; bx < gx; bx++) {
      let s = 0, n = 0;
      for (let y = Math.floor((by * h) / gy); y < Math.floor(((by + 1) * h) / gy); y += 2)
        for (let x = Math.floor((bx * w) / gx); x < Math.floor(((bx + 1) * w) / gx); x += 2) {
          const i = (y * w + x) * 3;
          s += 0.2126 * data[i] + 0.7152 * data[i + 1] + 0.0722 * data[i + 2];
          n++;
        }
      sig.push(s / n);
    }
  }
  grids.push({file, sig});
  console.log(
    `${file}  ${w}x${h}  meanLum ${(sum / (w * h)).toFixed(1)}  magenta ${((magenta / (w * h)) * 100).toFixed(2)}%`,
  );
}

if (grids.length > 1) {
  const base = grids[0];
  for (const g of grids.slice(1)) {
    let d = 0;
    for (let i = 0; i < g.sig.length; i++) d += Math.abs(g.sig[i] - base.sig[i]);
    console.log(`diff vs ${base.file}: ${(d / g.sig.length).toFixed(2)} mean abs luminance delta`);
  }
}
