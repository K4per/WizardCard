class_name MatchEventLog
extends RefCounted

const CAPACITY: int = 512
var generation: String = ""
var viewer: int = -1
var epoch: String = "0"
var cursor: String = "0"
var entries: Array[Dictionary] = []

func reset(next_generation: String, next_viewer: int) -> void:
	generation = next_generation
	viewer = next_viewer
	epoch = "0"
	cursor = "0"
	entries.clear()

func ingest(batch: Dictionary) -> Dictionary:
	if int(batch.get("schema", -1)) != KernelContract.SCHEMA or str(batch.get("generation", "")) != generation or int(batch.get("viewer", -1)) != viewer:
		return {}
	var reset_log: bool = bool(batch.get("reset", false))
	if not reset_log and (str(batch.get("epoch", "")) != epoch or str(batch.get("base", "")) != cursor):
		return {}
	if reset_log:
		entries.clear()
	var added: Array[Dictionary] = []
	for event: Dictionary in batch.get("events", []):
		entries.append(event)
		added.append(event)
	var truncated: bool = entries.size() > CAPACITY
	while entries.size() > CAPACITY:
		entries.pop_front()
	epoch = str(batch.get("epoch", "0"))
	cursor = str(batch.get("next", "0"))
	return {"reset": reset_log, "events": added, "truncated": truncated}
