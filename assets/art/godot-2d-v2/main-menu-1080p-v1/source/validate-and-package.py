"""Read-only image checks and archive construction; no image manipulation."""
import hashlib
import json
import zipfile
from pathlib import Path
from PIL import Image

BATCH = Path(__file__).resolve().parents[1]
PROJECT = BATCH.parents[3]

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    manifest = json.loads((BATCH / 'manifest.json').read_text(encoding='utf-8-sig'))
    report = {'ok': True, 'designSize': [1920,1080], 'assets': [], 'previews': []}
    for asset in manifest['assets']:
        path = BATCH / asset['file']
        with Image.open(path) as source:
            image = source.convert('RGBA')
            alpha = image.getchannel('A')
            hist = alpha.histogram()
            valid = image.size == (1920,1080) and sha(path) == asset['sha256'] and sha(BATCH / asset['source']) == asset['sourceSha256']
            valid &= (hist[0]>0 and hist[255]>0) if asset['transparent'] else hist[255]==1920*1080
            report['assets'].append({'id':asset['id'],'ok':valid,'size':list(image.size),'alphaRange':list(alpha.getextrema()),'alphaBounds':alpha.getbbox(),'sha256':sha(path)})
            report['ok'] &= valid
    preview_names = ['preview-1080p.png','button-states-1080p.png']
    if (BATCH/'godot-menu-1080p.png').exists():
        preview_names.append('godot-menu-1080p.png')
    for name in preview_names:
        with Image.open(BATCH/name) as image:
            valid = image.size == (1920,1080)
            report['previews'].append({'file':name,'ok':valid,'size':list(image.size),'sha256':sha(BATCH/name)})
            report['ok'] &= valid
    report['browser'] = json.loads((BATCH/'preview-validation.json').read_text(encoding='utf-8'))
    report['ok'] &= report['browser']['ok']
    (BATCH/'validation.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    if not report['ok']:
        raise RuntimeError('Asset validation failed')
    output = PROJECT/'build/art-delivery/WizardCard-MainMenu-1080p-v1.zip'
    output.parent.mkdir(parents=True,exist_ok=True)
    files = [p for p in BATCH.rglob('*') if p.is_file() and p.name!='package.json' and '__pycache__' not in p.parts]
    files += [PROJECT/'assets/fonts/NotoSansCJKsc-Regular.otf',PROJECT/'assets/fonts/OFL.txt',PROJECT/'assets/art/RESOURCE_INDEX.md',PROJECT/'assets/art/ART_INVENTORY.json',PROJECT/'docs/art-resource-requirements-godot-2d.md']
    integration_report = PROJECT/'docs/reports/main-menu-art-integration.md'
    if integration_report.exists():
        files.append(integration_report)
    with zipfile.ZipFile(output,'w',zipfile.ZIP_DEFLATED) as archive:
        for path in files:
            archive.write(path,path.relative_to(PROJECT).as_posix())
    data = {'ok':True,'file':output.relative_to(PROJECT).as_posix(),'bytes':output.stat().st_size,'sha256':sha(output),'productionPng':7,'previewPng':len(preview_names),'layout':'Extract into project root; includes font and its license'}
    (BATCH/'package.json').write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps(data,ensure_ascii=False))

if __name__ == '__main__':
    main()
