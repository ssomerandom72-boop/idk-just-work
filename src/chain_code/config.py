from __future__ import annotations

from pathlib import Path
import tomllib
import tomli_w

DEFAULT_CONFIG = {
    "provider": "ollama",
    "model": "codellama:13b",
    "temperature": 0.2,
    "max_context_files": 8,
    "approve_shell": True,
}


def config_path() -> Path:
    return Path.home() / ".config" / "chain-code" / "config.toml"


def load_config() -> dict:
    path = config_path()
    if not path.exists():
        return DEFAULT_CONFIG.copy()
    with path.open("rb") as f:
        data = tomllib.load(f)
    merged = DEFAULT_CONFIG.copy()
    merged.update(data)
    return merged


def save_config(cfg: dict) -> None:
    path = config_path()
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("wb") as f:
        f.write(tomli_w.dumps(cfg).encode("utf-8"))
