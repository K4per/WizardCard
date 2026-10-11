class_name DeckCardGrid
extends Control

signal inspected(id: String)
signal adjusted(id: String, delta: int, section: String)

var service: FrontendService
var ids: Array[String] = []
var columns: int = 6
var rows: int = 5
var slot_count: int = 30
var origin: String = "built"
var section: String = "cards"
var selected_id: String = ""
var _hits: Array[Dictionary] = []
var _drop_active: bool = false
var _keyboard_index: int = 0

func _ready() -> void:
	focus_mode = Control.FOCUS_ALL
	clip_contents = true
	texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	resized.connect(queue_redraw)
	mouse_exited.connect(_clear_drop)

func _draw() -> void:
	_hits.clear()
	if service == null:
		return
	var cell: Vector2 = size / Vector2(columns, rows)
	for index: int in range(mini(slot_count, columns * rows)):
		# Keep a narrow breathing line between cards while giving each card and
		# empty-slot base the largest stable rectangle in its grid cell.
		var area: Rect2 = Rect2(Vector2(index % columns, index / columns) * cell + Vector2(2, 2), cell - Vector2(4, 4))
		var frame: Rect2 = CardPainter.fitted_rect(area)
		if index >= ids.size():
			if origin == "built":
				draw_texture_rect(ArtBank.texture("c2d.v2.deck.empty-slot"), frame, false)
			continue
		var id: String = ids[index]
		var card: CardPresentation = service.card_by_id(id)
		var available: bool = true
		if origin == "pool":
			var draft: Dictionary = service.editing_draft
			var membership: String = "sideCards" if int(service.definitions.get(id, {}).get("type", -1)) == 3 else "cards"
			available = service.named_copies(id) < 3 and DeckUi.total(draft.get(membership, {})) < (10 if membership == "sideCards" else 40)
		CardPainter.paint(self, card, frame, "builder", selected_id == id)
		if origin == "pool":
			var count: int = int(service.editing_draft.get("cards", {}).get(id, 0)) + int(service.editing_draft.get("sideCards", {}).get(id, 0))
			var badge: Rect2 = Rect2(frame.end - Vector2(64, 26), Vector2(64, 26))
			draw_texture_rect(ArtBank.texture("c2d.v2.deck.quantity-badge"), badge, false)
			draw_string(Typography.HEADING, badge.position + Vector2(19, 18), "%d/3" % count, HORIZONTAL_ALIGNMENT_LEFT, -1, 12, CardPainter.PAPER)
			if not available:
				draw_rect(frame, Color(0.02, 0.05, 0.09, 0.4))
		_hits.append({"id": id, "rect": frame})
	if _drop_active:
		draw_style_box(ArtBank.box("c2d.v2.deck.drop-highlight", 0), Rect2(Vector2.ZERO, size))

func id_at(point: Vector2) -> String:
	for hit: Dictionary in _hits:
		var rect: Rect2 = hit["rect"]
		if rect.has_point(point):
			return str(hit["id"])
	return ""

func _gui_input(event: InputEvent) -> void:
	if event is InputEventMouseMotion:
		var id: String = id_at((event as InputEventMouseMotion).position)
		if not id.is_empty():
			tooltip_text = service.card_by_id(id).title
			inspected.emit(id)
	elif event is InputEventMouseButton:
		var mouse: InputEventMouseButton = event
		var id: String = id_at(mouse.position)
		if not mouse.pressed or id.is_empty():
			return
		grab_focus()
		inspected.emit(id)
		if mouse.button_index == MOUSE_BUTTON_RIGHT:
			adjusted.emit(id, 1 if origin == "pool" else -1, section)
		accept_event()
	elif event.is_action_pressed(&"ui_left") or event.is_action_pressed(&"ui_right"):
		if ids.is_empty():
			return
		_keyboard_index = wrapi(_keyboard_index + (-1 if event.is_action_pressed(&"ui_left") else 1), 0, ids.size())
		inspected.emit(ids[_keyboard_index])
		accept_event()
	elif event.is_action_pressed(&"ui_accept") and not ids.is_empty():
		inspected.emit(ids[clampi(_keyboard_index, 0, ids.size() - 1)])
		accept_event()

func _get_drag_data(point: Vector2) -> Variant:
	var id: String = id_at(point)
	if id.is_empty():
		return null
	var preview: CardFace = CardFace.new()
	preview.custom_minimum_size = Vector2(120, 160)
	preview.configure(service.card_by_id(id))
	set_drag_preview(preview)
	return {"kind": "deck-card", "id": id, "origin": origin, "section": section}

func _can_drop_data(_point: Vector2, data: Variant) -> bool:
	var allowed: bool = data is Dictionary and data.get("kind", "") == "deck-card" and data.get("origin", "") != origin
	if allowed and origin == "built":
		var id: String = str(data.get("id", ""))
		var membership: String = "sideCards" if int(service.definitions.get(id, {}).get("type", -1)) == 3 else "cards"
		allowed = service.definitions.has(id) and service.named_copies(id) < 3 and DeckUi.total(service.editing_draft.get(membership, {})) < (10 if membership == "sideCards" else 40)
	if allowed != _drop_active:
		_drop_active = allowed
		queue_redraw()
	return allowed

func _input(event: InputEvent) -> void:
	if not get_viewport().gui_is_dragging():
		return
	var right_click: bool = event is InputEventMouseButton and (event as InputEventMouseButton).button_index == MOUSE_BUTTON_RIGHT and event.is_pressed()
	if event.is_action_pressed(&"ui_cancel") or right_click:
		get_viewport().gui_cancel_drag()
		get_viewport().set_input_as_handled()

func _drop_data(_point: Vector2, data: Variant) -> void:
	var payload: Dictionary = data
	adjusted.emit(str(payload["id"]), 1 if origin == "built" else -1, str(payload["section"]))
	_clear_drop()

func _notification(what: int) -> void:
	if what == NOTIFICATION_DRAG_END:
		_clear_drop()

func _clear_drop() -> void:
	_drop_active = false
	queue_redraw()
