// V2: count words broken mid-word across lines inside table cells, per board, for a project folder.
// Usage: node brokenwords.cjs <projectDir> <out.json> [substring...]
const { chromium } = require('playwright');
const fs = require('fs'), path = require('path');
const [PROJ, OUTF, ...only] = process.argv.slice(2);
const ASSETS = JSON.parse(fs.readFileSync('/home/user/Outpost.Commander/DesignReference/compose/assets.json'));
const byBlob = {}; for (const [k, v] of Object.entries(ASSETS)) byBlob[v] = '/home/user/Outpost.Commander/DesignReference/_work/assets/' + k;
const canvas = JSON.parse(fs.readFileSync(path.join(PROJ, 'canvas.json')));
(async () => {
  const browser = await chromium.launch({ proxy: { server: process.env.HTTPS_PROXY } });
  const ctx = await browser.newContext({ ignoreHTTPSErrors: true });
  const page = await ctx.newPage();
  await page.route('**/*', async (route) => {
    const url = new URL(route.request().url());
    if (url.pathname.startsWith('/_blob/')) return route.fulfill({ path: byBlob[url.pathname], contentType: 'image/png' });
    if (url.hostname === 'local') { if (url.pathname.endsWith('.js')) return route.fulfill({ body: '' }); return route.fulfill({ path: path.join(PROJ, url.pathname), contentType: 'text/html' }); }
    if (url.hostname.includes('fonts.g')) return route.continue();
    return route.abort();
  });
  const out = {};
  for (const [file, b] of Object.entries(canvas.boards)) {
    if (only.length && !only.some(a => file.includes(a))) continue;
    if (file.startsWith('screen-')) continue;
    await page.setViewportSize({ width: b.w, height: 900 });
    await page.goto('http://local/' + file, { waitUntil: 'networkidle' });
    await page.evaluate(() => document.fonts.ready);
    out[file] = await page.evaluate(() => {
      const res = [];
      for (const cell of document.querySelectorAll('td,th')) {
        const walker = document.createTreeWalker(cell, NodeFilter.SHOW_TEXT);
        let n;
        while ((n = walker.nextNode())) {
          const t = n.textContent; const re = /\S+/g; let m;
          while ((m = re.exec(t))) {
            const r = document.createRange(); r.setStart(n, m.index); r.setEnd(n, m.index + m[0].length);
            const tops = new Set([...r.getClientRects()].filter(q => q.width > 0).map(q => Math.round(q.top)));
            if (tops.size > 1) res.push({ word: m[0], inCode: !!n.parentElement.closest('code'), tag: cell.tagName });
          }
        }
      }
      return res;
    });
  }
  fs.writeFileSync(OUTF, JSON.stringify(out, null, 1));
  for (const [f, r] of Object.entries(out)) {
    const plain = r.filter(x => !x.inCode && x.tag === 'TD'), th = r.filter(x => x.tag === 'TH'), code = r.filter(x => x.inCode);
    if (r.length) console.log(f, 'tdPlain=' + plain.length, 'th=' + th.length, 'code=' + code.length, '|', plain.slice(0, 12).map(x => x.word).join(' '));
  }
  await browser.close();
})();
