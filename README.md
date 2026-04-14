# Chain Code

Chain Code is an Arch Linux-friendly coding assistant CLI inspired by terminal-first AI coding tools.

## What this project is

- A **local-first**, terminal coding assistant scaffold.
- Built for **Arch Linux and Arch-based distros** (Manjaro, EndeavourOS, Garuda, etc.).
- Includes:
  - interactive chat
  - autonomous agent mode that can execute shell commands
  - one-shot prompts
  - shell command execution helper
  - project-aware context loading
  - provider abstraction (`ollama`, `openai`, `anthropic`)

## What this project is not

This is **not** an official product and does not include proprietary internals from any commercial code assistant. It is an original implementation with a similar workflow.

## Install (Arch)

### A. Run from source

```bash
sudo pacman -S --needed python python-pip python-rich python-click python-requests python-tomli-w
python -m venv .venv
source .venv/bin/activate
pip install -e .
chain-code --help
```

### B. Build package

```bash
makepkg -si
```

## Quick start

```bash
chain-code init
chain-code config set provider ollama
chain-code config set model codellama:13b
chain-code ask "Summarize this repository"
chain-code agent "inspect this repo and list TODOs"
chain-code chat
```

## Provider setup

### Ollama (recommended for local Arch)

```bash
sudo pacman -S --needed ollama
sudo systemctl enable --now ollama
ollama pull codellama:13b
```

Then configure:

```bash
chain-code config set provider ollama
chain-code config set model codellama:13b
```

### OpenAI / Anthropic

Set environment variables:

- `OPENAI_API_KEY`
- `ANTHROPIC_API_KEY`

Then configure provider and model with `chain-code config set`.

## Commands

- `chain-code init` – create default config.
- `chain-code config show` – print current config.
- `chain-code config set <key> <value>` – update config.
- `chain-code ask "..."` – one-shot prompt with repository context.
- `chain-code agent "..."` – autonomous mode: model plans and executes shell commands (no confirmation by default).
- `chain-code chat` – REPL chat session.
- `chain-code run "..."` – run shell command with confirmation.
- `chain-code doctor` – verify runtime deps and provider connectivity.

Use `chain-code agent --confirm "..."` if you want per-command prompts.

## Security notes

- `run` requires explicit confirmation before executing shell commands.
- No telemetry.
- Configuration lives at `~/.config/chain-code/config.toml`.

## License

MIT
