const { chromium } = require('playwright');
const fs=require('fs'),path=require('path');
const PROJ='/home/user/Outpost.Commander/DesignReference/canvas/project';
(async()=>{
 const b=await chromium.launch(); const p=await b.newPage({viewport:{width:1440,height:5570}});
 await p.route('**/*',r=>{const u=new URL(r.request().url()); if(u.hostname==='local'){const f=path.join(PROJ,u.pathname); if(u.pathname.endsWith('.js'))return r.fulfill({body:''}); return r.fulfill({path:f,contentType:'text/html'});} return r.abort();});
 await p.goto('http://local/'+process.argv[2]);
 const out=await p.evaluate(()=>{const res=[];for(const el of document.querySelectorAll('*')){const b=el.getBoundingClientRect(); if(b.right>1441 && el.children.length<3) res.push([el.tagName,Math.round(b.left),Math.round(b.right),Math.round(b.top),(el.textContent||'').trim().slice(0,60)]);} return res.slice(0,15);});
 console.log(JSON.stringify(out,null,0)); await b.close();})();
