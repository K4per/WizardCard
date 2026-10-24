class_name ResponseTiming
extends RefCounted

enum Mode { ALL, AUTO, IGNORE }

static func pass_action(snapshot: Dictionary, mode: int) -> Dictionary:
	if mode == Mode.ALL or int(snapshot.get("result", -1)) != -1 or bool(snapshot.get("paused", false)):
		return {}
	var raw: Variant = snapshot.get("decision")
	if not raw is Dictionary:
		return {}
	var decision: Dictionary = raw
	if int(decision.get("kind", -1)) != 0 or int(decision.get("player", -1)) != int(snapshot.get("viewer", -2)):
		return {}
	var pass_choice: Dictionary = {}
	for action: Dictionary in snapshot.get("actions", []):
		var command: Dictionary = action.get("command", {})
		var type: int = int(command.get("type", -1))
		if type == 13 and str(command.get("decision", "")) == str(decision.get("id", "")):
			pass_choice = action
		elif type != 11 and mode == Mode.AUTO:
			# Every selectable response, including response setting and flipping,
			# counts. Only native PassResponse can be submitted automatically.
			return {}
	return pass_choice
