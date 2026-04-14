from __future__ import annotations

import subprocess
from pathlib import Path

import click
from rich.console import Console
from rich.panel import Panel

from .config import DEFAULT_CONFIG, load_config, save_config
from .context import collect_context
from .providers import ask_model

console = Console()


def _build_prompt(user_prompt: str, cfg: dict) -> str:
    ctx = collect_context(Path.cwd(), max_files=int(cfg.get("max_context_files", 8)))
    system = (
        "You are Chain Code, an Arch Linux oriented coding assistant. "
        "Return concise, actionable help with command examples where useful."
    )
    return f"{system}\n\nProject context:\n{ctx}\n\nUser request:\n{user_prompt}"


@click.group()
def main() -> None:
    """Chain Code - coding assistant for Arch Linux systems."""


@main.command()
def init() -> None:
    """Create default config file."""
    cfg = DEFAULT_CONFIG.copy()
    save_config(cfg)
    console.print("[green]Initialized[/green] ~/.config/chain-code/config.toml")


@main.group()
def config() -> None:
    """View or edit config."""


@config.command("show")
def config_show() -> None:
    cfg = load_config()
    for k, v in cfg.items():
        console.print(f"[cyan]{k}[/cyan] = {v}")


@config.command("set")
@click.argument("key")
@click.argument("value")
def config_set(key: str, value: str) -> None:
    cfg = load_config()
    if key in {"temperature"}:
        cfg[key] = float(value)
    elif key in {"max_context_files"}:
        cfg[key] = int(value)
    elif key in {"approve_shell"}:
        cfg[key] = value.lower() in {"1", "true", "yes", "on"}
    else:
        cfg[key] = value
    save_config(cfg)
    console.print(f"[green]Updated[/green] {key}")


@main.command()
@click.argument("prompt")
def ask(prompt: str) -> None:
    """One-shot prompt with local repo context."""
    cfg = load_config()
    full = _build_prompt(prompt, cfg)
    out = ask_model(cfg["provider"], cfg["model"], full, float(cfg["temperature"]))
    console.print(Panel(out, title="Chain Code"))


@main.command()
def chat() -> None:
    """Interactive chat session."""
    cfg = load_config()
    console.print("[bold]Chain Code chat[/bold] (type /exit to quit)")
    while True:
        user = console.input("[blue]you> [/blue]").strip()
        if user in {"/exit", "/quit"}:
            break
        full = _build_prompt(user, cfg)
        out = ask_model(cfg["provider"], cfg["model"], full, float(cfg["temperature"]))
        console.print(Panel(out, title=f"{cfg['provider']}:{cfg['model']}"))


@main.command()
@click.argument("cmd", nargs=-1)
def run(cmd: tuple[str, ...]) -> None:
    """Run shell command with confirmation."""
    cfg = load_config()
    joined = " ".join(cmd)
    if not joined:
        raise click.UsageError("Pass a command to run")
    if cfg.get("approve_shell", True):
        if not click.confirm(f"Execute: {joined}", default=False):
            console.print("Aborted")
            return
    res = subprocess.run(joined, shell=True, text=True, capture_output=True)
    if res.stdout:
        console.print(res.stdout)
    if res.stderr:
        console.print(f"[yellow]{res.stderr}[/yellow]")
    raise SystemExit(res.returncode)


@main.command()
def doctor() -> None:
    """Run health checks."""
    cfg = load_config()
    provider = cfg["provider"]
    console.print(f"Provider: [cyan]{provider}[/cyan]")
    if provider == "ollama":
        try:
            out = subprocess.check_output(["bash", "-lc", "command -v ollama"], text=True).strip()
            console.print(f"[green]ollama found:[/green] {out}")
        except subprocess.CalledProcessError:
            console.print("[red]ollama not found[/red]")
    elif provider == "openai":
        console.print("OPENAI_API_KEY set" if __import__("os").getenv("OPENAI_API_KEY") else "[red]OPENAI_API_KEY missing[/red]")
    elif provider == "anthropic":
        console.print("ANTHROPIC_API_KEY set" if __import__("os").getenv("ANTHROPIC_API_KEY") else "[red]ANTHROPIC_API_KEY missing[/red]")
