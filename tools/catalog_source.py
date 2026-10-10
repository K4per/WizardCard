"""Read the explicit canonical catalog; emit the legacy aggregate for consumers."""
from pathlib import Path
import json

ROOT = Path(__file__).resolve().parents[1]

def load_catalog(directory=ROOT / 'assets'):
    directory = Path(directory).resolve()
    manifest_path = directory / 'catalog.json'
    if not manifest_path.exists():
        return json.loads((directory / 'cards.json').read_text(encoding='utf-8-sig'))
    manifest = json.loads(manifest_path.read_text(encoding='utf-8-sig'))
    if manifest.get('format') != 1 or not isinstance(manifest.get('metadata'), dict) or 'cards' in manifest['metadata']:
        raise ValueError('Invalid catalog manifest')
    entries = manifest.get('definitions')
    if not isinstance(entries, list) or not entries:
        raise ValueError('Catalog definitions must be a nonempty list')
    cards, ids, paths = [], set(), set()
    for entry in entries:
        card_id, filename = entry['id'], entry['file']
        if not isinstance(card_id, str) or not card_id or card_id in ids:
            raise ValueError('Duplicate or invalid card ID')
        if not isinstance(filename, str) or ':' in filename or '\\' in filename or any(p in ('.', '..') for p in filename.split('/')):
            raise ValueError('Invalid relative definition path')
        relative = Path(filename)
        if relative.is_absolute() or relative.suffix != '.json':
            raise ValueError('Definition must be relative JSON')
        path = (directory / relative).resolve()
        if not path.is_relative_to(directory) or path in paths:
            raise ValueError('Definition escapes catalog or repeats a file')
        card = json.loads(path.read_text(encoding='utf-8-sig'))
        if card.get('id') != card_id:
            raise ValueError('Definition ID disagrees with manifest')
        ids.add(card_id)
        paths.add(path)
        cards.append(card)
    return dict(manifest['metadata'], cards=cards)

def main():
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true', help='Fail if compatibility aggregate is stale')
    args = parser.parse_args()
    aggregate = ROOT / 'assets' / 'cards.json'
    catalog = load_catalog()
    if args.check:
        if json.loads(aggregate.read_text(encoding='utf-8-sig')) != catalog:
            raise SystemExit('cards.json is stale; run python tools/catalog_source.py')
        print('Canonical catalog matches compatibility aggregate')
    else:
        aggregate.write_text(json.dumps(catalog, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')

if __name__ == '__main__':
    main()
