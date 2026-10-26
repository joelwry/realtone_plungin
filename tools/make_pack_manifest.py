#!/usr/bin/env python3
"""Builds a RealTone library manifest from a pinned upstream repository.

For every instrument listed in the config it downloads the .sfz from the pinned commit, finds
the samples it references, downloads those too (into a cache folder, never into the repo) and
records the size and SHA-256 of each file. The manifest that comes out is what the installer
verifies downloads against.

    python3 tools/make_pack_manifest.py tools/vsco2ce_instruments.json \
        libraries/manifests/vsco2ce.json --cache ~/.cache/realtone-packs [--dry-run]
"""

import argparse
import concurrent.futures
import hashlib
import json
import re
import sys
import urllib.parse
import urllib.request
from pathlib import Path, PurePosixPath

RAW = "https://raw.githubusercontent.com/{repo}/{commit}/{path}"


def fetch(url, dest, tries=4):
    dest.parent.mkdir(parents=True, exist_ok=True)
    last = None
    for _ in range(tries):
        try:
            with urllib.request.urlopen(url, timeout=120) as r, open(dest.with_suffix(dest.suffix + ".part"), "wb") as f:
                while True:
                    chunk = r.read(1 << 20)
                    if not chunk:
                        break
                    f.write(chunk)
            dest.with_suffix(dest.suffix + ".part").replace(dest)
            return
        except Exception as e:  # noqa: BLE001 - retry any transport error
            last = e
    raise RuntimeError(f"download failed: {url}: {last}")


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def sample_paths(sfz_text):
    """Mirrors the engine's reading of default_path and sample= (comments stripped)."""
    text = re.sub(r"//[^\n]*", "", sfz_text)
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    default_path = ""
    m = re.search(r"default_path=([^\n<]*)", text)
    if m:
        default_path = m.group(1).strip().replace("\\", "/")
        if default_path and not default_path.endswith("/"):
            default_path += "/"
    found = []
    for m in re.finditer(r"\bsample=([^\n<]*?)(?=\s+[A-Za-z_][A-Za-z0-9_]*=|\s*<|\s*$)", text, flags=re.M):
        name = m.group(1).strip().replace("\\", "/")
        if name and not name.startswith("*"):
            found.append(str(PurePosixPath(default_path + name)))
    return sorted(set(found))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("config")
    ap.add_argument("output")
    ap.add_argument("--cache", required=True)
    ap.add_argument("--dry-run", action="store_true", help="only list the files and sizes")
    ap.add_argument("--jobs", type=int, default=8)
    args = ap.parse_args()

    cfg = json.loads(Path(args.config).read_text())
    pack = cfg["pack"]
    cache = Path(args.cache) / pack["id"] / pack["commit"]

    def url_for(rel):
        return RAW.format(repo=pack["repo"], commit=pack["commit"], path=urllib.parse.quote(rel))

    # 1. sfz files and the samples they use
    inst_files = {}
    for inst in cfg["instruments"]:
        sfz_local = cache / inst["sfz"]
        if not sfz_local.exists():
            fetch(url_for(inst["sfz"]), sfz_local)
        samples = sample_paths(sfz_local.read_text(encoding="utf-8", errors="replace"))
        if not samples:
            sys.exit(f"{inst['sfz']}: no samples found")
        inst_files[inst["id"]] = [inst["sfz"]] + samples

    all_files = sorted({f for files in inst_files.values() for f in files})
    print(f"{len(cfg['instruments'])} instruments, {len(all_files)} unique files")

    # 2. download and hash
    def work(rel):
        local = cache / rel
        if not local.exists():
            fetch(url_for(rel), local)
        head = local.read_bytes()[:16] if local.suffix == ".wav" else b""
        if local.suffix == ".wav" and not head.startswith(b"RIFF"):
            raise RuntimeError(f"{rel} is not a WAV file (LFS pointer or error page?)")
        return rel, local.stat().st_size, sha256(local)

    if args.dry_run:
        total = 0
        for rel in all_files:
            req = urllib.request.Request(url_for(rel), method="HEAD")
            with urllib.request.urlopen(req, timeout=60) as r:
                total += int(r.headers.get("Content-Length", 0))
        print(f"total download: {total / 1e6:.1f} MB")
        return

    records = {}
    with concurrent.futures.ThreadPoolExecutor(args.jobs) as ex:
        for i, (rel, size, digest) in enumerate(ex.map(work, all_files), 1):
            records[rel] = {"path": rel, "size": size, "sha256": digest}
            if i % 50 == 0 or i == len(all_files):
                print(f"  {i}/{len(all_files)}")

    manifest = {
        "schema": 1,
        "id": pack["id"],
        "name": pack["name"],
        "version": pack["version"],
        "license": pack["license"],
        "source": {
            "homepage": pack["homepage"],
            "baseUrl": f"https://raw.githubusercontent.com/{pack['repo']}/{pack['commit']}/",
            "allowedHosts": ["raw.githubusercontent.com"],
        },
        "files": [records[k] for k in sorted(records)],
        "instruments": [
            {
                "id": i["id"], "name": i["name"], "category": i["category"], "family": i["family"],
                "description": i["description"], "sfz": i["sfz"], "tags": i.get("tags", []),
                "files": inst_files[i["id"]],
            }
            for i in cfg["instruments"]
        ],
    }
    Path(args.output).write_text(json.dumps(manifest, indent=1) + "\n")
    total = sum(r["size"] for r in records.values())
    print(f"wrote {args.output}: {len(records)} files, {total / 1e6:.1f} MB")


if __name__ == "__main__":
    main()
