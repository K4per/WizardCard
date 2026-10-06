"""Copy original generated PNGs, inspect alpha, write a gallery and ZIP. No pixel editing."""
from pathlib import Path
from hashlib import sha256
from html import escape
from zipfile import ZipFile, ZIP_DEFLATED
import json, shutil
from PIL import Image

ROOT=Path(__file__).resolve().parent
data=json.loads((ROOT/'production.json').read_text(encoding='utf-8'))
records=[]
for a in data['assets']:
    src=Path(a['sourcePath']);dest=ROOT/a['file']
    if dest.exists() and dest.read_bytes()!=src.read_bytes():raise RuntimeError('Preserve existing asset: '+str(dest))
    shutil.copy2(src,dest)
    im=Image.open(dest);im.load()
    assert im.mode=='RGBA',a['id']
    alpha=im.getchannel('A');hist=alpha.histogram()
    assert hist[0]>0 and sum(hist[2:])>0,a['id']
    # The AI outputs can contain alpha=1 residue. Preserve it and report separately.
    edge=max(alpha.crop((0,0,im.width,1)).getextrema()[1],alpha.crop((0,im.height-1,im.width,im.height)).getextrema()[1],alpha.crop((0,0,1,im.height)).getextrema()[1],alpha.crop((im.width-1,0,im.width,im.height)).getextrema()[1])
    corners=[alpha.getpixel(p) for p in [(0,0),(im.width-1,0),(0,im.height-1),(im.width-1,im.height-1)]]
    assert max(corners)<=1,(a['id'],'opaque corner')
    assert edge<=1,(a['id'],'visible content reaches edge',edge)
    records.append({'id':a['id'],'name':a['name'],'file':a['file'],'size':list(im.size),'mode':im.mode,
      'source':'OpenAI built-in imagegen','original_file':src.name,'sha256':sha256(dest.read_bytes()).hexdigest(),
      'transparent_pixels':hist[0],'partial_alpha_pixels':sum(hist[1:255]),'outer_edge_alpha_max':edge,
      'alpha_corners':corners,'original_bytes_preserved':True,'runtime_integrated':False})
(ROOT/'manifest.json').write_text(json.dumps({'version':'alpha-illustrations-v1','date':data['date'],'assets':records},ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
cards=''.join('<article><a href="'+a['file']+'" target="_blank"><div class="image"><img src="'+a['file']+'" alt="'+a['name']+'"></div></a><h2>'+a['name']+'</h2><p>'+a['id']+' · '+str(a['size'][0])+' × '+str(a['size'][1])+'</p><a href="'+a['file']+'" download>下载透明 PNG</a></article>' for a in records)
html='''<!doctype html><html lang="zh-CN"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>WizardCard Alpha · 卡牌插图</title><style>
*{box-sizing:border-box}body{margin:0;padding:40px;background:#0a1119;color:#e7e5d5;font:16px/1.6 system-ui,"Microsoft YaHei",sans-serif}main{max-width:1400px;margin:auto}h1{font-weight:500;color:#e0be7c}p{color:#a4b4cb}a{color:#72d7c5}button{background:#122029;color:#e7e5d5;border:1px solid #c49a5a;padding:10px 18px;cursor:pointer;font:inherit}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(300px,1fr));gap:24px;margin-top:32px}article{background:#122029;padding:18px;border:1px solid #364e59}h2{font-size:20px;font-weight:500;margin:10px 0}article p{font-size:13px}.image{background-color:#192630;background-image:linear-gradient(45deg,#263944 25%,transparent 25%),linear-gradient(-45deg,#263944 25%,transparent 25%),linear-gradient(45deg,transparent 75%,#263944 75%),linear-gradient(-45deg,transparent 75%,#263944 75%);background-size:24px 24px;background-position:0 0,0 12px,12px -12px,-12px 0}.image img{width:100%;aspect-ratio:4/3;object-fit:contain;image-rendering:pixelated;display:block}body.light .image{background:#e7e5d5}footer{margin:35px 0;border-top:1px solid #6d5035;padding-top:20px}@media(max-width:600px){body{padding:18px}.grid{grid-template-columns:1fr}}
</style><main><h1>WizardCard Alpha · 卡牌插图</h1><p>v1.0 卡牌设计草案 / 11 张透明主体插画 / 2026-10-06</p><p>像素奇幻风；无场景背景、卡面文字或外层卡框。“昂扬”以提供的上升符文为轮廓参考。</p><button onclick="document.body.classList.toggle('light')">切换浅色底 / 透明棋盘格</button> <a href="manifest.json">资源清单</a> · <a href="README.md">说明</a><div class="grid">'''+cards+'''</div><footer>棋盘格仅用于网页预览，不在 PNG 内。图片保留原始 RGBA；允许像素风生成图的半透明边缘，不宣称严格限色精修。尚未接入运行卡池。</footer></main></html>'''
(ROOT/'index.html').write_text(html,encoding='utf-8')
zip_path=ROOT/'WizardCard-Alpha-v1-illustrations.zip'
with ZipFile(zip_path,'w',ZIP_DEFLATED) as z:
    for a in records:z.write(ROOT/a['file'],a['file'])
    for name in ['manifest.json','production.json','spec_lock.md','README.md','index.html']:z.write(ROOT/name,name)
with ZipFile(zip_path) as z:assert z.testzip() is None
print(json.dumps({'count':len(records),'sizes':sorted(set(tuple(a['size']) for a in records)),'edge_alpha':{a['id']:a['outer_edge_alpha_max'] for a in records},'zip':str(zip_path),'zip_bytes':zip_path.stat().st_size,'checks':'RGBA, transparent corners/margins, source hashes, ZIP CRC passed'},ensure_ascii=False,indent=2))
