class_name DecisionPresentation
extends Resource

var generation: String = ""
var revision: String = ""
var decision_id: String = ""
var title: String = ""
var description: String = ""
var choices: Array[DecisionChoice] = []
var minimum: int = 1
var maximum: int = 1
var may_cancel: bool = true
var response_mode: bool = false
var browse_mode: bool = false
var docked: bool = false
var pass_id: String = ""

func has_exact_context() -> bool:
	# Never parse uint64 IDs as a GDScript int or float.  The opening
	# formation choice is a local UI decision, so it uses a stable sentinel
	# instead of a native decision ID; generation and revision remain strict.
	return _is_exact_numeric_id(generation) \
		and _is_exact_numeric_id(revision) \
		and (decision_id == "opening" or _is_exact_numeric_id(decision_id))

func _is_exact_numeric_id(value: String) -> bool:
	if value.is_empty() or value.length() > 20:
		return false
	for index: int in range(value.length()):
		var code: int = value.unicode_at(index)
		if code < 48 or code > 57:
			return false
	return true
