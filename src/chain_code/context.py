from __future__ import annotations

from pathlib import Path

SKIP_DIRS = {".git", ".venv", "node_modules", "dist", "build", "__pycache__"}
TEXT_EXT = {".py", ".md", ".txt", ".toml", ".json", ".yaml", ".yml", ".sh", ".js", ".ts", ".rs", ".go", ".c", ".h", ".cpp"}


def collect_context(root: Path, max_files: int = 8) -> str:
    chunks: list[str] = []
    count = 0
    for p in root.rglob("*"):
        if count >= max_files:
            break
        if not p.is_file():
            continue
        if any(part in SKIP_DIRS for part in p.parts):
            continue
        if p.suffix.lower() not in TEXT_EXT:
            continue
        try:
            text = p.read_text(encoding="utf-8", errors="ignore")
        except OSError:
            continue
        rel = p.relative_to(root)
        snippet = text[:2000]
        chunks.append(f"\n### FILE: {rel}\n{snippet}")
        count += 1
    return "\n".join(chunks) if chunks else "(no context files detected)"
