"""Index existing assets without moving consumers; validate and package this add-on."""
from pathlib import Path
import json, hashlib, shutil, zipfile, re
from PIL import Image
import importlib.util

HERE=Path(__file__).resolve().parent
P=HERE.parents[3];ART=P/'assets/art';OUT=ART/'nonmodel/a-gilded-v15';GODOT=P/'godot/art/nonmodel/a-gilded-v15'

def read(path):return json.loads(path.read_text(encoding='utf-8'))
def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def write(path,data):path.write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n',encoding='utf-8',newline='\n')

def main():
    manifest=read(OUT/'manifest.json'); old=read(ART/'ui/a-gilded-v2/manifest.json'); runtime=read(ART/'runtime.json')
    spec=importlib.util.spec_from_file_location('nonmodel_builder',HERE/'build_assets.py')
    builder=importlib.util.module_from_spec(spec);spec.loader.exec_module(builder)
    builder.entries.extend(manifest['assets']);builder.gallery()
    errors=[];seen=set();artifacts=[]
    for entry in manifest['assets']:
        id=entry['id'];path=ART/entry['file']; engine=P/'godot'/entry['engine'].removeprefix('res://')
        if id in seen or id in {e['id'] for e in old['assets']}:errors.append('duplicate '+id)
        seen.add(id)
        if digest(path)!=entry['sha256'] or digest(engine)!=entry['sha256']:errors.append('checksum '+id)
        im=Image.open(path)
        if list(im.size)!=entry['size'] or im.mode!=entry['mode']:errors.append('format '+id)
        if entry['requirement'] in ('MAT-02','UI-01','UI-02','UI-03','ANI-02') or '/vfx/' in entry['file']:
            if im.mode!='RGBA' or im.getchannel('A').getextrema()[0]!=0:errors.append('alpha '+id)
        if 'insets' in entry:
            a,b,c,d=entry['insets']
            if a+c>=im.width or b+d>=im.height:errors.append('slice '+id)
        if '/sheets/' in entry['file'] and im.size!=(entry['frames']*256,256):errors.append('frames '+id)
    for path in sorted(GODOT.rglob('*')):
        if not path.is_file() or path.suffix in ('.png','.import','.uid') or path.name=='manifest.json':continue
        relative=path.relative_to(GODOT);target=OUT/relative;target.parent.mkdir(parents=True,exist_ok=True)
        # Preserve a standalone source mirror for artist handoff.
        shutil.copy2(path,target)
        artifacts.append({'file':str(target.relative_to(ART)).replace('\\','/'),'engine':'res://'+str(path.relative_to(P/'godot')).replace('\\','/'),'sha256':digest(path)})
    manifest['artifacts']=artifacts;manifest['counts']={'png':len(seen),'scene_material_sets':4,'paper_edge_set':1,'effect_scenes':15,'svg_sources':len(list((HERE/'sources').rglob('*.svg'))),'animation_clips':20}
    write(OUT/'manifest.json',manifest);write(GODOT/'manifest.json',manifest)
    packs=[]
    for key,path,status in [('A 古籍金饰 v2',ART/'ui/a-gilded-v2/manifest.json','current card/UI base'),('v1.5 非建模增补',OUT/'manifest.json','current scene/material/effect supplement'),('C 版',ART/'ui/c-theme-v1/manifest.json','historical style draft'),('早期像素 UI',ART/'ui/manifest.json','historical SDL client base')]:
        data=read(path);packs.append({'name':key,'status':status,'manifest':str(path.relative_to(P)).replace('\\','/'),'assetCount':len(data['assets']),'sha256':hashlib.sha256(path.read_text(encoding='utf-8').encode('utf-8')).hexdigest(),'hashEncoding':'UTF-8 LF; Git text normalization'})
    current={'date':'2026-10-07','requirements':'docs/art-resource-requirements-godot-v1.5.md','theme':'A 古籍金饰; legendary gems golden yellow',
             'cardIllustrations':{'count':len(runtime['cards']),'mapping':'assets/art/runtime.json','files':[{'id':id,'file':file,'sha256':digest(ART/file)} for id,file in runtime['cards'].items()]},
             'packs':packs,'branding':'assets/art/branding/icon-manifest.json','fonts':'assets/fonts/NotoSansCJKsc-Regular.otf',
             'audio':'assets/audio/manifest.json','statuses':'docs/art-resource-status-v1.5.md','gallery':'assets/art/design/nonmodel-v15/gallery.html',
             'organization':'index only; no loaded asset moved or deleted','modelRequirementsDeferred':['MOD-01','MOD-02','MOD-03','MOD-04','PRP-01']}
    write(ART/'catalog.json',current)
    integration=read(P/'godot/art-integration.json')
    integration['nonmodelPack']={'manifest':'assets/art/nonmodel/a-gilded-v15/manifest.json','engineRoot':'res://art/nonmodel/a-gilded-v15','assetPngCount':len(seen),
      'materialsIntegrated':['dark_wood','leather','antique_gold','paper_edge','card_face','card_back'],'materialReadyForModels':['environment_stone'],
      'effectCueMapping':{'2':'analysis_complete','3':'prepare','4':'release','5':'prepare (response declaration)','6':'counter (cancel/invalid)','7':'damage','8':'heal','9':'mana','10':'load'},
      'standaloneEffects':['analysis_start','analysis_loop','attach','destroy','leave'],
      'note':'Resources complete; standalone effects/AnimationLibrary ready for consumer hooks. No modeling delivered.',
      'previewScene':'res://scenes/art/nonmodel_gallery.tscn','status':'docs/art-resource-status-v1.5.md'}
    integration['layout']['match']='current 2.5D mirrored five zones; fixed slanted camera; left detail/right chain-log/bottom hand and independent action'
    integration['evidence']='assets/art/design/nonmodel-v15/previews/engine'
    write(P/'godot/art-integration.json',integration)
    for e in read(HERE/'existing-assets-validation.json')['errors']:errors.append('existing: '+e)
    logs={}
    for name in ('art-nonmodel-import.log','art-nonmodel-headless-smoke.log','art-nonmodel-effects-smoke.log','art-nonmodel-capture.log','art-nonmodel-effects-capture.log'):
        path=P/'build'/name
        content=path.read_text(encoding='utf-8',errors='replace') if path.exists() else ''
        logs[name]={'present':path.exists(),'errors':bool(re.search(r'(^|\n)(?:SCRIPT ERROR|ERROR:)',content)),'sha256':digest(path) if path.exists() else None}
        if not content or logs[name]['errors']:errors.append('diagnostic '+name)
    report={'ok':not errors,'counts':manifest['counts'],'originalPackPng':len(old['assets']),'cardIllustrations':len(runtime['cards']),
      'sourceEngineCopies':'all PNG SHA256 identical','nativeSources':'procedural generator + SVG; AI original images not edited',
      'logs':logs,'gpuInputSimulation':'failed in desktop window; headless input smoke passes; human GPU/DPI acceptance pending',
      'fullGamePerformance':'not verified','errors':errors}
    write(HERE/'validation.json',report)
    # Human-friendly file list includes channels and where to consume them.
    lines=['# 非建模资源文件清单','',f'新增 {len(seen)} PNG，{len(artifacts)} 个引擎配置资源，{manifest["counts"]["svg_sources"]} SVG 源稿。原 A 版314 PNG、30卡插画保留。','',
      '| ID | 需求 | 尺寸 | 文件 |','|---|---|---|---|']
    for e in manifest['assets']:lines.append(f'| {e["id"]} | {e["requirement"]} | {e["size"][0]}×{e["size"][1]} | [{Path(e["file"]).name}](../../{e["file"]}) |')
    lines+=['','## 引擎配置','']+[f'- `{e["engine"]}`' for e in artifacts]
    (HERE/'ASSET_LIST.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
    # Include sources and review screenshots; do not include previous huge deliveries or editor caches.
    dist=HERE/'dist';dist.mkdir(exist_ok=True);zip_path=dist/'WizardCard-A-v15-nonmodel.zip'
    candidates=[]
    for root in (OUT,GODOT,HERE):
        for path in root.rglob('*'):
            if path.is_file() and not any(part in ('dist','__pycache__') for part in path.parts) and path.suffix not in ('.import','.uid') and path.name not in ('debug_smoke.py',):candidates.append(path)
    candidates += [ART/'catalog.json',ART/'README.md',P/'docs/art-resource-status-v1.5.md',P/'godot/art-integration.json',
       P/'godot/scripts/art/wizard_art_effect_3d.gd',P/'godot/scripts/art/wizard_art_gallery.gd',P/'godot/scenes/art/nonmodel_gallery.tscn',P/'godot/scenes/art/card_motion_sample.tscn']
    candidates += [P/name for name in ['godot/scripts/ui/wizard_skin.gd','godot/scripts/ui/wizard_audio.gd','godot/scripts/match/wizard_zone_3d.gd','godot/scripts/match/wizard_card_3d.gd','godot/scripts/match/wizard_battlefield.gd','godot/scripts/match/wizard_hand_card.gd','godot/scripts/match/wizard_hand_fan.gd','godot/scripts/match/wizard_match_view.gd','godot/scenes/match/table_3d.tscn','godot/scenes/match/battlefield.tscn','godot/tests/art_nonmodel_smoke.gd','godot/tests/capture_art_effects.gd','godot/tests/capture_art_nonmodel.gd']]
    with zipfile.ZipFile(zip_path,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as archive:
        for path in sorted(set(candidates)):archive.write(path,path.relative_to(P))
    write(dist/'package.json',{'file':zip_path.name,'bytes':zip_path.stat().st_size,'sha256':digest(zip_path),'type':'add-on asset/source review package; depends on existing WizardCard project and A v2 art pack','validation':report['ok']})
    print(json.dumps({'validation':report['ok'],'newPNG':len(seen),'sourceSVG':manifest['counts']['svg_sources'],'artifactCount':len(artifacts),'packageBytes':zip_path.stat().st_size,'errors':errors},ensure_ascii=False))
    if errors:raise SystemExit(1)

if __name__=='__main__':main()
