class_name AutomaticActionController
extends RefCounted

static func request(snapshot: Dictionary, response_mode: int) -> Dictionary:
	if int(snapshot.get("result", -1)) != -1 or bool(snapshot.get("paused", false)) or bool(snapshot.get("technicalPaused", false)):
		return {}
	if bool(snapshot.get("aiTurn", false)):
		return {"method": &"step_ai", "arguments": [str(snapshot.get("generation", "")), str(snapshot.get("revision", ""))]}
	var command: Dictionary = ResponseTiming.pass_action(snapshot, response_mode)
	if command.is_empty() and snapshot.get("automaticCommand") is Dictionary:
		command = snapshot["automaticCommand"]
	if command.is_empty():
		return {}
	return {"method": &"submit_command", "arguments": [int(snapshot.get("viewer", -1)), command, str(snapshot.get("generation", "")), str(snapshot.get("revision", ""))]}
