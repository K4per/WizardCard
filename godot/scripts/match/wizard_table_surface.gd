class_name WizardTableSurface
extends Control

signal card_selected(card: Dictionary)
signal inspected(card: Dictionary)
signal zone_selected(side: int, zone: int)
signal dropped(data: Dictionary, destination: int, target: String)
var board: WizardBattlefield
var _hovered: String = ""

func _gui_input(event: InputEvent) -> void:
	if board == null or not board.input_enabled:
		return
	if event is InputEventMouseMotion and not get_viewport().gui_is_dragging():
		var hit: Dictionary = board.pick((event as InputEventMouseMotion).position)
		var card: Dictionary = hit.get("card", {}) as Dictionary
		var id: String = str(card.get("id", ""))
		if id != _hovered:
			_hovered = id
			if not card.is_empty():
				inspected.emit(card)
	if event is InputEventMouseButton:
		var button: InputEventMouseButton = event as InputEventMouseButton
		if button.button_index == MOUSE_BUTTON_LEFT and not button.pressed and not get_viewport().gui_is_dragging():
			var hit: Dictionary = board.pick(button.position)
			if hit.has("card"):
				card_selected.emit(hit["card"] as Dictionary)
			elif hit.has("zone"):
				zone_selected.emit(int(hit["side"]), int(hit["zone"]))
			accept_event()

func _can_drop_data(at: Vector2, data: Variant) -> bool:
	if board == null or not board.input_enabled or not data is Dictionary:
		return false
	var payload: Dictionary = data as Dictionary
	if str(payload.get("context", "")) != "match" or str(payload.get("generation", "")) != str(board.view.get("generation", "")) or str(payload.get("revision", "")) != str(board.view.get("revision", "")):
		return false
	var hit: Dictionary = board.pick(at)
	return hit.has("zone") and int(hit.get("side", -1)) == 0 and int(hit["zone"]) in [2, 3, 4, 5, 6]

func _drop_data(at: Vector2, data: Variant) -> void:
	if not _can_drop_data(at, data):
		return
	var hit: Dictionary = board.pick(at)
	var card: Dictionary = hit.get("card", {}) as Dictionary
	dropped.emit(data as Dictionary, int(hit["zone"]), str(card.get("id", "0")))
