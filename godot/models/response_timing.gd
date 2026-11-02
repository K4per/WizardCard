class_name ResponseTiming
extends RefCounted

enum Mode { ALL, AUTO, IGNORE }

static func pass_action(snapshot: Dictionary, mode: int) -> Dictionary:
	if mode == Mode.ALL or int(snapshot.get("result", -1)) != -1 or bool(snapshot.get("paused", false)) or bool(snapshot.get("technicalPaused", false)):
		return {}
	var decision: Variant = snapshot.get("decision")
	if not decision is Dictionary or int(decision.get("kind", -1)) != KernelContract.DecisionKind.Response or int(decision.get("player", -1)) != int(snapshot.get("viewer", -2)):
		return {}
	var summary: Dictionary = snapshot.get("actionSummary", {})
	if mode == Mode.AUTO and bool(summary.get("hasNonPhaseAction", false)):
		return {}
	var command: Variant = summary.get("safeAdvance")
	if command is Dictionary and int(command.get("type", -1)) == KernelContract.CommandKind.PassResponse and str(command.get("decision", "")) == str(decision.get("id", "")):
		return command
	return {}
