// Render every artboard from disk with Playwright; report the rendered size against canvas.json, overflowing or clipped
// elements, broken images, and take a screenshot of each.
const { chromium } = require('playwright');
const fs = require('fs'), path = require('path');
const PROJ = '/home/user/Outpost.Commander/DesignReference/canvas/project';
const ASSETS = JSON.parse(fs.readFileSync('/home/user/Outpost.Commander/DesignReference/canvas/assets.json'));
const byBlob = {}; for (const [k, v] of Object.entries(ASSETS)) byBlob[v] = '/home/user/Outpost.Commander/DesignReference/_work/assets/' + k;
const canvas = JSON.parse(fs.readFileSync(path.join(PROJ, 'canvas.json')));
const OUT = '/home/user/Outpost.Commander/DesignReference/_work/V/render';
(async () => {
  const browser = await chromium.launch();
  const page = await browser.newPage({ viewport: { width: 1440, height: 900 } });
  await page.route('**/*', async (route) => {
    const url = new URL(route.request().url());
    if (url.pathname.startsWith('/_blob/')) {
      const f = byBlob[url.pathname];
      if (f && fs.existsSync(f)) return route.fulfill({ path: f, contentType: 'image/png' });
      return route.fulfill({ status: 404, body: '' });
    }
    if (url.hostname === 'local') {
      const p = path.join(PROJ, url.pathname);
      if (url.pathname.endsWith('support.js')) return route.fulfill({ body: '', contentType: 'text/javascript' });
      if (fs.existsSync(p)) return route.fulfill({ path: p, contentType: 'text/html' });
      return route.fulfill({ status: 404, body: '' });
    }
    return route.abort();
  });
  const results = [];
  const only = process.argv[2];
  for (const [file, b] of Object.entries(canvas.boards)) {
    if (only && !file.includes(only)) continue;
    await page.setViewportSize({ width: b.w, height: Math.min(b.h, 16000) });
    await page.goto('http://local/' + file, { waitUntil: 'load' });
    await page.waitForTimeout(150);
    const r = await page.evaluate(() => {
      const root = document.querySelector('[data-root]') || document.body;
      const rr = root.getBoundingClientRect();
      const imgs = [...document.images].map(i => ({ src: i.getAttribute('src'), ok: i.complete && i.naturalWidth > 0 }));
      // elements whose content overflows them horizontally (clipped text in tables/cells)
      const over = [];
      for (const el of root.querySelectorAll('td,th,p,li,div,span,pre,code,h1,h2,h3')) {
        const cs = getComputedStyle(el);
        if (el.scrollWidth > el.clientWidth + 2 && (cs.overflow === 'hidden' || cs.overflowX === 'hidden' || cs.textOverflow === 'ellipsis') && el.clientWidth > 0) {
          over.push((el.textContent || '').trim().slice(0, 80));
        }
      }
      // anything drawn outside the root's box
      let maxRight = 0, maxBottom = 0;
      for (const el of root.querySelectorAll('*')) {
        const b = el.getBoundingClientRect();
        if (b.width === 0 && b.height === 0) continue;
        maxRight = Math.max(maxRight, b.right); maxBottom = Math.max(maxBottom, b.bottom);
      }
      return { w: rr.width, h: rr.height, scrollW: document.documentElement.scrollWidth, maxRight, maxBottom, imgs, over: over.slice(0, 10), nOver: over.length };
    });
    r.file = file; r.boardW = b.w; r.boardH = b.h;
    results.push(r);
    await page.screenshot({ path: path.join(OUT, file.replace('.dc.html', '.png')), fullPage: false, clip: { x: 0, y: 0, width: b.w, height: Math.min(b.h, 16000) } });
  }
  fs.writeFileSync(path.join(OUT, 'results.json'), JSON.stringify(results, null, 1));
  for (const r of results) {
    const bad = r.imgs.filter(i => !i.ok);
    const flag = (Math.abs(r.h - r.boardH) > 4 ? ` HEIGHT ${r.h.toFixed(0)} vs board ${r.boardH}` : '') + (r.maxRight > r.boardW + 1 ? ` RIGHT ${r.maxRight.toFixed(0)}>${r.boardW}` : '') + (r.maxBottom > r.boardH + 1 ? ` BOTTOM ${r.maxBottom.toFixed(0)}>${r.boardH}` : '') + (bad.length ? ` BADIMG ${bad.length}` : '') + (r.nOver ? ` OVERFLOW ${r.nOver}` : '');
    console.log(r.file + (flag || ' ok'));
  }
  await browser.close();
})();
