const fs=require('fs');const path=require('path');
const {chromium}=require('C:/Users/home_/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/playwright');
const root=path.resolve(__dirname,'../..');
(async()=>{
const browser=await chromium.launch({headless:true,executablePath:'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',args:['--allow-file-access-from-files']});
const page=await browser.newPage();const errors=[];page.on('pageerror',e=>errors.push(String(e)));
await page.goto('file:///'+path.join(root,'index.html').replaceAll('\\','/'));await page.waitForFunction(()=>window.reviewReady);await page.evaluate(()=>document.fonts.ready);
const states=await page.evaluate(()=>window.REVIEW.states);const checks=[];const shots=path.join(root,'concepts/c-pixel-v1/verified');fs.mkdirSync(shots,{recursive:true});
for(const [width,height] of [[1920,1080],[1280,720],[1600,1000]]){
 await page.setViewportSize({width,height});
 for(const [group,values] of Object.entries(states))for(const state of values){
  await page.evaluate(({group,state})=>{document.querySelector('#page').value=group;document.querySelector('#page').dispatchEvent(new Event('change'));document.querySelector('#state').value=state;window.REVIEW.render();},{group,state});
  await page.evaluate(()=>Promise.all(Array.from(document.images).map(i=>i.complete?null:new Promise(r=>{i.onload=r;i.onerror=r}))));
  const result=await page.evaluate(()=>({horizontalOverflow:document.documentElement.scrollWidth>innerWidth+1,brokenImages:Array.from(document.images).filter(i=>!i.complete||!i.naturalWidth).map(i=>i.getAttribute('src')),detailClipping:Array.from(document.querySelectorAll('.detail-card')).filter(e=>{const a=e.getBoundingClientRect(),b=e.closest('.panel').getBoundingClientRect();return a.left<b.left+11||a.right>b.right-11}).length,heading:document.querySelector('h1,h2')?.textContent}));
  checks.push({width,height,group,state,...result});
  if(['menu','builder'].includes(group)&&state===values[0]||group==='battle'&&['complex','recipe','facedown-zero','danger','exhaustion'].includes(state))await page.screenshot({path:path.join(shots,`${group}-${state}-${width}x${height}.png`),fullPage:true});
 }
}
await page.evaluate(()=>document.querySelector('#assetBtn').click());await page.waitForTimeout(1000);
const assetImages=await page.evaluate(()=>({brokenImages:Array.from(document.images).filter(i=>!i.complete||!i.naturalWidth).map(i=>i.src),count:document.querySelectorAll('.asset-item').length}));
await page.screenshot({path:path.join(shots,'asset-gallery-top.png'),fullPage:false});
const report={ok:errors.length===0&&checks.every(c=>!c.horizontalOverflow&&!c.brokenImages.length&&!c.detailClipping)&&!assetImages.brokenImages.length,resolutions:[[1920,1080],[1280,720],[1600,1000]],pageGroups:Object.keys(states).length,states:Object.values(states).reduce((n,a)=>n+a.length,0),checks:checks.length,errors,failures:checks.filter(c=>c.horizontalOverflow||c.brokenImages.length||c.detailClipping),assetImages,scope:'Browser design assembly, not Godot game logic or human input acceptance'};
fs.writeFileSync(path.join(root,'manifests/c-pixel-v1/browser-validation.json'),JSON.stringify(report,null,2)+'\n');console.log(JSON.stringify(report));await browser.close();if(!report.ok)process.exitCode=1;
})().catch(e=>{console.error(e);process.exitCode=1});
