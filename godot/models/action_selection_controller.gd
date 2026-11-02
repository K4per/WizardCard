class_name ActionSelectionController
extends RefCounted

signal changed(options: ActionOptions)
signal completed(command: Dictionary, intent: Dictionary)
signal failed(message: String)

var service: FrontendService
var active: bool = false
var intent: Dictionary = {}
var options: ActionOptions
var generation: String = ""
var revision: String = ""
var viewer: int = -1
var _awaiting: bool = false

func begin(source: String = "0", operation: int = -1) -> void:
	cancel()
	active = true
	generation = str(service.match_snapshot.get("generation", ""))
	revision = str(service.match_snapshot.get("revision", ""))
	viewer = service.match_viewer
	intent = {"source": source, "selections": [], "offset": 0, "limit": 64}
	if operation >= 0:
		intent["selections"].append({"field": KernelContract.ActionField.Operation, "value": str(operation)})
	refresh()

func cancel() -> void:
	active = false
	_awaiting = false
	intent.clear()
	options = null

func current() -> bool:
	return active and viewer == service.match_viewer and generation == str(service.match_snapshot.get("generation", "")) and revision == str(service.match_snapshot.get("revision", ""))

func refresh() -> void:
	if not current():
		cancel()
		return
	var response: Dictionary = service.match_request(&"query_actions", [viewer, intent, generation, revision])
	if not bool(response.get("ok", false)):
		failed.emit(str(response.get("error", "动作查询失败")))
		cancel()
		return
	var parsed: ActionOptions = ActionOptions.parse(response.get("data", {}))
	if parsed == null or not parsed.matches(service.match_snapshot, viewer):
		cancel()
		return
	_awaiting = parsed.pending
	if _awaiting:
		return
	options = parsed
	if not options.complete.is_empty():
		completed.emit(options.complete, intent.duplicate(true))
		return
	# A contextual card has already selected its source; the query still carries
	# the explicit source step so the authority validates the same path.
	if options.field == KernelContract.ActionField.Source and str(intent.get("source", "0")) != "0":
		choose(str(intent["source"]))
		return
	changed.emit(options)

func poll() -> void:
	if _awaiting:
		refresh()

func choose(value: String) -> void:
	if not current() or options == null:
		return
	if value == "@next":
		intent["offset"] = int(intent.get("offset", 0)) + options.options.size()
		refresh()
		return
	if value == "@previous":
		intent["offset"] = maxi(0, int(intent.get("offset", 0)) - int(intent.get("limit", 64)))
		refresh()
		return
	for option: Dictionary in options.options:
		if str(option.get("value", "")) == value:
			intent["selections"].append({"field": options.field, "value": value})
			intent["offset"] = 0
			refresh()
			return

func back() -> void:
	if not current():
		return
	if intent["selections"].is_empty():
		cancel()
		return
	intent["selections"].pop_back()
	intent["offset"] = 0
	refresh()
