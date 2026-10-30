class_name ActionOptions
extends RefCounted

var generation: String = ""
var revision: String = ""
var viewer: int = -1
var decision_id: String = "0"
var field: int = KernelContract.ActionField.Operation
var options: Array[Dictionary] = []
var complete: Dictionary = {}
var has_more: bool = false
var pending: bool = false

static func parse(value: Dictionary) -> ActionOptions:
	if int(value.get("schema", -1)) != KernelContract.SCHEMA or not value.get("options") is Array:
		return null
	var result: ActionOptions = ActionOptions.new()
	result.generation = str(value.get("generation", ""))
	result.revision = str(value.get("revision", ""))
	result.viewer = int(value.get("viewer", -1))
	result.decision_id = str(value.get("decisionId", "0"))
	result.field = int(value.get("field", -1))
	if result.field < KernelContract.ActionField.Operation or result.field > KernelContract.ActionField.Confirm:
		return null
	for row: Variant in value["options"]:
		if not row is Dictionary or not row.get("value") is String:
			return null
		result.options.append(row)
	if value.get("complete") is Dictionary:
		result.complete = value["complete"]
	result.has_more = bool(value.get("hasMore", false))
	result.pending = bool(value.get("pending", false))
	return result

func matches(snapshot: Dictionary, expected_viewer: int) -> bool:
	var decision: Variant = snapshot.get("decision")
	var current_id: String = str(decision.get("id", "0")) if decision is Dictionary else ("opening" if bool(snapshot.get("opening", false)) else "0")
	return viewer == expected_viewer and generation == str(snapshot.get("generation", "")) and revision == str(snapshot.get("revision", "")) and decision_id == current_id
