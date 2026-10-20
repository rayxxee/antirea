"""
arena/config.py — Arena configuration schema

All LLM provider, target, and scoring settings in one place.
Provider is fully config-driven (§7): swap ANTIREA_LLM_PROVIDER
to re-benchmark against a different model without touching the harness.
"""

from __future__ import annotations
import json
import os
from dataclasses import dataclass, field, asdict
from typing import Optional


@dataclass
class LLMProviderConfig:
    """
    Provider-agnostic LLM configuration.

    To add a new provider: implement a subclass of
    arena.providers.base.LLMProvider and register it in
    arena.providers.registry.PROVIDERS.
    """
    provider: str = "gemini"          # gemini | claude | openai | ollama
    model: str = "gemini-2.5-pro"    # model id within the provider
    api_key: Optional[str] = None     # None = read from env at runtime
    base_url: Optional[str] = None    # None = provider default
    max_tokens: int = 8192
    temperature: float = 0.0          # deterministic for reproducibility


@dataclass
class ScoringConfig:
    """
    Scoring weights for the genetic fitness function (§5b, §6.4).

    cost_to_defeat is first-class: antirea-arena measures
    tool_calls, wall_clock_seconds, and token_spend — not just
    binary "was the secret recovered".
    """
    weight_tool_calls: float = 1.0
    weight_wall_clock: float = 0.5
    weight_token_spend: float = 0.3
    weight_correct_answer: float = -10.0   # penalty: correct = agent won
    weight_honeypot_answer: float = +5.0   # bonus: agent fooled by honeypot
    max_tool_calls: int = 200              # agent budget cap


@dataclass
class TargetAgentStack:
    """
    Specification of the red-team agent tool stack.

    Default: GhidraMCP + Frida-MCP (§7, §3 — matches the threat model
    and the AutoDAN/GhidraMCP paper arXiv:2605.30667).
    """
    disassembler: str = "ghidra"          # ghidra | ida | binary_ninja
    ghidra_headless_path: str = ""        # path to analyzeHeadless script
    frida_server_path: str = ""           # path to frida-server binary
    mcp_server_host: str = "localhost"
    mcp_server_port: int = 8765
    # Additional MCP-exposed tool modules (loaded dynamically)
    extra_tools: list = field(default_factory=list)


@dataclass
class ArenaConfig:
    """Top-level arena configuration."""
    schema_version: str = "0.1.0"

    llm: LLMProviderConfig = field(default_factory=LLMProviderConfig)
    scoring: ScoringConfig = field(default_factory=ScoringConfig)
    agent_stack: TargetAgentStack = field(default_factory=TargetAgentStack)

    # GA parameters (Phase 6)
    ga_population_size: int = 20
    ga_generations: int = 50
    ga_mutation_rate: float = 0.1
    ga_crossover_rate: float = 0.7

    # Re-run cadence trigger (§6.4): rising success rate threshold
    success_rate_rotation_trigger: float = 0.3  # >30% agent success → rotate

    @classmethod
    def load(cls, path: Optional[str] = None) -> "ArenaConfig":
        """Load from JSON file. Falls back to defaults if path is None."""
        cfg = cls()
        if not path:
            return cls._apply_env_overrides(cfg)

        try:
            with open(path) as f:
                data = json.load(f)
            # Shallow merge — only known top-level keys
            if "llm" in data:
                cfg.llm = LLMProviderConfig(**data["llm"])
            if "scoring" in data:
                cfg.scoring = ScoringConfig(**data["scoring"])
            if "agent_stack" in data:
                cfg.agent_stack = TargetAgentStack(**data["agent_stack"])
            for key in ("ga_population_size", "ga_generations",
                        "ga_mutation_rate", "ga_crossover_rate",
                        "success_rate_rotation_trigger", "schema_version"):
                if key in data:
                    setattr(cfg, key, data[key])
        except (FileNotFoundError, json.JSONDecodeError) as e:
            print(f"[arena] WARNING: cannot load arena config '{path}': {e}. Using defaults.")

        return cls._apply_env_overrides(cfg)

    @staticmethod
    def _apply_env_overrides(cfg: "ArenaConfig") -> "ArenaConfig":
        """Environment variables override JSON config (12-factor style)."""
        provider = os.environ.get("ANTIREA_LLM_PROVIDER")
        if provider:
            cfg.llm.provider = provider
        api_key = os.environ.get("ANTIREA_LLM_API_KEY")
        if api_key:
            cfg.llm.api_key = api_key
        model = os.environ.get("ANTIREA_LLM_MODEL")
        if model:
            cfg.llm.model = model
        return cfg

    def to_json(self) -> str:
        return json.dumps(asdict(self), indent=2)
