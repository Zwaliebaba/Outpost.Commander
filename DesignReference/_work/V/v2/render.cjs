// V2: render artboards of the published canvas copy with Playwright, web fonts loaded through the proxy, /_blob/ mapped
// through compose/assets.json. Usage: node render.cjs [substring...]. Writes render/<board>.png and render/results.json.
const { chromium } = require('playwright');
const fs = require('fs'), path = require('path');
const PROJ = process.env.V2_PROJ || '/tmp/claude-0/-home-user-Outpost-Commander/31f4e1f0-0b17-5485-916e-c867697f0402/scratchpad/canvas/project';
const ASSETS = JSON.parse(fs.readFileSync('/home/user/Outpost.Commander/DesignReference/compose/assets.json'));
const byBlob = {}; for (const [k, v] of Object.entries(ASSETS)) byBlob[v] = '/home/user/Outpost.Commander/DesignReference/_work/assets/' + k;
const canvas = JSON.parse(fs.readFileSync(path.join(PROJ, 'canvas.json')));
const OUT = process.env.V2_OUT || path.join(__dirname, 'render');
(async () => {
  const browser = await chromium.launch({ proxy: { server: process.env.HTTPS_PROXY } });
  const ctx = await browser.newContext({ ignoreHTTPSErrors: true, viewport: { width: 1440, height: 900 } });
  const page = await ctx.newPage();
  let fontReq = 0, fontOk = 0;
  await page.route('**/*', async (route) => {
    const url = new URL(route.request().url());
    if (url.pathname.startsWith('/_blob/')) {
      const f = byBlob[url.pathname];
      if (f && fs.existsSync(f)) return route.fulfill({ path: f, contentType: 'image/png' });
      return route.fulfill({ status: 404, body: '' });
    }
    if (url.hostname === 'local') {
      if (url.pathname.endsWith('.js')) return route.fulfill({ body: '', contentType: 'text/javascript' });
      const p = path.join(PROJ, url.pathname);
      if (fs.existsSync(p)) return route.fulfill({ path: p, contentType: 'text/html' });
      return route.fulfill({ status: 404, body: '' });
    }
    if (url.hostname.includes('fonts.g')) { fontReq++; return route.continue(); }
    return route.abort();
  });
  const args = process.argv.slice(2);
  const results = [];
  for (const [file, b] of Object.entries(canvas.boards)) {
    if (args.length && !args.some(a => file.includes(a))) continue;
    await page.setViewportSize({ width: b.w, height: Math.min(b.h, 16000) });
    await page.goto('http://local/' + file, { waitUntil: 'networkidle' });
    await page.evaluate(() => document.fonts.ready);
    await page.waitForTimeout(150);
    const r = await page.evaluate(() => {
      const root = document.querySelector('[data-root]') || document.body;
      const rr = root.getBoundingClientRect();
      const fonts = [...document.fonts].filter(f => f.status === 'loaded').map(f => f.family + ' ' + f.weight);
      const imgs = [...document.images].map(i => ({ src: i.getAttribute('src'), ok: i.complete && i.naturalWidth > 0 }));
      let maxRight = 0, maxBottom = 0, wide = [];
      for (const el of root.querySelectorAll('*')) {
        const bb = el.getBoundingClientRect();
        if (bb.width === 0 && bb.height === 0) continue;
        maxRight = Math.max(maxRight, bb.right); maxBottom = Math.max(maxBottom, bb.bottom);
        if (el.tagName === 'TABLE' && bb.right > rr.right + 1) wide.push(Math.round(bb.width) + ':' + (el.textContent || '').trim().slice(0, 40));
      }
      return { w: rr.width, h: rr.height, scrollW: document.documentElement.scrollWidth, maxRight, maxBottom, imgs, wide, fonts: [...new Set(fonts)] };
    });
    r.file = file; r.boardW = b.w; r.boardH = b.h;
    results.push(r);
    await page.screenshot({ path: path.join(OUT, file.replace('.dc.html', '.png')), clip: { x: 0, y: 0, width: b.w, height: Math.min(b.h, 16000) } });
  }
  fs.writeFileSync(path.join(OUT, 'results.json'), JSON.stringify(results, null, 1));
  for (const r of results) {
    const bad = r.imgs.filter(i => !i.ok);
    const flag = (Math.abs(r.h - r.boardH) > 4 ? ` HEIGHT ${r.h.toFixed(0)} vs board ${r.boardH}` : '') + (r.maxRight > r.boardW + 1 ? ` RIGHT ${r.maxRight.toFixed(0)}>${r.boardW}` : '') + (r.maxBottom > r.boardH + 1 ? ` BOTTOM ${r.maxBottom.toFixed(0)}>${r.boardH}` : '') + (bad.length ? ` BADIMG ${bad.length}` : '') + (r.wide.length ? ` WIDETABLE ${r.wide.join('|')}` : '');
    console.log(r.file + (flag || ' ok') + ' fonts=' + r.fonts.length);
  }
  console.log('font requests', fontReq);
  await browser.close();
})();
