class_name ActionSelectionController
extends RefCounted

signal changed(options: ActionOptions)
signal completed(command: Dictionary, intent: Dictionary)
signal failed(message: String)
signal rings_changed(targets: Dictionary[String, PackedInt32Array])

var service: FrontendService
var active: bool = false
var intent: Dictionary = {}
var options: ActionOptions
var generation: String = ""
var revision: String = ""
var viewer: int = -1
var _awaiting: bool = false
var _revalidating: bool = false
var decision_id: String = "0"
var _ring_jobs: Array[Dictionary] = []
var _ring_targets: Dictionary[String, PackedInt32Array] = {}
var _ring_probe: Dictionary = {}
var _material_probe: Dictionary = {}
var _material_options: Array[Dictionary] = []

func begin(source: String = "0", operation: int = -1) -> void:
	cancel()
	active = true
	generation = str(service.match_snapshot.get("generation", ""))
	revision = str(service.match_snapshot.get("revision", ""))
	viewer = service.match_viewer
	decision_id = ActionOptions.context_id(service.match_snapshot)
	intent = {"source": source, "selections": [], "offset": 0, "limit": 64}
	if operation >= 0:
		intent["selections"].append({"field": KernelContract.ActionField.Operation, "value": str(operation)})
	refresh()

func cancel() -> void:
	active = false
	_awaiting = false
	_revalidating = false
	intent.clear()
	options = null
	_clear_probes()

func _clear_probes() -> void:
	_ring_jobs.clear()
	_ring_targets.clear()
	_ring_probe.clear()
	_material_probe.clear()
	_material_options.clear()

func current() -> bool:
	return active and viewer == service.match_viewer and generation == str(service.match_snapshot.get("generation", "")) and revision == str(service.match_snapshot.get("revision", ""))

func revalidate() -> void:
	if not active:
		return
	if viewer != service.match_viewer or generation != str(service.match_snapshot.get("generation", "")) or decision_id != ActionOptions.context_id(service.match_snapshot):
		cancel()
		failed.emit(GameText.label("ui.selection.stale_window"))
		return
	revision = str(service.match_snapshot.get("revision", ""))
	_clear_probes()
	_revalidating = true
	refresh()

func refresh() -> void:
	if not current():
		cancel()
		return
	var response: Dictionary = service.match_request(&"query_actions", [viewer, intent, generation, revision])
	if not bool(response.get("ok", false)):
		var message: String = GameText.label("ui.selection.stale_choice") if _revalidating else str(response.get("error", GameText.label("ui.selection.query_failed")))
		cancel()
		failed.emit(message)
		return
	var parsed: ActionOptions = ActionOptions.parse(response.get("data", {}))
	if parsed == null or not parsed.matches(service.match_snapshot, viewer):
		cancel()
		return
	_awaiting = parsed.pending
	if _awaiting:
		return
	options = parsed
	var revalidating: bool = _revalidating
	_revalidating = false
	if revalidating and options.options.is_empty() and options.complete.is_empty():
		cancel()
		failed.emit(GameText.label("ui.selection.stale_choice"))
		return
	if not options.complete.is_empty():
		if not material_ids().is_empty():
			changed.emit(options)
		elif revalidating:
			options.field = KernelContract.ActionField.Confirm
			options.options = [{"value": "@confirm", "label": GameText.label("ui.selection.confirm_valid")}]
			changed.emit(options)
		else:
			completed.emit(options.complete, intent.duplicate(true))
		return
	# A contextual card has already selected its source; the query still carries
	# the explicit source step so the authority validates the same path.
	if options.field == KernelContract.ActionField.Source and str(intent.get("source", "0")) != "0":
		choose(str(intent["source"]))
		return
	if options.field == KernelContract.ActionField.Host and _uses_ring():
		_ring_probe = intent.duplicate(true)
		_ring_probe["offset"] = options.options.size()
		_add_ring_jobs(options)
		_poll_rings()
		return
	if options.field == KernelContract.ActionField.Material and options.has_more:
		_material_probe = intent.duplicate(true)
		_material_probe["offset"] = options.options.size()
		_material_options.assign(options.options)
		_poll_material_pages()
		return
	changed.emit(options)

func poll() -> void:
	if _awaiting:
		refresh()
	elif current() and (not _ring_jobs.is_empty() or not _ring_probe.is_empty()):
		_poll_rings()
	elif current() and not _material_probe.is_empty():
		_poll_material_pages()

func _uses_ring() -> bool:
	var selections: Array = intent.get("selections", [])
	return not selections.is_empty() and int(selections[0]["value"]) in [KernelContract.CommandKind.StartAnalysis, KernelContract.CommandKind.SetAmbush, KernelContract.CommandKind.PreloadWord]

func _probe(request: Dictionary) -> ActionOptions:
	var response: Dictionary = service.match_request(&"query_actions", [viewer, request, generation, revision])
	if not bool(response.get("ok", false)):
		return null
	var result: ActionOptions = ActionOptions.parse(response.get("data", {}))
	return result if result != null and result.matches(service.match_snapshot, viewer) else null

func _add_ring_jobs(hosts: ActionOptions) -> void:
	for host: Dictionary in hosts.options:
		var request: Dictionary = intent.duplicate(true)
		request["offset"] = 0
		request["selections"].append({"field": KernelContract.ActionField.Host, "value": str(host["value"])})
		_ring_jobs.append(request)
	if not hosts.has_more:
		_ring_probe.clear()

func _poll_rings() -> void:
	while not _ring_probe.is_empty():
		var hosts: ActionOptions = _probe(_ring_probe)
		if hosts == null:
			cancel()
			failed.emit(GameText.label("ui.selection.stale_host"))
			return
		if hosts.pending:
			return
		_ring_probe["offset"] += hosts.options.size()
		_add_ring_jobs(hosts)
	while not _ring_jobs.is_empty():
		var request: Dictionary = _ring_jobs[0]
		var rings: ActionOptions = _probe(request)
		if rings != null and rings.pending:
			return
		if rings != null and rings.field == KernelContract.ActionField.Ring:
			var host: String = str(request["selections"].back()["value"])
			var indexes: PackedInt32Array = _ring_targets.get(host, PackedInt32Array())
			for ring: Dictionary in rings.options:
				indexes.append(int(ring["value"]))
			_ring_targets[host] = indexes
			if rings.has_more:
				request["offset"] += rings.options.size()
				continue
		_ring_jobs.pop_front()
	if _ring_targets.is_empty():
		changed.emit(options)
	else:
		rings_changed.emit(_ring_targets)

func choose_ring(host: String, ring: int) -> bool:
	if not current() or _awaiting or not _ring_targets.has(host) or not _ring_targets[host].has(ring):
		return false
	intent["selections"].append({"field": KernelContract.ActionField.Host, "value": host})
	intent["selections"].append({"field": KernelContract.ActionField.Ring, "value": str(ring)})
	intent["offset"] = 0
	_clear_probes()
	refresh()
	return true

func _poll_material_pages() -> void:
	while not _material_probe.is_empty():
		var page: ActionOptions = _probe(_material_probe)
		if page == null:
			cancel()
			failed.emit(GameText.label("ui.selection.stale_material"))
			return
		if page.pending:
			return
		_material_options.append_array(page.options)
		_material_probe["offset"] += page.options.size()
		if not page.has_more:
			_material_probe.clear()
	options.options.assign(_material_options)
	options.has_more = false
	_material_options.clear()
	changed.emit(options)

func material_ids() -> Array[String]:
	var ids: Array[String] = []
	for entry: Dictionary in intent.get("selections", []):
		if int(entry["field"]) == KernelContract.ActionField.Material:
			ids.append(str(entry["value"]))
	return ids

func toggle_material(id: String) -> void:
	if not current() or _awaiting or options == null or not _material_probe.is_empty():
		return
	var selections: Array = intent["selections"]
	for index: int in range(selections.size()):
		if int(selections[index]["field"]) == KernelContract.ActionField.Material and str(selections[index]["value"]) == id:
			selections.remove_at(index)
			intent["offset"] = 0
			refresh()
			return
	if options.field == KernelContract.ActionField.Material:
		choose(id)

func choose(value: String) -> void:
	if not current() or options == null:
		return
	if value == "@confirm" and not options.complete.is_empty():
		completed.emit(options.complete, intent.duplicate(true))
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
