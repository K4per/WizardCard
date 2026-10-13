class_name DeckCardGrid
extends Control

signal inspected(id: String)
signal adjusted(id: String, delta: int, section: String)

var service: FrontendService
var ids: Array[String] = []
var columns: int = 10
var rows: int = 4
var slot_count: int = 40
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
		var area: Rect2 = Rect2(Vector2(index % columns, index / columns) * cell + Vector2.ONE * 2.0, (cell - Vector2.ONE * 4.0).max(Vector2.ONE))
		var width: float = minf(area.size.x, area.size.y * 0.75)
		var extent: Vector2 = Vector2(width, width / 0.75)
		var frame: Rect2 = Rect2(area.get_center() - extent * 0.5, extent)
		if index >= ids.size():
			if origin == "built":
				draw_rect(frame, DeckTheme.BACKGROUND, true)
				draw_rect(frame, DeckTheme.BORDER, false, 1.0)
			continue
		var id: String = ids[index]
		var card: CardPresentation = service.card_by_id(id)
		var available: bool = true
		if origin == "pool":
			var draft: Dictionary = service.editing_draft
			var membership: String = "sideCards" if int(service.definitions.get(id, {}).get("type", -1)) == 3 else "cards"
			available = service.named_copies(id, membership) < 3 and DeckUi.total(draft.get(membership, {})) < (10 if membership == "sideCards" else 40)
		DeckCardPainter.paint(self, card, frame, selected_id == id)
		if origin == "pool":
			var count: int = int(service.editing_draft.get("cards", {}).get(id, 0)) + int(service.editing_draft.get("sideCards", {}).get(id, 0))
			var artwork: Rect2 = DeckCardPainter.artwork_rect(frame)
			var badge: Rect2 = Rect2(artwork.end.x - 28.0, artwork.position.y + 1.0, 28.0, 16.0)
			draw_rect(badge, DeckTheme.HOVER, true)
			draw_string(Typography.BODY, badge.position + Vector2(3, 12), "%d/3" % count, HORIZONTAL_ALIGNMENT_LEFT, -1, 10, DeckTheme.TEXT)
			if not available:
				draw_rect(frame, Color(0.04, 0.07, 0.10, 0.38))
		_hits.append({"id": id, "rect": frame})
	if _drop_active:
		draw_rect(Rect2(Vector2.ZERO, size).grow(-2), DeckTheme.ACCENT, false, 2.0)

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
	elif event.is_action_pressed(&"ui_left") or event.is_action_pressed(&"ui_right") or event.is_action_pressed(&"ui_up") or event.is_action_pressed(&"ui_down"):
		if ids.is_empty():
			return
		var step: int = -1 if event.is_action_pressed(&"ui_left") else 1
		if event.is_action_pressed(&"ui_up") or event.is_action_pressed(&"ui_down"):
			step = -columns if event.is_action_pressed(&"ui_up") else columns
		_keyboard_index = wrapi(_keyboard_index + step, 0, ids.size())
		inspected.emit(ids[_keyboard_index])
		accept_event()
	elif event.is_action_pressed(&"ui_accept") and not ids.is_empty():
		inspected.emit(ids[clampi(_keyboard_index, 0, ids.size() - 1)])
		accept_event()

func _get_drag_data(point: Vector2) -> Variant:
	var id: String = id_at(point)
	if id.is_empty():
		return null
	var preview: DeckCardPainter = DeckCardPainter.new()
	preview.custom_minimum_size = Vector2(120, 160)
	preview.configure(service.card_by_id(id))
	set_drag_preview(preview)
	return {"kind": "deck-card", "id": id, "origin": origin, "section": section}

func _can_drop_data(_point: Vector2, data: Variant) -> bool:
	var allowed: bool = data is Dictionary and data.get("kind", "") == "deck-card" and data.get("origin", "") != origin
	if allowed and origin == "built":
		var id: String = str(data.get("id", ""))
		var membership: String = "sideCards" if int(service.definitions.get(id, {}).get("type", -1)) == 3 else "cards"
		allowed = service.definitions.has(id) and membership == section and service.named_copies(id, membership) < 3 and DeckUi.total(service.editing_draft.get(membership, {})) < (10 if membership == "sideCards" else 40)
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
	adjusted.emit(str(payload["id"]), 1 if origin == "built" else -1, section if origin == "built" else str(payload["section"]))
	_clear_drop()

func _notification(what: int) -> void:
	if what == NOTIFICATION_DRAG_END:
		_clear_drop()

func _clear_drop() -> void:
	_drop_active = false
	queue_redraw()
