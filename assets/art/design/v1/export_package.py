"""Export the self-contained art delivery, keeping relative SVG/HTML links intact."""
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED
import json

ROOT=Path(__file__).resolve().parent
PROJECT=ROOT.parents[3]
DEST=PROJECT/'dist/WizardCard-art-v1.zip'
FILES=[PROJECT/'assets/art/runtime.json',PROJECT/'assets/art/provenance.json',PROJECT/'assets/art/README.md',
       PROJECT/'assets/cards.json',PROJECT/'assets/fonts/NotoSansCJKsc-Regular.otf',PROJECT/'assets/fonts/OFL.txt',
       PROJECT/'docs/art-resource-requirements.md',PROJECT/'docs/validation.md',PROJECT/'LICENSE']

def export():
    for folder in ['assets/art/cards','assets/art/ui','assets/art/design/v1']:
        FILES.extend(p for p in (PROJECT/folder).rglob('*') if p.is_file() and '__pycache__' not in p.parts and p.suffix!='.pyc')
    # Include documentation targets without copying old prototypes or build artifacts.
    FILES.extend(p for p in (PROJECT/'docs').glob('*.md') if p not in FILES)
    for name in ['README.md','CHANGELOG.md']:
        FILES.append(PROJECT/name)
    DEST.parent.mkdir(parents=True,exist_ok=True)
    with ZipFile(DEST,'w',ZIP_DEFLATED,compresslevel=6) as z:
        for p in sorted(set(FILES)):z.write(p,p.relative_to(PROJECT).as_posix())
        z.writestr('OPEN-ART-GALLERY.txt','Open assets/art/design/v1/index.html in a browser. Preserve the full directory tree for SVG and HTML links.\n')
    with ZipFile(DEST) as z:
        bad=z.testzip()
        if bad:raise RuntimeError('ZIP CRC failed: '+bad)
        names=z.namelist()
    print(json.dumps({'package':str(DEST),'files':len(names),'bytes':DEST.stat().st_size,'zip_crc':'pass'},ensure_ascii=False,indent=2))

if __name__=='__main__':export()
