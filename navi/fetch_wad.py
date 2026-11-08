"""Fetch the shareware WAD from doomgeneric's published browser demo.

The copyrighted game data is kept out of Git. Only the known v1.9 shareware
hash is accepted. The package and byte range are pinned in assets/manifest.json.
"""
from pathlib import Path
import hashlib,json,urllib.request
HERE=Path(__file__).resolve().parent
manifest=json.loads((HERE/'assets/manifest.json').read_text())
target=HERE/'assets/doom1.wad'
if target.exists():
    data=target.read_bytes()
else:
    with urllib.request.urlopen(manifest['source_package'],timeout=60) as response:
        package=response.read()
    assert hashlib.sha256(package).hexdigest()==manifest['package_sha256'],'package changed'
    span=manifest['package_range'];data=package[span['start_inclusive']:span['end_exclusive']]
assert len(data)==manifest['bytes']
assert hashlib.sha256(data).hexdigest()==manifest['sha256'],'not the pinned shareware WAD'
assert hashlib.md5(data).hexdigest()==manifest['expected_md5']
if not target.exists():target.write_bytes(data)
print(f'{target.name}: {len(data)} bytes, shareware SHA-256 verified')
