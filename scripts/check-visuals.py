#!/usr/bin/env python3
"""Validate README/gallery references and reproducible visual companions."""
from __future__ import annotations

import argparse
import os
import re
import struct
import sys
from pathlib import Path
from urllib.parse import urlparse

PNG = b"\x89PNG\r\n\x1a\n"
GIF = (b"GIF87a", b"GIF89a")


def local(raw: str) -> str | None:
    target = raw.strip().strip("<>")
    parsed = urlparse(target)
    if not target or target.startswith(("#", "/", "//")) or parsed.scheme or parsed.netloc:
        return None
    return parsed.path


def info(path: Path) -> tuple[str, int, int, int]:
    data = path.read_bytes()
    if data.startswith(PNG):
        if len(data) < 24 or data[12:16] != b"IHDR":
            raise ValueError("truncated PNG")
        width, height = struct.unpack(">II", data[16:24])
        if not width or not height or len(data) > 300 * 1024:
            raise ValueError("invalid dimensions or PNG exceeds 300 KiB")
        return "PNG", width, height, len(data)
    if data.startswith(GIF):
        if len(data) < 10:
            raise ValueError("truncated GIF")
        width, height = struct.unpack("<HH", data[6:10])
        if not width or not height or len(data) > 3 * 1024 * 1024:
            raise ValueError("invalid dimensions or GIF exceeds 3 MiB")
        return "GIF", width, height, len(data)
    raise ValueError("unrecognised PNG/GIF magic bytes")


def targets(text: str) -> set[str]:
    raw = re.findall(r"!?(?:\[[^\]]*\])\(([^)\s]+)(?:\s+[^)]*)?\)", text)
    raw += re.findall(r"<img\b[^>]*?\bsrc=[\"']([^\"']+)", text, re.I)
    return {value for item in raw if (value := local(item))}


def images(text: str) -> set[str]:
    raw = re.findall(r"!\[[^\]]*\]\(([^)\s]+)(?:\s+[^)]*)?\)", text)
    return {value for item in raw if (value := local(item))}


def check_doc(root: Path, path: Path, errors: list[str]) -> None:
    for target in sorted(targets(path.read_text(encoding="utf-8"))):
        resolved = (path.parent / target).resolve()
        try:
            resolved.relative_to(root.resolve())
        except ValueError:
            errors.append(f"{path.relative_to(root)} reference escapes repository: {target}")
            continue
        if not resolved.exists():
            errors.append(f"{path.relative_to(root)} reference does not exist: {target}")
        elif resolved.is_file() and resolved.suffix.lower() in (".png", ".gif"):
            try:
                kind, width, height, size = info(resolved)
                print(f"visual: {path.relative_to(root)}:{target} ({kind} {width}x{height}, {size} bytes)")
            except (OSError, ValueError) as exc:
                errors.append(f"invalid {path.relative_to(root)} image {target}: {exc}")


def companions(root: Path, source: Path) -> list[Path]:
    rel = source.relative_to(root / "examples").with_suffix("")
    names = [rel.name, str(rel).replace(os.sep, "-")]
    return [root / "docs" / "images" / f"{name}{suffix}" for name in dict.fromkeys(names) for suffix in (".png", ".gif")]


def check(root: Path) -> list[str]:
    errors: list[str] = []
    readme = root / "README.md"
    if not readme.is_file():
        return ["README.md is missing"]
    image_root = root / "docs" / "images"
    if image_root.is_dir():
        for path in sorted(image_root.rglob("*")):
            if path.is_file() and path.suffix.lower() in (".png", ".gif"):
                try:
                    info(path)
                except (OSError, ValueError) as exc:
                    errors.append(f"invalid generated image {path.relative_to(root)}: {exc}")
    check_doc(root, readme, errors)
    gallery = root / "docs" / "gallery.md"
    if gallery.is_file():
        check_doc(root, gallery, errors)
    else:
        errors.append("docs/gallery.md is missing")
    preamble = readme.read_text(encoding="utf-8").split("\n## ", 1)[0]
    count = len(images(preamble))
    if not 3 <= count <= 20:
        errors.append(f"README gallery must contain 3-20 images before first ## heading (found {count})")
    sources = re.findall(r"\((examples/[^)\s]+\.lms)\)", preamble)
    autoruns = re.findall(r"lmctl(?:\s+autorun)?\s+\S+\.lms", preamble)
    if len(sources) < count:
        errors.append(f"README gallery has {count} images but only {len(sources)} linked example sources")
    if len(autoruns) < count:
        errors.append(f"README gallery has {count} images but only {len(autoruns)} autorun commands")
    for source in sorted((root / "examples").rglob("*.lms")):
        candidate = next((path for path in companions(root, source) if path.is_file()), None)
        if candidate is None:
            errors.append(f"example has no companion visual: {source.relative_to(root)}")
        else:
            try:
                info(candidate)
            except (OSError, ValueError) as exc:
                errors.append(f"invalid companion {candidate.relative_to(root)}: {exc}")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("root", nargs="?", type=Path, default=Path(__file__).resolve().parents[1])
    errors = check(parser.parse_args().root.resolve())
    for error in errors:
        print(f"visual check: ERROR: {error}", file=sys.stderr)
    if errors:
        return 1
    print("visual check: all README references and example companions are valid")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
