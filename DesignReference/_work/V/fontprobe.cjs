// Re-measure overflowing boards with the web fonts the pages request (Barlow, JetBrains Mono, Noto Sans), through the proxy.
const { chromium } = require('playwright');
const fs=require('fs'),path=require('path');
const PROJ='/home/user/Outpost.Commander/DesignReference/canvas/project';
const canvas=JSON.parse(fs.readFileSync(path.join(PROJ,'canvas.json')));
(async()=>{
 const b=await chromium.launch({proxy:{server:process.env.HTTPS_PROXY}, args:['--ignore-certificate-errors']});
 const ctx=await b.newContext({ignoreHTTPSErrors:true}); const p=await ctx.newPage();
 await p.route('**/*',r=>{const u=new URL(r.request().url()); if(u.pathname.startsWith('/_blob/'))return r.fulfill({status:404,body:''}); if(u.hostname==='local'){const f=path.join(PROJ,u.pathname); if(u.pathname.endsWith('.js'))return r.fulfill({body:''}); return r.fulfill({path:f,contentType:'text/html'});} if(u.hostname.includes('fonts.g'))return r.continue(); return r.abort();});
 const files=process.argv.slice(2).length?process.argv.slice(2):Object.keys(canvas.boards).filter(f=>!f.startsWith('screen-'));
 for(const file of files){const bd=canvas.boards[file]; await p.setViewportSize({width:bd.w,height:900});
  await p.goto('http://local/'+file,{waitUntil:'networkidle'}); await p.evaluate(()=>document.fonts.ready);
  const r=await p.evaluate(()=>{let mr=0,mb=0;const root=document.querySelector('[data-root]');for(const el of root.querySelectorAll('*')){const b=el.getBoundingClientRect(); if(b.width||b.height){mr=Math.max(mr,b.right);mb=Math.max(mb,b.bottom);}} const fams=[...document.fonts].filter(f=>f.status==='loaded').map(f=>f.family); return {mr,mb,h:root.getBoundingClientRect().height,fams:[...new Set(fams)]};});
  console.log(file, 'right',Math.round(r.mr),'w',bd.w,'height',Math.round(r.h),'board',bd.h, r.mr>bd.w+1?'OVERFLOW':'', r.h>bd.h+1?'TALLER':'', JSON.stringify(r.fams));}
 await b.close();})();
