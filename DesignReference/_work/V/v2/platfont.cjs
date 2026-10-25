// V2: which platform font Chrome actually uses for the first element matching a selector on a board.
const { chromium } = require('playwright');
const fs = require('fs'), path = require('path');
const PROJ = '/tmp/claude-0/-home-user-Outpost-Commander/31f4e1f0-0b17-5485-916e-c867697f0402/scratchpad/canvas/project';
(async () => {
  const [file, sel] = process.argv.slice(2);
  const browser = await chromium.launch({ proxy: { server: process.env.HTTPS_PROXY } });
  const ctx = await browser.newContext({ ignoreHTTPSErrors: true });
  const page = await ctx.newPage();
  await page.route('**/*', async (route) => {
    const url = new URL(route.request().url());
    if (url.hostname === 'local') { if (url.pathname.endsWith('.js')) return route.fulfill({ body: '' }); return route.fulfill({ path: path.join(PROJ, url.pathname), contentType: 'text/html' }); }
    if (url.hostname.includes('fonts.g')) return route.continue();
    return route.fulfill({ status: 404, body: '' });
  });
  await page.goto('http://local/' + file, { waitUntil: 'networkidle' });
  const s = await ctx.newCDPSession(page);
  await s.send('DOM.enable'); await s.send('CSS.enable');
  const { root } = await s.send('DOM.getDocument', { depth: -1 });
  const { nodeIds } = await s.send('DOM.querySelectorAll', { nodeId: root.nodeId, selector: sel });
  for (const id of nodeIds.slice(0, 3)) { const f = await s.send('CSS.getPlatformFontsForNode', { nodeId: id }); console.log(JSON.stringify(f.fonts)); }
  await browser.close();
})();
