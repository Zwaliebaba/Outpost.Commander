// Measures each artboard's rendered height at its width, so a frame is never clipped (heights.json for build.py).
const { chromium } = require('playwright');
const fs = require('fs');
const path = require('path');
(async () => {
  const project = path.join(process.argv[2], 'project');
  const out = process.argv[3];
  const canvas = JSON.parse(fs.readFileSync(path.join(project, 'canvas.json'), 'utf8'));
  const browser = await chromium.launch();
  const heights = {};
  for (const [file, b] of Object.entries(canvas.boards)) {
    const page = await browser.newPage({ viewport: { width: b.w, height: 1000 } });
    await page.goto('file://' + path.join(project, file), { waitUntil: 'load' });
    await page.evaluate(() => document.fonts.ready);
    await page.waitForTimeout(150);
    const h = await page.evaluate(() => Math.ceil(document.querySelector('[data-root]').getBoundingClientRect().height));
    heights[file] = Math.ceil(h * 1.03) + 24;
    if (process.argv[4]) await page.screenshot({ path: path.join(process.argv[4], file.replace('.dc.html', '.png')), fullPage: true });
    await page.close();
  }
  await browser.close();
  fs.writeFileSync(out, JSON.stringify(heights, null, 1));
  console.log(Object.keys(heights).length, 'measured; tallest', Math.max(...Object.values(heights)));
})();
