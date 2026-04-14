from __future__ import annotations

import os
import requests


def ask_model(provider: str, model: str, prompt: str, temperature: float = 0.2) -> str:
    provider = provider.lower()
    if provider == "ollama":
        return _ollama(model, prompt, temperature)
    if provider == "openai":
        return _openai(model, prompt, temperature)
    if provider == "anthropic":
        return _anthropic(model, prompt, temperature)
    raise ValueError(f"Unsupported provider: {provider}")


def _ollama(model: str, prompt: str, temperature: float) -> str:
    resp = requests.post(
        "http://localhost:11434/api/generate",
        json={"model": model, "prompt": prompt, "stream": False, "options": {"temperature": temperature}},
        timeout=120,
    )
    resp.raise_for_status()
    return resp.json().get("response", "")


def _openai(model: str, prompt: str, temperature: float) -> str:
    api_key = os.getenv("OPENAI_API_KEY")
    if not api_key:
        raise RuntimeError("OPENAI_API_KEY is not set")
    resp = requests.post(
        "https://api.openai.com/v1/responses",
        headers={"Authorization": f"Bearer {api_key}", "Content-Type": "application/json"},
        json={"model": model, "input": prompt, "temperature": temperature},
        timeout=120,
    )
    resp.raise_for_status()
    body = resp.json()
    # best-effort extraction for responses API text output
    try:
        return body["output"][0]["content"][0]["text"]
    except Exception:
        return str(body)


def _anthropic(model: str, prompt: str, temperature: float) -> str:
    api_key = os.getenv("ANTHROPIC_API_KEY")
    if not api_key:
        raise RuntimeError("ANTHROPIC_API_KEY is not set")
    resp = requests.post(
        "https://api.anthropic.com/v1/messages",
        headers={
            "x-api-key": api_key,
            "anthropic-version": "2023-06-01",
            "content-type": "application/json",
        },
        json={"model": model, "max_tokens": 2048, "temperature": temperature, "messages": [{"role": "user", "content": prompt}]},
        timeout=120,
    )
    resp.raise_for_status()
    body = resp.json()
    try:
        return body["content"][0]["text"]
    except Exception:
        return str(body)
