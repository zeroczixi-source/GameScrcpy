// Numeric sanity check for the UiTour beats: regions, ink, and edges.
import {decodePng} from './png.mjs';

const L = (r, g, b) => 0.2126 * r + 0.7152 * g + 0.0722 * b;

const regions = {
  head: [140, 110, 900, 250],
  card: [150, 320, 1200, 690],
  legend: [1360, 340, 430, 400],
  caption: [110, 1000, 700, 40],
};

for (const file of process.argv.slice(2)) {
  const {w, h, data} = decodePng(file);
  const out = [];
  for (const [name, [x0, y0, rw, rh]] of Object.entries(regions)) {
    let sum = 0;
    let ink = 0;
    let n = 0;
    let minX = 1e9;
    let maxX = -1e9;
    let minY = 1e9;
    let maxY = -1e9;
    const x1 = Math.min(w, x0 + rw);
    const y1 = Math.min(h, y0 + rh);
    for (let y = y0; y < y1; y++) {
      for (let x = x0; x < x1; x++) {
        const i = (y * w + x) * 3;
        const l = L(data[i], data[i + 1], data[i + 2]);
        sum += l;
        n++;
        if (l > 120) {
          ink++;
          if (x < minX) minX = x;
          if (x > maxX) maxX = x;
          if (y < minY) minY = y;
          if (y > maxY) maxY = y;
        }
      }
    }
    out.push(
      `${name}: mean ${(sum / Math.max(1, n)).toFixed(1)} ink% ${((ink / Math.max(1, n)) * 100).toFixed(1)}` +
        (maxX > 0 ? ` bbox x[${minX},${maxX}] y[${minY},${maxY}]` : ' bbox none'),
    );
  }
  // global ink bbox to catch anything escaping the layout
  let gx0 = 1e9;
  let gx1 = -1e9;
  let gy0 = 1e9;
  let gy1 = -1e9;
  for (let y = 0; y < h; y++) {
    for (let x = 0; x < w; x++) {
      const i = (y * w + x) * 3;
      if (L(data[i], data[i + 1], data[i + 2]) > 150) {
        if (x < gx0) gx0 = x;
        if (x > gx1) gx1 = x;
        if (y < gy0) gy0 = y;
        if (y > gy1) gy1 = y;
      }
    }
  }
  console.log(`${file}`);
  for (const line of out) console.log('  ' + line);
  console.log(`  global ink bbox x[${gx0},${gx1}] y[${gy0},${gy1}]`);
}
