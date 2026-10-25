const { chromium } = require('playwright');
const fs=require('fs'),path=require('path');
const PROJ='/home/user/Outpost.Commander/DesignReference/canvas/project';
const ASSETS=JSON.parse(fs.readFileSync('/home/user/Outpost.Commander/DesignReference/canvas/assets.json'));const byBlob={};for(const[k,v]of Object.entries(ASSETS))byBlob[v]='/home/user/Outpost.Commander/DesignReference/_work/assets/'+k;
(async()=>{
 const [file,x,y,w,h,vw]=process.argv.slice(2); const b=await chromium.launch(); const p=await b.newPage({viewport:{width:+vw||1440,height:+y + +h}});
 await p.route('**/*',r=>{const u=new URL(r.request().url()); if(u.pathname.startsWith('/_blob/'))return r.fulfill({path:byBlob[u.pathname]}); if(u.hostname==='local'){const f=path.join(PROJ,u.pathname); if(u.pathname.endsWith('.js'))return r.fulfill({body:''}); return r.fulfill({path:f,contentType:'text/html'});} return r.abort();});
 await p.goto('http://local/'+file); await p.waitForTimeout(200);
 await p.screenshot({path:'render/'+file.replace('.dc.html','')+`_${x}_${y}.png`,clip:{x:+x,y:+y,width:+w,height:+h}}); await b.close();})();
