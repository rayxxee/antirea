"""
antirea-arena — AntiREA red/blue evaluation harness

Phase 0: Package skeleton + config loader.
Phase 6: Genetic algorithm + live agent attachment.

Usage (Phase 6+):
  python -m arena.harness.run --target=target.elf --profile=paranoid.json

Architecture:
  arena/
    harness/    - target process management, agent attachment, scoring
    ga/         - genetic algorithm: evolves obfuscation params + Pass 4b payloads
    providers/  - LLM provider adapters (Gemini, Claude, OpenAI, Ollama, ...)
    config.py   - arena configuration schema

Design goal (§5b):
  Score function = cost-to-defeat:
    - tool_calls: number of agent tool calls to claim an answer
    - wall_clock_seconds: elapsed time
    - token_spend: provider-reported tokens (provider-agnostic field)
    - correct: bool (was the claimed answer the real secret or the honeypot?)
  Rising agent_success_rate trend over time → trigger for Pass 4/5 rotation.

LLM provider is config-driven (not hardcoded) per §7 answer:
  set ANTIREA_LLM_PROVIDER=gemini|claude|openai|ollama
  set ANTIREA_LLM_API_KEY=...
  or specify in arena_config.json.
"""

__version__ = "0.1.0"
__all__ = []
