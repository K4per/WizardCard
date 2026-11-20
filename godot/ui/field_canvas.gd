@tool
class_name FieldCanvas
extends Control

signal card_selected(card: CardPresentation)
signal card_hovered(card: CardPresentation)
signal selection_cleared()
signal hand_toggle_requested()
signal placement_requested(card_id: String, host_id: String, ring_index: int)
signal ring_selected(host_id: String, ring_index: int)
signal formation_slot_selected(slot_index: int)
signal zone_clicked(kind: String, owner: int)
signal layout_changed()

@export_group("Presentation")
@export var greybox: bool = false
## Enable to author field bounds directly with the child Control anchors.
## Otherwise FieldLayout supplies their bounds in both editor and runtime.
@export var scene_region_overrides: bool = false:
	set(value):
		scene_region_overrides = value
		queue_redraw()
var background_theme: String = "arcane"
@export var layout: FieldLayout = preload("res://ui/default_field_layout.tres"):
	set(value):
		if layout != null and layout.changed.is_connected(_layout_resource_changed):
			layout.changed.disconnect(_layout_resource_changed)
		layout = value if value != null else FieldLayout.new()
		if not layout.changed.is_connected(_layout_resource_changed):
			layout.changed.connect(_layout_resource_changed)
		queue_redraw()
@export_group("Editor Preview")
@export var editor_preview: bool = true:
	set(value):
		editor_preview = value
		queue_redraw()
@export_range(0, 12, 1) var preview_ring_count: int = 6:
	set(value):
		preview_ring_count = value
		queue_redraw()
@export_range(0, 20, 1) var preview_hand_count: int = 5:
	set(value):
		preview_hand_count = value
		queue_redraw()


var field: FieldPresentation = FieldPresentation.new()
var privacy: bool = false
var hand_collapsed: bool = false
## Optional presentation clearance for resource panels in the bottom-left and
## top-right corners. It constrains only the hand fan, never board legality.
var hand_side_clearance: float = 0.0:
	set(value):
		if is_equal_approx(hand_side_clearance, value):
			return
		hand_side_clearance = maxf(0.0, value)
		queue_redraw()
var _card_hits: Array[Dictionary] = []
var _opponent_hand_hits: Array[Dictionary] = []
var _ring_hits: Array[Dictionary] = []
var _formation_slot_hits: Array[Dictionary] = []
var _zone_hits: Array[Dictionary] = []
var _hover_id: String = ""
var _keyboard_id: String = ""
var _keyboard_ring: int = -1
var _expand_hits: Array[Dictionary] = []
var _host_menu: PopupMenu
var _casting_start: Array[int] = [0, 0]
var _casting_page_hits: Array[Dictionary] = []
var _press_position: Vector2
var _dragged: CardPresentation
var _drag_active: bool = false
var _pointer: Vector2
var _own_region: Rect2
var _opponent_region: Rect2
var _own_casting_region: Rect2
var _opponent_casting_region: Rect2
var selected_id: String = ""
var formation_slot_target: int = -1
var target_ids: Dictionary[String, bool] = {}
var target_rings: Dictionary[String, PackedInt32Array] = {}
var _transition: FieldTransition = FieldTransition.new()
var _transition_strength: float = 0.0
var _transition_tween: Tween
var _has_projection: bool = false

func set_action_targets(ids: Array[String], rings: Dictionary[String, PackedInt32Array] = {}) -> void:
	target_ids.clear()
	for id: String in ids:
		target_ids[id] = true
	target_rings = rings.duplicate()
	_clear_keyboard()
	formation_slot_target = -1
	_dragged = null
	_drag_active = false
	queue_redraw()

func clear_action_targets() -> void:
	_clear_keyboard()
	target_ids.clear()
	target_rings.clear()
	formation_slot_target = -1
	queue_redraw()

func highlight_next_formation_slot() -> bool:
	var next_slot: int = 0
	for formation: FormationPresentation in field.own_formations:
		next_slot += maxi(1, formation.body_size)
	if next_slot >= maxi(1, layout.formation_slot_count):
		return false
	formation_slot_target = next_slot
	queue_redraw()
	return true

func set_background_theme(theme_id: String) -> void:
	background_theme = theme_id if theme_id in ["arcane", "slate", "ember"] else "arcane"
	queue_redraw()

func card_bounds(card: CardPresentation) -> Rect2:
	for hit: Dictionary in _card_hits:
		if (hit["card"] as CardPresentation).instance_id == card.instance_id:
			return hit.get("face_rect", hit["rect"])
	return Rect2()

func _ready() -> void:
	if not layout.changed.is_connected(_layout_resource_changed):
		layout.changed.connect(_layout_resource_changed)
	focus_mode = Control.FOCUS_ALL
	clip_contents = true
	texture_filter = CanvasItem.TEXTURE_FILTER_LINEAR
	resized.connect(queue_redraw)
	mouse_exited.connect(_clear_hover)
	if get_window() != null and not get_window().focus_exited.is_connected(_cancel_pointer_state):
		get_window().focus_exited.connect(_cancel_pointer_state)

func _cancel_pointer_state() -> void:
	_dragged = null
	_drag_active = false
	_press_position = Vector2.ZERO
	_clear_keyboard()
	queue_redraw()

func _exit_tree() -> void:
	_cancel_pointer_state()
	if _transition_tween != null and _transition_tween.is_valid():
		_transition_tween.kill()
	_transition_tween = null
	if get_window() != null and get_window().focus_exited.is_connected(_cancel_pointer_state):
		get_window().focus_exited.disconnect(_cancel_pointer_state)

func _layout_resource_changed() -> void:
	queue_redraw()

func configure(value: FieldPresentation) -> void:
	_begin_transition(value)
	field = value
	_has_projection = true
	hand_collapsed = false
	_close_host_menu()
	selected_id = ""
	_dragged = null
	_drag_active = false
	_clear_keyboard()
	_expand_hits.clear()
	_card_hits.clear()
	_opponent_hand_hits.clear()
	_ring_hits.clear()
	_formation_slot_hits.clear()
	_zone_hits.clear()
	target_ids.clear()
	target_rings.clear()
	formation_slot_target = -1
	_clear_hover()

func _begin_transition(next: FieldPresentation) -> void:
	stop_feedback()
	if not _has_projection or Engine.is_editor_hint() or UiFactory.reduced_motion or privacy:
		_transition.clear()
		return
	if _transition.prepare(field, next, _card_hits):
		_transition_strength = 1.0
		_transition_tween = create_tween()
		_transition_tween.tween_method(_set_transition_strength, 1.0, 0.0, 0.24)

func stop_feedback() -> void:
	if _transition_tween != null and _transition_tween.is_valid():
		_transition_tween.kill()
	_transition_tween = null
	_transition.clear()
	_transition_strength = 0.0
	queue_redraw()

func _set_transition_strength(value: float) -> void:
	_transition_strength = value
	queue_redraw()

func set_hand_collapsed(enabled: bool) -> void:
	hand_collapsed = enabled
	_dragged = null
	_drag_active = false
	_clear_keyboard()
	if target_ids.is_empty() and target_rings.is_empty() and formation_slot_target < 0:
		selection_cleared.emit()
	_clear_hover()

func set_privacy(enabled: bool) -> void:
	privacy = enabled
	if enabled:
		_close_host_menu()
		if _transition_tween != null and _transition_tween.is_valid():
			_transition_tween.kill()
		_transition.clear()
		_transition_strength = 0.0
		clear_action_targets()
	selected_id = ""
	_dragged = null
	_drag_active = false
	_expand_hits.clear()
	_card_hits.clear()
	_opponent_hand_hits.clear()
	_ring_hits.clear()
	_formation_slot_hits.clear()
	_zone_hits.clear()
	_clear_keyboard()
	tooltip_text = ""
	queue_redraw()

func _clear_hover() -> void:
	if not _hover_id.is_empty():
		card_hovered.emit(null)
	_hover_id = ""
	tooltip_text = ""
	queue_redraw()

func _draw() -> void:
	_casting_page_hits.clear()
	_expand_hits.clear()
	_card_hits.clear()
	_opponent_hand_hits.clear()
	_ring_hits.clear()
	_formation_slot_hits.clear()
	_zone_hits.clear()
	if size.x < 1.0 or size.y < 1.0:
		return
	if Engine.is_editor_hint():
		if editor_preview:
			_draw_editor_preview()
		return
	if greybox:
		draw_rect(Rect2(Vector2.ZERO, size), Color("18232e"))
	elif background_theme == "slate":
		draw_rect(Rect2(Vector2.ZERO, size), Color("243441"))
	elif background_theme == "ember":
		draw_rect(Rect2(Vector2.ZERO, size), Color("302329"))
	else:
		draw_rect(Rect2(Vector2.ZERO, size), Color("14202b"))
	draw_rect(Rect2(Vector2.ZERO, size), Color(0.031, 0.075, 0.122, 0.18))
	if privacy:
		_draw_label("交接设备后，再揭开场地", Rect2(Vector2(0, size.y * 0.5 - 24), Vector2(size.x, 48)), 26, Color("e4edf0"), true)
		return
	# Keep deck labels clear of the first formation even when the live match
	# shares the viewport with its action panel.
	_update_regions()
	_draw_formations(field.opponent_formations, _opponent_region, false)
	_draw_formations(field.own_formations, _own_region, true)
	_draw_casting(field.opponent_casting, field.opponent_casting_slots, _opponent_casting_region, false)
	_draw_casting(field.own_casting, field.own_casting_slots, _own_casting_region, true)
	_draw_piles(false)
	_draw_piles(true)
	_draw_opponent_hand()
	_draw_hand(size.y * layout.hand_height_ratio)
	_transition.paint(self, _card_hits, _own_region, _opponent_region, _transition_strength)
	if _drag_active and _dragged != null:
		_draw_simple_card(_dragged, Rect2(_pointer - Vector2(40, 55), Vector2(80, 110)), true)
	layout_changed.emit()

func _update_regions() -> void:
	_own_region = _scene_region("OwnAnalysis", layout.field_region(size, layout.own_analysis))
	_opponent_region = _scene_region("OpponentAnalysis", layout.field_region(size, layout.opponent_analysis))
	_own_casting_region = _casting_half(true)
	_opponent_casting_region = _casting_half(false)

func _casting_half(own: bool) -> Rect2:
	var band: Rect2 = layout.field_region(size, layout.own_casting if own else layout.opponent_casting)
	var half_width: float = maxf(0, (band.size.x - layout.slot_gap) * 0.5)
	if not own:
		band.position.x = band.end.x - half_width
	band.size.x = half_width
	return _scene_region("OwnCasting" if own else "OpponentCasting", band)

func casting_control_rect(own: bool) -> Rect2:
	var region: Rect2 = _own_casting_region if own else _opponent_casting_region
	var left: float = 3.0 if own else region.end.x + 3.0
	var right: float = region.position.x - 3.0 if own else size.x - 3.0
	return Rect2(left, region.position.y, maxf(0, right - left), region.size.y)

func _scene_region(node_name: String, fallback: Rect2) -> Rect2:
	var region_node: Control = get_node_or_null(NodePath(node_name)) as Control
	if region_node == null:
		return fallback
	if scene_region_overrides:
		return Rect2(region_node.position, region_node.size)
	region_node.anchor_left = fallback.position.x / size.x
	region_node.anchor_top = fallback.position.y / size.y
	region_node.anchor_right = fallback.end.x / size.x
	region_node.anchor_bottom = fallback.end.y / size.y
	region_node.offset_left = 0.0
	region_node.offset_top = 0.0
	region_node.offset_right = 0.0
	region_node.offset_bottom = 0.0
	return fallback

func _mirror(rect: Rect2) -> Rect2:
	return Rect2(size - rect.end, rect.size)

func _draw_piles(own: bool) -> void:
	var extent: Vector2 = size * layout.pile_size
	var label_size: int = Typography.fitted_size(17, 24)
	var minimum_width: float = ceilf(Typography.HEADING.get_string_size("副卡组", HORIZONTAL_ALIGNMENT_LEFT, -1, label_size).x) + 8
	extent.x = maxf(extent.x, minimum_width)
	var pile_positions: Array[Vector2] = [layout.main_deck_position, layout.ash_position, layout.side_deck_position]
	var names: Array[String] = ["牌库", "灰烬区", "副卡组"]
	var kinds: Array[String] = ["main_deck", "ash", "side_deck"]
	var counts: Array[int] = [field.own_deck_count, field.own_ash_count, field.own_side_count]
	if not own:
		counts.assign([field.opponent_deck_count, field.opponent_ash_count, field.opponent_side_count])
	for index: int in range(3):
		var rect: Rect2 = Rect2(Vector2(size.x * pile_positions[index].x, size.y * pile_positions[index].y), extent)
		var node_names: Array[String] = ["OwnMainDeck", "OwnAsh", "OwnSideDeck"]
		rect = _scene_region(node_names[index], rect)
		if not own:
			rect = _mirror(rect)
		_zone_hits.append({"kind": kinds[index], "owner": field.viewer if own else 1 - field.viewer, "rect": rect})
		draw_rect(rect, Color("20303e"), true)
		draw_rect(rect, Color("78919f"), false, 1)
		if own and kinds[index] == "side_deck" and field.can_set_formation:
			_draw_glow(rect, Color("f2cb62"))
		_draw_label(names[index], Rect2(rect.position + Vector2(4, 10), Vector2(rect.size.x - 8, 24)), 17, Color("e4edf0"), true)
		_draw_label(str(counts[index]), Rect2(rect.position + Vector2(4, 36), Vector2(rect.size.x - 8, 32)), 25, Color("e4edf0"), true)

func _draw_opponent_hand() -> void:
	var count: int = maxi(0, field.opponent_hand_count)
	var extent: Vector2 = _hand_card_size(size.y * 0.066)
	var stride: float = _hand_stride(extent.x, count)
	var start_x: float = (size.x - (count - 1) * stride) * 0.5
	# Derive the opponent silhouette from the same fan, then rotate the whole
	# fan around the field center. No private card ID or definition is needed.
	var mirror: Transform2D = Transform2D(PI, size)
	var rect: Rect2 = Rect2(-extent * 0.5, extent)
	var band: Rect2 = _mirror(_hand_band(extent))
	_scene_region("OpponentHand", band)
	for index: int in _fan_draw_order(count):
		var pose: Transform2D = mirror * _hand_pose(index, count, start_x, stride, extent, false)
		_opponent_hand_hits.append({"pose": pose, "rect": pose * rect, "local_rect": rect})
		draw_set_transform_matrix(pose)
		_draw_editor_card(rect)
	draw_set_transform_matrix(Transform2D.IDENTITY)
	if count > 0:
		_draw_label("对方手牌 %d" % count, _opponent_count_bounds(), 13, Color("b7c6ce"))

func _opponent_count_bounds() -> Rect2:
	var right: float = size.x
	for silhouette: Dictionary in _opponent_hand_hits:
		right = minf(right, (silhouette["rect"] as Rect2).position.x)
	return Rect2(4, 2, maxf(0.0, right - 8), 24)

func _draw_editor_preview() -> void:
	# Editor-only silhouettes use no native service, player data or input hitboxes.
	draw_rect(Rect2(Vector2.ZERO, size), Color("14202b"))
	_update_regions()
	_draw_editor_analysis(_opponent_region, false)
	_draw_editor_analysis(_own_region, true)
	_draw_piles(false)
	_draw_piles(true)
	_zone_hits.clear()
	for region: Rect2 in [_opponent_casting_region, _own_casting_region]:
		draw_rect(region, Color("303540"), true)
		_draw_label("施法区 · 编辑器预览", region.grow(-8), 22, Color("f1dfb3"), true)
	_draw_label("场地布局 · 仅展示数据", Rect2(size.x * 0.35, 4, size.x * 0.30, 48), 24, Color("e4edf0"), true)
	var extent: Vector2 = _hand_card_size(size.y * layout.hand_height_ratio)
	var stride: float = _hand_stride(extent.x, preview_hand_count)
	var total: float = extent.x + (preview_hand_count - 1) * stride
	var start_x: float = (size.x - total + extent.x) * 0.5
	for index: int in _fan_draw_order(preview_hand_count):
		draw_set_transform_matrix(_hand_pose(index, preview_hand_count, start_x, stride, extent, false))
		_draw_editor_card(Rect2(-extent * 0.5, extent))
	draw_set_transform_matrix(Transform2D.IDENTITY)

func _draw_editor_card(rect: Rect2) -> void:
	if not rect.has_area():
		return
	draw_rect(rect, Color("243c4b"), true)
	draw_rect(rect, Color("75919e"), false, 1)

func _draw_label(text: String, rect: Rect2, font_size: int, color: Color = Color("e4edf0"), centered: bool = false) -> void:
	font_size = Typography.fitted_size(font_size, rect.size.y)
	var font: Font = Typography.HEADING
	var displayed: String = text
	while not displayed.is_empty() and font.get_string_size(displayed, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size).x > rect.size.x:
		displayed = displayed.left(displayed.length() - 1)
	if displayed != text:
		while not displayed.is_empty() and font.get_string_size(displayed + "…", HORIZONTAL_ALIGNMENT_LEFT, -1, font_size).x > rect.size.x:
			displayed = displayed.left(displayed.length() - 1)
		displayed += "…"
	var width: float = font.get_string_size(displayed, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size).x
	var x: float = (rect.size.x - width) * 0.5 if centered else 0.0
	var baseline: Vector2 = rect.position + Vector2(x, (rect.size.y - font.get_height(font_size)) * 0.5 + font.get_ascent(font_size))
	draw_string_outline(font, baseline, displayed, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, 3, Color("06121e"))
	draw_string(font, baseline, displayed, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, color)

func _formation_slot(region: Rect2, slot: int, own: bool) -> Rect2:
	var content: Rect2 = layout.analysis_content(region, own)
	var count: int = maxi(1, layout.formation_slot_count)
	var width: float = maxf(0.0, (content.size.x - layout.slot_gap * (count - 1)) / count)
	var column: int = slot if own else count - 1 - slot
	return Rect2(content.position + Vector2(column * (width + layout.slot_gap), 0), Vector2(width, content.size.y))

func _formation_band(cluster: Rect2, own: bool) -> Rect2:
	var height: float = cluster.size.y * 0.45
	return Rect2(cluster.position.x, cluster.position.y if own else cluster.end.y - height, cluster.size.x, height)

func _ring_grid(cluster: Rect2, count: int, own: bool) -> Array[Rect2]:
	var result: Array[Rect2] = []
	if count <= 0:
		return result
	var band_height: float = _formation_band(cluster, own).size.y
	var area: Rect2 = Rect2(cluster.position + Vector2(0, band_height), cluster.size - Vector2(0, band_height)).grow(-3)
	if not area.has_area():
		return result
	# Choose the grid which gives each occupied spell the largest 3:4 face.
	var columns: int = 1
	var widest_face: float = 0
	for candidate: int in range(1, count + 1):
		var rows_needed: int = ceili(float(count) / candidate)
		var portrait_width: float = minf((area.size.x + 3) / candidate - 3, ((area.size.y + 3) / rows_needed - 3) * 0.75)
		if portrait_width > widest_face:
			widest_face = portrait_width
			columns = candidate
	var rows: int = ceili(float(count) / columns)
	var gap: float = minf(3, minf(area.size.x / columns, area.size.y / rows) * 0.1)
	var cell: Vector2 = (area.size - Vector2(columns - 1, rows - 1) * gap) / Vector2(columns, rows)
	for ring: int in range(count):
		var rect: Rect2 = Rect2(area.position + Vector2(ring % columns, ring / columns) * (cell + Vector2.ONE * gap), cell)
		if not own:
			rect.position = cluster.end - (rect.end - cluster.position)
		result.append(rect)
	return result

func _draw_formations(formations: Array[FormationPresentation], region: Rect2, own: bool) -> void:
	draw_rect(region, Color("20303e"), true)
	var slot_count: int = maxi(1, layout.formation_slot_count)
	if _formation_slot(region, 0, own).size.x < 16 or region.size.y < 25:
		return
	var slot: int = 0
	for formation: FormationPresentation in formations:
		var span: int = maxi(1, formation.body_size)
		if slot + span > slot_count:
			break
		var cluster: Rect2 = _formation_slot(region, slot, own).merge(_formation_slot(region, slot + span - 1, own))
		_draw_formation_cluster(formation, cluster, own)
		slot += span
	for empty_slot: int in range(slot, slot_count):
		var empty: Rect2 = _formation_slot(region, empty_slot, own)
		draw_rect(empty, Color("172935"), true)
		var band: Rect2 = _formation_band(empty, own)
		draw_rect(band, Color("243a47"), true)
		var highlighted: bool = own and empty_slot == formation_slot_target
		if highlighted:
			_draw_glow(band.grow(-3), Color("f2cb62"))
			_formation_slot_hits.append({"rect": band, "slot": empty_slot})
		_draw_label("点击设置" if highlighted else "阵法槽 %d" % (empty_slot + 1), band, 13, Color("ffe69c") if highlighted else Color("829aa8"), true)
	var header_height: float = layout.analysis_header_height
	var header_y: float = region.end.y - header_height if own else region.position.y
	_draw_label(("己方" if own else "对方") + "解析区　阵体 %d/%d" % [slot,slot_count], Rect2(region.position.x + 6, header_y, region.size.x - 12, header_height), 12, Color("b7c6ce"))

func _draw_formation_cluster(formation: FormationPresentation, cluster: Rect2, own: bool) -> void:
	draw_rect(cluster, Color("162b36"))
	draw_rect(cluster, Color("4c6673"), false, 1)
	var band: Rect2 = _formation_band(cluster, own)
	draw_rect(band, Color("2c4149") if own else Color("3b3e48"))
	var face_height: float = minf(band.size.y - 4, band.size.x * 0.66 / 0.75)
	var face_size: Vector2 = Vector2(face_height * 0.75, face_height)
	var face: Rect2 = Rect2(band.get_center() - face_size * 0.5, face_size)
	_card_hits.append({"rect": band, "face_rect": face, "art_rect": board_artwork_rect(face, formation.card), "card": formation.card})
	_paint_board_card(formation.card, face)
	if target_rings.has(formation.card.instance_id) and not target_rings[formation.card.instance_id].is_empty():
		draw_rect(band.grow(-2), Color("82db9d"), false, 2)
	else:
		_draw_legal_highlight(formation.card, band.grow(-3))
	if formation.body_size > 1:
		var side_width: float = face.position.x - band.position.x - 5
		_draw_label("阵体 %d" % formation.body_size, Rect2(band.position + Vector2(4, 0), Vector2(side_width, band.size.y)), 14, Color("bfd0d4"), true)
	var rings: Array[Rect2] = _ring_grid(cluster, formation.ring_count, own)
	for ring: int in range(rings.size()):
		var rect: Rect2 = rings[ring]
		_ring_hits.append({"rect": rect, "host": formation.card.instance_id, "ring": ring})
		draw_rect(rect, Color("213743"))
		draw_rect(rect, Color("47626e"), false, 1)
		if formation.occupants.has(ring):
			_draw_card(formation.occupants[ring], rect, not own)
		else:
			_draw_label(str(ring + 1), rect, 13, Color("91a8b1"), true)
		if _legal_ring(formation.card.instance_id, ring):
			draw_rect(rect, Color(0.49, 0.86, 0.61, 0.14))
			draw_rect(rect.grow(-1), Color("82db9d"), false, 2)
			if formation.card.instance_id == _keyboard_id and ring == _keyboard_ring:
				draw_rect(rect.grow(-4), Color("fff2ab"), false, 3)
	if formation.ring_count > 1 or field.attachments.has(formation.card.instance_id) or not formation.occupants.is_empty():
		var available: float = band.end.x - face.end.x - 3
		var width: float = minf(54, available)
		var height: float = minf(28, band.size.y - 4)
		var badge: Rect2 = Rect2(band.end.x - width - 2, band.position.y + 2, width, height)
		_draw_expand_button(formation.card, badge, formation)
	_draw_single_rune(formation.card, face, not own)

func _draw_editor_analysis(region: Rect2, own: bool) -> void:
	draw_rect(region, Color("20303e"))
	for slot: int in range(layout.formation_slot_count):
		var cluster: Rect2 = _formation_slot(region, slot, own)
		_draw_editor_card(_formation_band(cluster, own))
		for ring: Rect2 in _ring_grid(cluster, preview_ring_count, own):
			_draw_editor_card(ring)

func _legal_ring(host_id: String, ring: int) -> bool:
	return target_rings.has(host_id) and target_rings[host_id].has(ring)

static func board_artwork_rect(face: Rect2, card: CardPresentation) -> Rect2:
	if card == null or card.hidden or card.face_down:
		return Rect2()
	return CardPainter.artwork_rect(card, face)

func _paint_board_card(card: CardPresentation, face: Rect2) -> void:
	CardPainter.paint_miniature(self, card, face, card.instance_id in [_hover_id, selected_id])

func _draw_card(card: CardPresentation, cell: Rect2, mirrored: bool = false, casting: bool = false) -> void:
	var rect: Rect2 = cell if casting else CardPainter.fitted_rect(cell.grow(-1))
	var art: Rect2 = rect.grow(-rect.size.x * 0.085) if casting else board_artwork_rect(rect, card)
	_card_hits.append({"rect": cell, "face_rect": rect, "art_rect": art, "card": card})
	if casting:
		CardPainter.paint_art(self, card, rect, card.instance_id in [_hover_id, selected_id])
	else:
		_paint_board_card(card, rect)
	_draw_legal_highlight(card, rect)
	var runes: Array = field.attachments.get(card.instance_id, [])
	if runes.size() > 1:
		var extent: Vector2 = Vector2(minf(44, rect.size.x * 0.42), minf(25, rect.size.y * 0.45))
		_draw_expand_button(card, Rect2(rect.end - extent, extent))
	else:
		_draw_single_rune(card, rect, mirrored)

func _draw_single_rune(host: CardPresentation, rect: Rect2, mirrored: bool) -> void:
	var cards: Array = field.attachments.get(host.instance_id, [])
	if cards.size() != 1:
		return
	var height: float = minf(32, minf(rect.size.x, rect.size.y) * 0.47)
	var extent: Vector2 = Vector2(height * 0.75, height)
	var box: Rect2 = Rect2(rect.position + Vector2(1, rect.size.y - height - 1), extent)
	if mirrored:
		box.position = rect.end - (box.end - rect.position)
	var card: CardPresentation = cards[0] as CardPresentation
	_card_hits.append({"rect": box, "face_rect": box, "art_rect": board_artwork_rect(box, card), "card": card})
	_paint_board_card(card, box)
	_draw_legal_highlight(card, box)

func _marked_card(card: CardPresentation) -> bool:
	return target_ids.has(card.instance_id) if not target_ids.is_empty() else field.legal_cards.has(card.instance_id)

func _has_marked_rune(host: CardPresentation) -> bool:
	for rune: CardPresentation in field.attachments.get(host.instance_id, []):
		if _marked_card(rune):
			return true
	return false

func _draw_expand_button(host: CardPresentation, rect: Rect2, formation: FormationPresentation = null) -> void:
	if not rect.has_area():
		return
	var marked: bool = _has_marked_rune(host)
	if formation != null:
		marked = marked or (target_rings.has(host.instance_id) and not target_rings[host.instance_id].is_empty())
		for occupant: CardPresentation in formation.occupants.values():
			if _marked_card(occupant) or _has_marked_rune(occupant):
				marked = true
				break
	_expand_hits.append({"host": host, "formation": formation, "rect": rect, "has_target": marked})
	draw_rect(rect, Color("3c4859"))
	draw_rect(rect, Color("82db9d") if marked else Color("95abc0"), false, 2 if marked else 1)
	var caption: String = "展开" if formation != null else "符%d" % (field.attachments[host.instance_id] as Array).size()
	_draw_label(caption if rect.size.x >= 30 else "+", rect, 12, Color("eaf0f4"), true)

func _close_host_menu() -> void:
	if _host_menu != null:
		_host_menu.hide()
		_host_menu.clear()

func _add_menu_item(label: String, data: Dictionary, enabled: bool = true) -> void:
	var index: int = _host_menu.item_count
	_host_menu.add_item(label, index)
	_host_menu.set_item_metadata(index, data)
	_host_menu.set_item_disabled(index, not enabled)

func _add_rune_items(host: CardPresentation) -> void:
	for rune: CardPresentation in field.attachments.get(host.instance_id, []):
		var marker: String = "　✓" if _marked_card(rune) else ""
		_add_menu_item("　符文 · " + rune.title + marker, {"card": rune})

func _open_host_menu(hit: Dictionary) -> void:
	if _host_menu == null:
		_host_menu = PopupMenu.new()
		add_child(_host_menu)
		_host_menu.id_pressed.connect(_host_item_pressed)
	_host_menu.clear()
	var host: CardPresentation = hit["host"]
	var formation: FormationPresentation = hit["formation"]
	_add_menu_item(host.title, {"card": host})
	_add_rune_items(host)
	if formation != null:
		for ring: int in range(formation.ring_count):
			var occupant: CardPresentation = formation.occupants.get(ring) as CardPresentation
			var label: String = "环 %d · %s" % [ring + 1, occupant.title if occupant != null else "空"]
			if _legal_ring(host.instance_id, ring):
				_add_menu_item(label + "　✓", {"host": host.instance_id, "ring": ring})
			else:
				_add_menu_item(label, {"card": occupant}, occupant != null)
			if occupant != null:
				_add_rune_items(occupant)
	var anchor: Rect2 = get_global_transform_with_canvas() * (hit["rect"] as Rect2)
	_host_menu.max_size = Vector2i(0, int(get_viewport_rect().size.y * 0.68))
	_host_menu.reset_size()
	_host_menu.popup_on_parent(Rect2i(anchor))

func _host_item_pressed(id: int) -> void:
	if privacy:
		return
	var data: Dictionary = _host_menu.get_item_metadata(_host_menu.get_item_index(id))
	if data.has("ring"):
		if _legal_ring(data["host"], data["ring"]):
			ring_selected.emit(data["host"], data["ring"])
	elif data.get("card") != null:
		card_selected.emit(data["card"])

func _draw_simple_card(card: CardPresentation, rect: Rect2, highlighted: bool = false) -> void:
	CardPainter.paint_miniature(self, card, rect, highlighted)

func _draw_legal_highlight(card: CardPresentation, rect: Rect2) -> void:
	if not target_ids.is_empty():
		if target_ids.has(card.instance_id):
			draw_rect(rect.grow(4), Color(0.49, 0.86, 0.61, 0.18), false, 7)
			draw_rect(rect.grow(2), Color("82db9d"), false, 2)
		return
	var availability: int = field.legal_cards.get(card.instance_id, 0)
	if availability > 0 and not card.hidden:
		if availability & FieldPresentation.LEGAL_GOLD:
			_draw_glow(rect, Color("f2cb62"))
		if availability & FieldPresentation.LEGAL_AMBUSH:
			var outline: Rect2 = rect.grow(5 if availability & FieldPresentation.LEGAL_GOLD else 2)
			var corners: PackedVector2Array = [outline.position, Vector2(outline.end.x, outline.position.y), outline.end, Vector2(outline.position.x, outline.end.y)]
			for index: int in range(corners.size()):
				draw_dashed_line(corners[index], corners[(index + 1) % corners.size()], Color("40c7ff"), 2, 5)

func _draw_glow(rect: Rect2, color: Color) -> void:
	draw_rect(rect.grow(4), Color(color, 0.24), false, 7)
	draw_rect(rect.grow(2), color, false, 2)

func _draw_casting(cards: Array[CardPresentation], slots: int, region: Rect2, own: bool) -> void:
	if region.size.x < 24 or region.size.y < 16:
		return
	draw_rect(region, Color("253b3e") if own else Color("303744"))
	draw_rect(region, Color("536b73"), false, 1)
	var capacity: int = _casting_page_size(cards.size(), region)
	var paged: bool = cards.size() > capacity
	var side: int = 0 if own else 1
	var last_page: int = maxi(0, (cards.size() - 1) / capacity)
	var page: int = clampi(_casting_start[side] / capacity, 0, last_page)
	var first: int = page * capacity
	_casting_start[side] = first
	var count: int = mini(capacity, cards.size() - first)
	var header_height: float = minf(23, region.size.y * 0.4)
	var label: String = "%s施法区　%d/%d" % ["己方" if own else "对方", cards.size(), slots]
	if paged:
		label += "　%d/%d 页" % [page + 1, last_page + 1]
	_draw_label(label, Rect2(region.position, Vector2(region.size.x, header_height)), 14, Color("e4d6b4"), true)
	var track: Rect2 = Rect2(region.position + Vector2(3, header_height), region.size - Vector2(6, header_height + 4))
	var arrow_width: float = minf(28, region.size.x * 0.12) if paged else 0.0
	var gap: float = 4.0
	var available: float = track.size.x - 2 * arrow_width - gap * maxi(0, count - 1)
	var card_width: float = minf(track.size.y, available / maxi(count, 1))
	var x: float = track.get_center().x - (count * card_width + gap * maxi(0, count - 1)) * 0.5
	for index: int in range(count):
		_draw_card(cards[first + index], Rect2(x, track.get_center().y - card_width * 0.5, card_width, card_width), not own, true)
		x += card_width + gap
	if paged:
		for direction: int in [-1, 1]:
			var arrow: Rect2 = Rect2(track.position.x if direction < 0 else track.end.x - arrow_width, track.position.y, arrow_width, track.size.y)
			var enabled: bool = page > 0 if direction < 0 else page < last_page
			var marked: bool = false
			var begin: int = 0 if direction < 0 else first + count
			var end: int = first if direction < 0 else cards.size()
			for index: int in range(begin, end):
				if _marked_card(cards[index]) or _has_marked_rune(cards[index]):
					marked = true
					break
			draw_rect(arrow, Color("344e58") if enabled else Color("24313a"))
			if marked:
				draw_rect(arrow.grow(-1), Color("82db9d"), false, 2)
			_draw_label("‹" if direction < 0 else "›", arrow, 23, Color("e4edf0") if enabled else Color("63717b"), true)
			if enabled:
				_casting_page_hits.append({"rect": arrow, "point": region.get_center(), "direction": direction, "has_target": marked})

func _casting_page_size(count: int, region: Rect2) -> int:
	var capacity: int = maxi(1, floori((region.size.x - 6) / maxf(40, region.size.y - 19)))
	return maxi(1, floori((region.size.x - 62) / maxf(40, region.size.y - 19))) if count > capacity else capacity

func _scroll_casting(point: Vector2, direction: int) -> bool:
	var own: bool = _own_casting_region.has_point(point)
	if not own and not _opponent_casting_region.has_point(point):
		return false
	var region: Rect2 = _own_casting_region if own else _opponent_casting_region
	var count: int = field.own_casting.size() if own else field.opponent_casting.size()
	var capacity: int = _casting_page_size(count, region)
	var side: int = 0 if own else 1
	_casting_start[side] = clampi(_casting_start[side] + direction * capacity, 0, maxi(0, (count - 1) / capacity * capacity))
	_clear_keyboard()
	_clear_hover()
	return true

func _draw_hand(height: float) -> void:
	var extent: Vector2 = _hand_card_size(height)
	var width: float = extent.x
	var count: int = field.hand.size()
	if count == 0 or hand_collapsed:
		return
	var stride: float = _hand_stride(width, count)
	var total: float = width + (count - 1) * stride
	var start_x: float = (size.x - total + width) * 0.5
	var raised_indices: Array[int] = []
	for index: int in _fan_draw_order(count):
		if field.hand[index].instance_id in [_hover_id, selected_id]:
			raised_indices.append(index)
		else:
			_draw_hand_card(index, start_x, stride, extent, false)
	# A raised card is painted last so neighbours cannot cover its face or hit area.
	for raised_index: int in raised_indices:
		_draw_hand_card(raised_index, start_x, stride, extent, true)

func _fan_draw_order(count: int) -> Array[int]:
	var indices: Array[int] = []
	# Paint from both ends toward the center, so endpoint overlaps are mirror
	# images instead of exposing an entire card at only the right edge.
	for depth: int in range(ceili(count * 0.5)):
		indices.append(depth)
		var opposite: int = count - 1 - depth
		if opposite != depth:
			indices.append(opposite)
	return indices

func _hand_card_size(height: float) -> Vector2:
	var card_height: float = minf(layout.hand_max_height, height)
	return Vector2(card_height * 0.75, card_height)

func _hand_stride(width: float, count: int) -> float:
	var hand_width: float = minf(size.x * layout.hand_width_ratio, maxf(width, size.x - hand_side_clearance * 2.0))
	return maxf(0.0, minf(width * layout.hand_card_spacing_ratio, (hand_width - width) / maxi(1, count - 1)))

func _hand_pose(index: int, count: int, start_x: float, stride: float, extent: Vector2, raised: bool) -> Transform2D:
	var fraction: float = (2.0 * index / (count - 1) - 1.0) if count > 1 else 0.0
	var angle: float = deg_to_rad(minf(layout.fan_max_angle, (count - 1) * layout.fan_angle_per_card) * fraction)
	var arch: float = layout.fan_arch_ratio if count > 1 else 0.0
	var band: Rect2 = _hand_band(extent)
	var center: Vector2 = Vector2(start_x + index * stride, band.end.y - extent.y * (0.5 + arch) - layout.hand_bottom_margin + fraction * fraction * extent.y * layout.fan_edge_drop_ratio)
	if raised:
		center.y -= extent.y * layout.hover_raise_ratio
		angle *= 0.2
	# A rotated Rect2 can extend past its unrotated center by a larger amount
	# than either half dimension. Clamp the transformed bounding box so every
	# real hit rectangle stays inside the full-screen field at every viewport.
	center = _clamp_hand_center(center, angle, extent)
	return Transform2D(angle, center)

func _hand_band(extent: Vector2) -> Rect2:
	return _scene_region("OwnHand", Rect2(0, size.y - extent.y, size.x, extent.y))

func _clamp_hand_center(center: Vector2, angle: float, extent: Vector2) -> Vector2:
	var cosine: float = absf(cos(angle))
	var sine: float = absf(sin(angle))
	var half_width: float = cosine * extent.x * 0.5 + sine * extent.y * 0.5
	var half_height: float = sine * extent.x * 0.5 + cosine * extent.y * 0.5
	var margin: float = 1.0
	return Vector2(
		clampf(center.x, half_width + margin, maxf(half_width + margin, size.x - half_width - margin)),
		clampf(center.y, half_height + margin, maxf(half_height + margin, size.y - half_height - margin))
	)

func _draw_hand_card(index: int, start_x: float, stride: float, extent: Vector2, raised: bool) -> void:
	var card: CardPresentation = field.hand[index]
	var pose: Transform2D = _hand_pose(index, field.hand.size(), start_x, stride, extent, raised)
	var rect: Rect2 = Rect2(-extent * 0.5, extent)
	var rest_pose: Transform2D = _hand_pose(index, field.hand.size(), start_x, stride, extent, false)
	# Keep the resting footprint responsive while the hovered card lifts. This
	# prevents raising it away from a stationary pointer and toggling hover.
	_card_hits.append({"rect": pose * rect, "rest_rect": rest_pose * rect, "art_rect": pose * board_artwork_rect(rect, card), "card": card, "pose": pose, "rest_pose": rest_pose, "local_rect": rect})
	draw_set_transform_matrix(pose)
	_draw_simple_card(card, rect, raised or card.instance_id == selected_id)
	_draw_legal_highlight(card, rect)
	draw_set_transform_matrix(Transform2D.IDENTITY)

func _opponent_hand_at(point: Vector2) -> bool:
	for hit: Dictionary in _opponent_hand_hits:
		var pose: Transform2D = hit["pose"]
		if (hit["rect"] as Rect2).has_point(point) and (hit["local_rect"] as Rect2).has_point(pose.affine_inverse() * point):
			return true
	return false

func card_at(point: Vector2) -> CardPresentation:
	if _opponent_hand_at(point):
		return null
	for index: int in range(_card_hits.size() - 1, -1, -1):
		var hit: Dictionary = _card_hits[index]
		var rect: Rect2 = hit["rect"]
		var rest_rect: Rect2 = hit.get("rest_rect", rect)
		if not rect.has_point(point) and not rest_rect.has_point(point):
			continue
		if hit.has("pose"):
			var pose: Transform2D = hit["pose"]
			var rest_pose: Transform2D = hit.get("rest_pose", pose)
			var local_rect: Rect2 = hit["local_rect"]
			if not local_rect.has_point(pose.affine_inverse() * point) and not local_rect.has_point(rest_pose.affine_inverse() * point):
				continue
		return hit["card"] as CardPresentation
	return null

func _gui_input(event: InputEvent) -> void:
	if Engine.is_editor_hint() or privacy:
		return
	if event is InputEventMouseMotion:
		var motion: InputEventMouseMotion = event
		_clear_keyboard()
		_pointer = motion.position
		if _dragged != null and motion.button_mask & MOUSE_BUTTON_MASK_LEFT and _press_position.distance_to(_pointer) > 8:
			_drag_active = true
		var hovered: CardPresentation = card_at(motion.position)
		var new_hover: String = hovered.instance_id if hovered != null else ""
		if _hover_id != new_hover:
			_hover_id = new_hover
			card_hovered.emit(hovered)
		tooltip_text = "" # Full information is shown by the page's fixed detail panel.
		queue_redraw()
	elif event is InputEventMouseButton:
		var mouse: InputEventMouseButton = event
		if mouse.button_index == MOUSE_BUTTON_LEFT and _opponent_hand_at(mouse.position):
			_dragged = null
			_drag_active = false
			accept_event()
			return
		if mouse.pressed and mouse.button_index in [MOUSE_BUTTON_WHEEL_UP, MOUSE_BUTTON_WHEEL_DOWN]:
			var direction: int = -1 if mouse.button_index == MOUSE_BUTTON_WHEEL_UP else 1
			if _scroll_casting(mouse.position, direction):
				accept_event()
		elif mouse.pressed and mouse.button_index == MOUSE_BUTTON_RIGHT:
			_dragged = null
			_drag_active = false
			if selected_id.is_empty():
				hand_toggle_requested.emit()
			else:
				selection_cleared.emit()
			accept_event()
		elif mouse.button_index == MOUSE_BUTTON_LEFT:
			var selected: CardPresentation = card_at(mouse.position)
			if selected != null and field.hand.has(selected):
				if mouse.pressed:
					grab_focus()
					_press_position = mouse.position
					_dragged = selected
				else:
					if not _drag_active:
						card_selected.emit(selected)
					_dragged = null
					_drag_active = false
				accept_event()
				return
			for hit: Dictionary in _expand_hits:
				if (hit["rect"] as Rect2).has_point(mouse.position):
					if not mouse.pressed and not _drag_active:
						_open_host_menu(hit)
					_dragged = null
					_drag_active = false
					accept_event()
					return
			for hit: Dictionary in _casting_page_hits:
				if (hit["rect"] as Rect2).has_point(mouse.position):
					if not mouse.pressed:
						_scroll_casting(hit["point"], int(hit["direction"]))
					accept_event()
					return
			if mouse.pressed:
				grab_focus()
				_press_position = mouse.position
				_dragged = card_at(mouse.position)
			elif _drag_active and _dragged != null:
				for hit: Dictionary in _ring_hits:
					var rect: Rect2 = hit["rect"]
					if rect.has_point(mouse.position):
						placement_requested.emit(_dragged.instance_id, str(hit["host"]), int(hit["ring"]))
						break
			else:
				for hit: Dictionary in _formation_slot_hits:
					if (hit["rect"] as Rect2).has_point(mouse.position):
						formation_slot_selected.emit(int(hit["slot"]))
						_dragged = null
						_drag_active = false
						accept_event()
						return
				for hit: Dictionary in _ring_hits:
					var host: String = str(hit["host"])
					var ring: int = int(hit["ring"])
					if target_rings.has(host) and target_rings[host].has(ring) and (hit["rect"] as Rect2).has_point(mouse.position):
						ring_selected.emit(host, ring)
						_dragged = null
						_drag_active = false
						accept_event()
						return
				if selected != null:
					card_selected.emit(selected)
				else:
					if _own_casting_region.has_point(mouse.position):
						zone_clicked.emit("casting", field.viewer)
					elif _opponent_casting_region.has_point(mouse.position):
						zone_clicked.emit("casting", 1 - field.viewer)
					for hit: Dictionary in _zone_hits:
						if hit["rect"].has_point(mouse.position):
							zone_clicked.emit(str(hit["kind"]), int(hit["owner"]))
							accept_event()
							break
			if not mouse.pressed:
				_dragged = null
				_drag_active = false
				queue_redraw()
	elif event.is_action_pressed(&"ui_left") or event.is_action_pressed(&"ui_right"):
		var direction: int = -1 if event.is_action_pressed(&"ui_left") else 1
		if _move_keyboard_focus(direction):
			queue_redraw()
			accept_event()
	elif event.is_action_pressed(&"ui_down"):
		for hit: Dictionary in _expand_hits:
			if (hit["host"] as CardPresentation).instance_id == _hover_id:
				_open_host_menu(hit)
				accept_event()
				return
	elif event.is_action_pressed(&"ui_accept"):
		if _keyboard_ring >= 0 and _legal_ring(_keyboard_id, _keyboard_ring):
			ring_selected.emit(_keyboard_id, _keyboard_ring)
			accept_event()
		elif not _keyboard_id.is_empty():
			var card: CardPresentation = _keyboard_cards().get(_keyboard_id)
			if card != null:
				card_selected.emit(card)
				accept_event()
		elif formation_slot_target >= 0:
			formation_slot_selected.emit(formation_slot_target)
			accept_event()

func _clear_keyboard() -> void:
	_keyboard_id = ""
	_keyboard_ring = -1

func _keyboard_cards() -> Dictionary[String, CardPresentation]:
	var cards: Dictionary[String, CardPresentation] = {}
	if not hand_collapsed:
		for card: CardPresentation in field.hand:
			cards[card.instance_id] = card
	for track: Array in [field.own_casting, field.opponent_casting]:
		for card: CardPresentation in track:
			cards[card.instance_id] = card
	for hit: Dictionary in _card_hits:
		var card: CardPresentation = hit["card"]
		cards[card.instance_id] = card
	if not target_ids.is_empty():
		for id: String in cards.keys():
			if not target_ids.has(id) and not _has_marked_rune(cards[id]):
				cards.erase(id)
	return cards

func _move_keyboard_focus(direction: int) -> bool:
	if not target_rings.is_empty():
		var rings: Array[Dictionary] = []
		var current: int = -1
		for hit: Dictionary in _ring_hits:
			var host: String = hit["host"]
			var ring: int = hit["ring"]
			if _legal_ring(host, ring):
				if host == _keyboard_id and ring == _keyboard_ring:
					current = rings.size()
				rings.append(hit)
		if rings.is_empty():
			return false
		var next: Dictionary = rings[_next_focus(current, direction, rings.size())]
		_keyboard_id = next["host"]
		_keyboard_ring = next["ring"]
		_hover_id = _keyboard_id
		return true
	var cards: Dictionary[String, CardPresentation] = _keyboard_cards()
	if cards.is_empty():
		return false
	var ids: Array[String] = cards.keys()
	_keyboard_id = ids[_next_focus(ids.find(_keyboard_id), direction, ids.size())]
	_keyboard_ring = -1
	_hover_id = _keyboard_id
	var card: CardPresentation = cards[_keyboard_id]
	for side: int in range(2):
		var track: Array[CardPresentation] = field.own_casting if side == 0 else field.opponent_casting
		var index: int = track.find(card)
		if index >= 0:
			var region: Rect2 = _own_casting_region if side == 0 else _opponent_casting_region
			var page_size: int = _casting_page_size(track.size(), region)
			_casting_start[side] = index / page_size * page_size
			break
	card_hovered.emit(card)
	return true

func _next_focus(current: int, direction: int, count: int) -> int:
	if current < 0:
		return 0 if direction > 0 else count - 1
	return wrapi(current + direction, 0, count)

func zone_at(point: Vector2) -> Dictionary:
	for index: int in range(_zone_hits.size() - 1, -1, -1):
		var hit: Dictionary = _zone_hits[index]
		if hit["rect"].has_point(point):
			return hit
	return {}
