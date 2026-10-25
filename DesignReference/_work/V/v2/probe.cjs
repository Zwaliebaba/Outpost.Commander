// V2: evaluate a JS expression on one board after fonts load. Usage: node probe.cjs <board> '<js expression>'
const { chromium } = require('playwright');
const fs = require('fs'), path = require('path');
const PROJ = '/tmp/claude-0/-home-user-Outpost-Commander/31f4e1f0-0b17-5485-916e-c867697f0402/scratchpad/canvas/project';
const ASSETS = JSON.parse(fs.readFileSync('/home/user/Outpost.Commander/DesignReference/compose/assets.json'));
const byBlob = {}; for (const [k, v] of Object.entries(ASSETS)) byBlob[v] = '/home/user/Outpost.Commander/DesignReference/_work/assets/' + k;
const canvas = JSON.parse(fs.readFileSync(path.join(PROJ, 'canvas.json')));
(async () => {
  const [file, expr] = process.argv.slice(2);
  const b = canvas.boards[file];
  const browser = await chromium.launch({ proxy: { server: process.env.HTTPS_PROXY } });
  const ctx = await browser.newContext({ ignoreHTTPSErrors: true, viewport: { width: b.w, height: Math.min(b.h, 16000) } });
  const page = await ctx.newPage();
  await page.route('**/*', async (route) => {
    const url = new URL(route.request().url());
    if (url.pathname.startsWith('/_blob/')) return route.fulfill({ path: byBlob[url.pathname], contentType: 'image/png' });
    if (url.hostname === 'local') { if (url.pathname.endsWith('.js')) return route.fulfill({ body: '' }); return route.fulfill({ path: path.join(PROJ, url.pathname), contentType: 'text/html' }); }
    if (url.hostname.includes('fonts.g')) return route.continue();
    return route.abort();
  });
  await page.goto('http://local/' + file, { waitUntil: 'networkidle' });
  await page.evaluate(() => document.fonts.ready);
  await page.waitForTimeout(150);
  const r = await page.evaluate(expr);
  console.log(typeof r === 'string' ? r : JSON.stringify(r, null, 1));
  await browser.close();
})();
