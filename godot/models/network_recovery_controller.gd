class_name NetworkRecoveryController
extends RefCounted

var waiting: bool = false
var blocked: bool = false
var notice: String = ""
var generation: String = ""
var revision: String = ""

func submitted() -> void:
	waiting = true

func update(state: Dictionary) -> bool:
	blocked = bool(state.get("blocked", false))
	notice = str(state.get("notice", ""))
	var next_generation: String = str(state.get("generation", ""))
	var next_revision: String = str(state.get("revision", ""))
	var changed: bool = generation != next_generation or revision != next_revision
	if changed or blocked or (not notice.is_empty() and notice != "操作已发送，等待房主确认。"):
		waiting = false
	generation = next_generation
	revision = next_revision
	return changed
