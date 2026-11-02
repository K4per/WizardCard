class_name QueryFixtureService
extends FrontendService

# Test fixture only: these concrete commands are supplied by each focused test.
# Production projections never carry full action lists.
var fixture_actions: Array[Dictionary] = []
var selected_actions: Array[String] = []
var confirmations: int = 0
var _selected_fixture: Dictionary = {}

func snapshot(_viewer: int = -1) -> Dictionary:
	return match_snapshot

func set_actions(actions: Array) -> void:
	fixture_actions.assign(actions)

func _path(action: Dictionary) -> Array[Dictionary]:
	var command: Dictionary = action.get("command", {})
	var kind: int = int(command.get("type", -1))
	var path: Array[Dictionary] = [{"field": KernelContract.ActionField.Operation, "value": str(kind)}]
	if kind == KernelContract.CommandKind.Choose:
		var choice: Variant = command.get("option", "0")
		path.append({"field": KernelContract.ActionField.Choice, "value": str(choice["kind"]) + ":" + str(choice["value"]) if choice is Dictionary else str(choice)})
		if int(command.get("ringIndex", -1)) >= 0:
			path.append({"field": KernelContract.ActionField.Ring, "value": str(command["ringIndex"])})
	elif kind == KernelContract.CommandKind.AdvancePhase:
		path.append({"field": KernelContract.ActionField.Choice, "value": "1" if bool(command.get("skipCast", false)) else "0"})
	elif command.has("card") or str(action.get("source", "0")) != "0":
		path.append({"field": KernelContract.ActionField.Source, "value": str(command.get("card", action.get("source", "0")))})
		for entry: Array in [["formation", KernelContract.ActionField.Host], ["host", KernelContract.ActionField.Host], ["replace", KernelContract.ActionField.Replacement]]:
			if command.has(entry[0]):
				path.append({"field": entry[1], "value": str(command[entry[0]])})
		for material: Variant in command.get("materials", []):
			path.append({"field": KernelContract.ActionField.Material, "value": str(material)})
		if command.has("ability"):
			path.append({"field": KernelContract.ActionField.Ability, "value": str(command["ability"])})
		if str(command.get("link", "0")) != "0":
			path.append({"field": KernelContract.ActionField.Link, "value": str(command["link"])})
		if not command.get("targets", []).is_empty():
			for target: Variant in command["targets"]:
				path.append({"field": KernelContract.ActionField.Target, "value": str(target)})
		elif str(command.get("target", "0")) != "0":
			path.append({"field": KernelContract.ActionField.Target, "value": str(command["target"])})
		if str(command.get("discard", "0")) != "0":
			path.append({"field": KernelContract.ActionField.Discard, "value": str(command["discard"])})
	return path

func _query(intent: Dictionary) -> Dictionary:
	var decision: Dictionary = match_snapshot.get("decision", {}) if match_snapshot.get("decision") is Dictionary else {}
	var data: Dictionary = {"schema": KernelContract.SCHEMA, "generation": str(match_snapshot.get("generation", "0")), "revision": str(match_snapshot.get("revision", "0")), "viewer": match_viewer, "decisionId": str(decision.get("id", "opening" if bool(match_snapshot.get("opening", false)) else "0")), "field": KernelContract.ActionField.Operation, "options": [], "hasMore": false, "pending": false, "complete": null}
	var selections: Array = intent.get("selections", [])
	var seen: Dictionary = {}
	for action: Dictionary in fixture_actions:
		var scope: String = str(intent.get("source", "0"))
		if scope != "0" and scope != str(action.get("source", action.get("command", {}).get("card", "0"))):
			continue
		var path: Array[Dictionary] = _path(action)
		if selections.size() > path.size():
			continue
		var matches: bool = true
		for index: int in range(selections.size()):
			if int(selections[index]["field"]) != int(path[index]["field"]) or str(selections[index]["value"]) != str(path[index]["value"]):
				matches = false
		if not matches:
			continue
		if selections.size() == path.size():
			data["complete"] = action.get("command", {})
			data["field"] = KernelContract.ActionField.Confirm
			_selected_fixture = action
			return data
		var next: Dictionary = path[selections.size()]
		var value: String = str(next["value"])
		var field: int = int(next["field"])
		if seen.has(value):
			continue
		seen[value] = true
		data["field"] = field
		var card_id: String = value if field in [KernelContract.ActionField.Source, KernelContract.ActionField.Host, KernelContract.ActionField.Replacement, KernelContract.ActionField.Material, KernelContract.ActionField.Target, KernelContract.ActionField.Discard] else "0"
		var player: int = int(action.get("queryPlayer", -1)) if field == KernelContract.ActionField.Target else -1
		if field == KernelContract.ActionField.Choice:
			var choice_value: PackedStringArray = value.split(":")
			if choice_value.size() == 2 and int(choice_value[0]) == KernelContract.ChoiceKind.Card:
				card_id = choice_value[1]
			else:
				card_id = str(action.get("queryCard", "0"))
		if player >= 0:
			card_id = "0"
		var label: String = str(action.get("label", action.get("id", "选项")))
		if field == KernelContract.ActionField.Operation:
			label = MatchProjection.action_label(match_snapshot, action)
		data["options"].append({"value": value, "label": label, "card": card_id, "player": player})
	var start: int = int(intent.get("offset", 0))
	var end: int = mini(data["options"].size(), start + int(intent.get("limit", 64)))
	data["hasMore"] = end < data["options"].size()
	data["options"] = data["options"].slice(start, end)
	return data

func match_request(method: StringName, arguments: Array = []) -> Dictionary:
	if method == &"query_actions":
		return {"ok": true, "data": _query(arguments[1])}
	if method == &"select_intent":
		var data: Dictionary = _query(arguments[1])
		if not data.get("complete") is Dictionary:
			return {"ok": false, "error": "incomplete"}
		selected_actions.append(str(_selected_fixture.get("id", "")))
		return {"ok": true, "data": {"command": data["complete"]}}
	if method == &"confirm_action":
		confirmations += 1
		return {"ok": true, "data": {"pending": true}}
	if method == &"submit_command":
		for action: Dictionary in fixture_actions:
			if action.get("command", {}) == arguments[1]:
				selected_actions.append(str(action.get("id", "")))
				break
		confirmations += 1
		return {"ok": true, "data": {"pending": true}}
	return {"ok": true, "data": {}}
