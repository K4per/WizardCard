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

func has_exact_context() -> bool:
	# Never parse uint64 IDs as a GDScript int or float.
	for id: String in [generation, revision, decision_id]:
		if id.is_empty() or id.length() > 20:
			return false
		for index: int in range(id.length()):
			var code: int = id.unicode_at(index)
			if code < 48 or code > 57:
				return false
	return true
