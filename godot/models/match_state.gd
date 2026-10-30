class_name MatchState
extends RefCounted

var generation: String = ""
var revision: String = ""
var viewer: int = -1
var decision_id: String = "0"
var technical_paused: bool = false
var data: Dictionary = {}

static func parse(value: Dictionary) -> MatchState:
	if int(value.get("schema", -1)) != KernelContract.SCHEMA:
		return null
	if not value.get("generation") is String or not value.get("revision") is String:
		return null
	if not value.get("cards") is Array or not value.get("players") is Array or value["players"].size() != 2:
		return null
	var result: MatchState = MatchState.new()
	result.generation = value["generation"]
	result.revision = value["revision"]
	result.viewer = int(value.get("viewer", -1))
	if result.viewer < 0 or result.viewer > 1:
		return null
	var decision: Variant = value.get("decision")
	result.decision_id = str(decision.get("id", "0")) if decision is Dictionary else ("opening" if bool(value.get("opening", false)) else "0")
	result.technical_paused = bool(value.get("technicalPaused", false))
	result.data = value
	return result
