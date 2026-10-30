"""Regenerate the scripting boundary enums from authoritative C++ declarations."""
from pathlib import Path
import re
root = Path(__file__).resolve().parents[1]
def enum(name, text):
    body = re.search(r"enum class " + name + r"(?:\s*:\s*\w+(?:::\w+)*)?\s*\{([^}]+)\}", text, re.S)[1]
    body = re.sub(r"//[^\n]*", "", body)
    return "enum " + name + " { " + ", ".join(value.strip() for value in body.split(",") if value.strip()) + " }\n"
commands = (root / "include/wizard/core/commands.hpp").read_text(encoding="utf8")
query = (root / "include/wizard/core/action_query.hpp").read_text(encoding="utf8")
output = "class_name KernelContract\nextends RefCounted\n\n# Generated from core contracts. Run tools/generate_godot_contract.py after changes.\nconst SCHEMA: int = 2\n"
output += enum("CommandKind", commands) + enum("ChoiceKind", commands) + enum("ChoiceMode", commands) + enum("DecisionKind", commands) + enum("ActionField", query)
(root / "godot/models/kernel_contract.gd").write_text(output, encoding="utf8")
