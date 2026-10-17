import {decodePng} from './png.mjs';

const files = process.argv.slice(2);
for (const file of files) {
  const {w, h, data} = decodePng(file);
  const lum = (x, y) => {
    const i = (y * w + x) * 3;
    return 0.2126 * data[i] + 0.7152 * data[i + 1] + 0.0722 * data[i + 2];
  };
  let total = 0;
  for (let y = 0; y < h; y++) for (let x = 0; x < w; x++) total += lum(x, y);
  const mean = total / (w * h);

  // text/UI bbox: pixels brighter than 100 (excludes backdrop glow, which stays <70)
  let minX = w, minY = h, maxX = -1, maxY = -1, ink = 0;
  let white = 0, accent = 0;
  for (let y = 0; y < h; y++) {
    for (let x = 0; x < w; x++) {
      const i = (y * w + x) * 3;
      const l = lum(x, y);
      if (l > 100) {
        ink++;
        if (x < minX) minX = x;
        if (x > maxX) maxX = x;
        if (y < minY) minY = y;
        if (y > maxY) maxY = y;
      }
      if (l > 200) white++;
      const r = data[i], g = data[i + 1], b = data[i + 2];
      if (b > 150 && b - r > 40 && g < b) accent++;
    }
  }
  // edge guard: ink inside outer 40px frame?
  let edgeInk = 0;
  for (let y = 0; y < h; y++)
    for (let x = 0; x < w; x++)
      if (x < 40 || x > w - 40 || y < 40 || y > h - 40) if (lum(x, y) > 40) edgeInk++;

  // 4x3 luminance grid
  const grid = [];
  for (let gy = 0; gy < 3; gy++) {
    const row = [];
    for (let gx = 0; gx < 4; gx++) {
      let s = 0, n = 0;
      for (let y = Math.floor((gy * h) / 3); y < Math.floor(((gy + 1) * h) / 3); y += 3)
        for (let x = Math.floor((gx * w) / 4); x < Math.floor(((gx + 1) * w) / 4); x += 3) {
          s += lum(x, y);
          n++;
        }
      row.push((s / n).toFixed(1));
    }
    grid.push(row.join(' '));
  }

  console.log(
    [
      file,
      `  size ${w}x${h} meanLum ${mean.toFixed(2)}`,
      `  ink ${((ink / (w * h)) * 100).toFixed(1)}%  white ${((white / (w * h)) * 100).toFixed(2)}%  accent ${((accent / (w * h)) * 100).toFixed(2)}%`,
      `  inkBBox x[${minX},${maxX}] y[${minY},${maxY}]  edgeInk ${edgeInk}`,
      `  grid ${grid.join(' | ')}`,
    ].join('\n'),
  );
}
