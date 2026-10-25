const { chromium } = require('playwright');
const fs=require('fs'),path=require('path');
const PROJ='/home/user/Outpost.Commander/DesignReference/canvas/project';
const ASSETS=JSON.parse(fs.readFileSync('/home/user/Outpost.Commander/DesignReference/canvas/assets.json'));const byBlob={};for(const[k,v]of Object.entries(ASSETS))byBlob[v]='/home/user/Outpost.Commander/DesignReference/_work/assets/'+k;
(async()=>{
 const [file,x,y,w,h,vw]=process.argv.slice(2);
 const b=await chromium.launch({proxy:{server:process.env.HTTPS_PROXY}}); const ctx=await b.newContext({ignoreHTTPSErrors:true,viewport:{width:+vw||1440,height:+y + +h}}); const p=await ctx.newPage();
 await p.route('**/*',r=>{const u=new URL(r.request().url()); if(u.pathname.startsWith('/_blob/'))return r.fulfill({path:byBlob[u.pathname]}); if(u.hostname==='local'){if(u.pathname.endsWith('.js'))return r.fulfill({body:''});return r.fulfill({path:path.join(PROJ,u.pathname),contentType:'text/html'});} if(u.hostname.includes('fonts.g'))return r.continue(); return r.abort();});
 await p.goto('http://local/'+file,{waitUntil:'networkidle'}); await p.evaluate(()=>document.fonts.ready); await p.waitForTimeout(300);
 await p.screenshot({path:'render/F_'+file.replace('.dc.html','')+`_${x}_${y}.png`,clip:{x:+x,y:+y,width:+w,height:+h}}); await b.close();})();
