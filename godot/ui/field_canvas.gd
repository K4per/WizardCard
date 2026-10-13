@tool
class_name FieldCanvas
extends Control

signal card_selected(card: CardPresentation)
signal card_hovered(card: CardPresentation)
signal selection_cleared()
signal hand_toggle_requested()
signal placement_requested(card_id: String, host_id: String, ring_index: int)
signal ring_selected(host_id: String, ring_index: int)
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
var _card_hits: Array[Dictionary] = []
var _opponent_hand_hits: Array[Dictionary] = []
var _ring_hits: Array[Dictionary] = []
var _zone_hits: Array[Dictionary] = []
var _hover_id: String = ""
var _keyboard_index: int = -1
var _own_start: int = 0
var _opponent_start: int = 0
var _press_position: Vector2
var _dragged: CardPresentation
var _drag_active: bool = false
var _pointer: Vector2
var _own_region: Rect2
var _opponent_region: Rect2
var _own_casting_region: Rect2
var _opponent_casting_region: Rect2
var selected_id: String = ""
var target_ids: Dictionary[String, bool] = {}
var target_rings: Dictionary[String, PackedInt32Array] = {}

func set_action_targets(ids: Array[String], rings: Dictionary[String, PackedInt32Array] = {}) -> void:
	target_ids.clear()
	for id: String in ids:
		target_ids[id] = true
	target_rings = rings.duplicate()
	_dragged = null
	_drag_active = false
	queue_redraw()

func clear_action_targets() -> void:
	target_ids.clear()
	target_rings.clear()
	queue_redraw()

func set_background_theme(theme_id: String) -> void:
	background_theme = theme_id if theme_id in ["arcane", "slate", "ember"] else "arcane"
	queue_redraw()

func card_bounds(card: CardPresentation) -> Rect2:
	for hit: Dictionary in _card_hits:
		if (hit["card"] as CardPresentation).instance_id == card.instance_id:
			return hit["rect"]
	return Rect2()

func _ready() -> void:
	if not layout.changed.is_connected(_layout_resource_changed):
		layout.changed.connect(_layout_resource_changed)
	focus_mode = Control.FOCUS_ALL
	clip_contents = true
	texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	resized.connect(queue_redraw)
	mouse_exited.connect(_clear_hover)

func _layout_resource_changed() -> void:
	queue_redraw()

func configure(value: FieldPresentation) -> void:
	field = value
	hand_collapsed = false
	_own_start = 0
	_opponent_start = 0
	selected_id = ""
	_dragged = null
	_drag_active = false
	_keyboard_index = -1
	_card_hits.clear()
	_opponent_hand_hits.clear()
	_ring_hits.clear()
	_zone_hits.clear()
	_clear_hover()

func set_hand_collapsed(enabled: bool) -> void:
	hand_collapsed = enabled
	_dragged = null
	_drag_active = false
	_keyboard_index = -1
	selection_cleared.emit()
	_clear_hover()

func set_privacy(enabled: bool) -> void:
	privacy = enabled
	selected_id = ""
	_dragged = null
	_drag_active = false
	_card_hits.clear()
	_opponent_hand_hits.clear()
	_ring_hits.clear()
	_zone_hits.clear()
	_keyboard_index = -1
	tooltip_text = ""
	queue_redraw()

func _clear_hover() -> void:
	if not _hover_id.is_empty():
		card_hovered.emit(null)
	_hover_id = ""
	tooltip_text = ""
	queue_redraw()

func _draw() -> void:
	_card_hits.clear()
	_opponent_hand_hits.clear()
	_ring_hits.clear()
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
	_draw_formations(field.opponent_formations, _opponent_region, _opponent_start, false)
	_draw_formations(field.own_formations, _own_region, _own_start, true)
	_draw_casting(field.opponent_casting, field.opponent_casting_slots, _opponent_casting_region, false)
	_draw_casting(field.own_casting, field.own_casting_slots, _own_casting_region, true)
	_draw_piles(false)
	_draw_piles(true)
	_draw_opponent_hand()
	_draw_hand(size.y * layout.hand_height_ratio)
	if _drag_active and _dragged != null:
		_draw_simple_card(_dragged, Rect2(_pointer - Vector2(40, 55), Vector2(80, 110)), true)
	layout_changed.emit()

func _update_regions() -> void:
	_own_region = _scene_region("OwnAnalysis", layout.field_region(size, layout.own_analysis))
	_opponent_region = _scene_region("OpponentAnalysis", layout.field_region(size, layout.opponent_analysis))
	_own_casting_region = _scene_region("OwnCasting", layout.field_region(size, layout.own_casting))
	_opponent_casting_region = _scene_region("OpponentCasting", layout.field_region(size, layout.opponent_casting))

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
	var extent: Vector2 = _hand_card_size(size.y * layout.hand_height_ratio)
	var stride: float = _hand_stride(extent.x, count)
	var start_x: float = (size.x - (count - 1) * stride) * 0.5
	# Derive the opponent silhouette from the same fan, then rotate the whole
	# fan around the field center. No private card ID or definition is needed.
	var mirror: Transform2D = Transform2D(PI, size)
	var card: CardPresentation = CardPresentation.anonymous()
	var rect: Rect2 = Rect2(-extent * 0.5, extent)
	var band: Rect2 = _mirror(_hand_band(extent))
	_scene_region("OpponentHand", band)
	for index: int in _fan_draw_order(count):
		var pose: Transform2D = mirror * _hand_pose(index, count, start_x, stride, extent, false)
		_opponent_hand_hits.append({"pose": pose, "rect": pose * rect, "local_rect": rect})
		draw_set_transform_matrix(pose)
		_draw_simple_card(card, rect)
	draw_set_transform_matrix(Transform2D.IDENTITY)

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

func _analysis_content(region: Rect2) -> Rect2:
	var content: Rect2 = layout.analysis_content(region)
	if region == _opponent_region:
		content.position.y -= minf(layout.analysis_header_height, maxf(0.0, region.size.y - region.size.y * layout.analysis_safe_margin.y * 2.0))
	return content

func _formation_slot(region: Rect2, slot: int, own: bool) -> Rect2:
	var content: Rect2 = _analysis_content(region)
	var count: int = maxi(1, layout.formation_slot_count)
	var height: float = maxf(0.0, (content.size.y - layout.slot_gap * (count - 1)) / count)
	var rect: Rect2 = Rect2(content.position + Vector2(0, slot * (height + layout.slot_gap)), Vector2(content.size.x, height))
	if not own:
		rect.position = content.end - (rect.end - content.position)
	return rect

func _draw_formations(formations: Array[FormationPresentation], region: Rect2, _start: int, own: bool) -> void:
	draw_rect(region, Color("20303e"), true)
	var slot_count: int = maxi(1, layout.formation_slot_count)
	# Controls can briefly have a tiny size while a page enters the tree.
	# Wait for layout instead of forming negative rectangles during that frame.
	if _analysis_content(region).size.y <= layout.slot_gap * (slot_count - 1) + slot_count * 10.0:
		return
	var slot: int = 0
	for formation: FormationPresentation in formations:
		var span: int = maxi(1, formation.body_size)
		# The native engine admits at most five total body units. Never shrink a
		# formation to make an invalid projection appear to fit an occupied slot.
		if slot + span > slot_count:
			break
		var cluster: Rect2 = _formation_slot(region, slot, own).merge(_formation_slot(region, slot + span - 1, own))
		draw_rect(cluster, Color("152632"), true)
		draw_rect(cluster, Color("465e6c"), false, 1)
		_draw_formation_cluster(formation, cluster, own)
		slot += span
	for empty_slot: int in range(slot, slot_count):
		var empty: Rect2 = _formation_slot(region, empty_slot, own)
		draw_rect(empty, Color("152632"), true)
		draw_rect(empty, Color("465e6c"), false, 1)
		_draw_empty_formation_lane(empty, empty_slot, own)
	var inset: Vector2 = region.size * layout.analysis_safe_margin
	var header_position: Vector2 = region.position + inset if own else Vector2(region.position.x + inset.x, region.end.y - inset.y - 24)
	_draw_label(("我方" if own else "对手") + " · 解析区（%d槽）" % layout.formation_slot_count, Rect2(header_position, Vector2(region.size.x - inset.x * 2, 24)), 16, Color("b7c6ce"))

func _draw_formation_cluster(formation: FormationPresentation, region: Rect2, own: bool = true) -> void:
	var count: int = maxi(0, formation.ring_count)
	var padding: float = 5.0
	var host_width: float = maxf(48.0, region.size.x * layout.formation_slot_width_ratio)
	var host_rect: Rect2 = Rect2(region.position + Vector2.ONE * padding, Vector2(host_width, region.size.y - padding * 2))
	var grid: Rect2 = Rect2(host_rect.end.x + padding, region.position.y + padding, region.size.x - host_width - padding * 3, region.size.y - padding * 2)
	# A formation occupies consecutive body slots. Distribute its actual rings
	# within that entire span instead of giving every formation a fixed row.
	var rows: int = mini(maxi(1, formation.body_size), maxi(1, count))
	var columns: int = maxi(1, ceili(float(count) / rows))
	var gap: float = 3.0
	var extent: Vector2 = Vector2((grid.size.x - gap * (columns - 1)) / columns, (grid.size.y - gap * (rows - 1)) / rows)
	if not own:
		host_rect.position.x = region.end.x - padding - host_width
	for ring: int in range(count):
		var ring_rect: Rect2 = Rect2(grid.position + Vector2(ring % columns, ring / columns) * (extent + Vector2.ONE * gap), extent)
		if not own:
			ring_rect.position = region.end - (ring_rect.end - region.position)
		_ring_hits.append({"rect": ring_rect, "host": formation.card.instance_id, "ring": ring})
		draw_rect(ring_rect, Color("263c49"), true)
		draw_rect(ring_rect, Color("506e7d"), false, 1)
		if target_rings.has(formation.card.instance_id) and target_rings[formation.card.instance_id].has(ring):
			draw_rect(ring_rect, Color(0.49, 0.86, 0.61, 0.22), true)
			draw_rect(ring_rect.grow(-1), Color("82db9d"), false, 2)
		if formation.occupants.has(ring):
			_draw_card(formation.occupants[ring], ring_rect, not own)
		else:
			_draw_label(str(ring + 1), ring_rect, 11, Color("77929f"), true)
	_draw_card(formation.card, host_rect, not own)

func _draw_empty_formation_lane(region: Rect2, slot: int, own: bool) -> void:
	var padding: float = 4.0
	var host_width: float = maxf(48.0, region.size.x * layout.formation_slot_width_ratio)
	var host: Rect2 = Rect2(region.position + Vector2.ONE * padding, Vector2(host_width, region.size.y - padding * 2))
	var lane: Rect2 = Rect2(host.end.x + padding, host.position.y, region.size.x - host_width - padding * 3, host.size.y)
	if not own:
		host.position.x = region.end.x - padding - host_width
		lane.position.x = region.position.x + padding
	draw_rect(host, Color("223642"), true)
	draw_rect(host, Color("567180"), false, 1)
	draw_rect(lane, Color("1d2f3c"), true)
	draw_rect(lane, Color("3f5968"), false, 1)
	_draw_label("阵法槽 %d" % (slot + 1), host.grow(-2), 12, Color("829aa8"), true)

func _draw_editor_analysis(region: Rect2, own: bool) -> void:
	# Editor preview mirrors the runtime lane structure but never creates card
	# hitboxes.  This keeps scene authorship predictable while still showing all
	# five body slots in the inspector.
	draw_rect(region, Color("20303e"), true)
	var slot_count: int = maxi(1, layout.formation_slot_count)
	for slot: int in range(slot_count):
		var row: Rect2 = _formation_slot(region, slot, own)
		var host_width: float = maxf(48.0, row.size.x * layout.formation_slot_width_ratio)
		var host: Rect2 = Rect2(row.position + Vector2(5, 5), Vector2(host_width, maxf(1.0, row.size.y - 10)))
		var lane: Rect2 = Rect2(host.end.x + 5, row.position.y + 5, maxf(1.0, row.size.x - host_width - 15), maxf(1.0, row.size.y - 10))
		if not own:
			host.position.x = row.end.x - 5 - host_width
			lane = Rect2(row.position.x + 5, row.position.y + 5, maxf(1.0, row.size.x - host_width - 15), maxf(1.0, row.size.y - 10))
		_draw_editor_card(host)
		var count: int = maxi(1, preview_ring_count)
		var gap: float = 3.0
		var side: float = maxf(1.0, (lane.size.x - gap * (count - 1)) / count)
		for ring: int in range(count):
			var ring_x: float = lane.position.x + ring * (side + gap)
			if not own:
				ring_x = lane.end.x - side - ring * (side + gap)
			_draw_editor_card(Rect2(ring_x, lane.position.y, side, lane.size.y))

func _draw_card(card: CardPresentation, rect: Rect2, mirrored: bool = false) -> void:
	_card_hits.append({"rect": rect, "card": card})
	_draw_simple_card(card, rect, card.instance_id == _hover_id or card.instance_id == selected_id)
	_draw_legal_highlight(card, rect)
	_draw_attachment_boxes(card, rect, mirrored)

func _draw_attachment_boxes(host: CardPresentation, rect: Rect2, mirrored: bool) -> void:
	var cards: Array = field.attachments.get(host.instance_id, [])
	if cards.is_empty():
		return
	var side: float = minf(clampf(rect.size.y * 0.45, 8.0, 13.0), minf(rect.size.x, rect.size.y) - 2.0)
	if side <= 0.0:
		return
	var gap: float = 2.0
	var stride: float = minf(side + gap, maxf(0.0, (rect.size.x - side - 2.0) / maxi(1, cards.size() - 1)))
	for index: int in range(cards.size()):
		var card: CardPresentation = cards[index] as CardPresentation
		var box: Rect2 = Rect2(rect.position.x + 1.0 + index * stride, rect.end.y - side - 1.0, side, side)
		if mirrored:
			box.position = rect.end - (box.end - rect.position)
		_card_hits.append({"rect": box, "card": card})
		draw_rect(box, Color("453658"), true)
		draw_rect(box, Color("cbb2ec") if card.instance_id == _hover_id or card.instance_id == selected_id else Color("aa8fd0"), false, 1.0)
		_draw_label("符", box.grow(-1), maxi(6, int(side - 2.0)), Color("ecdef9"), true)
		_draw_legal_highlight(card, box)

func _draw_simple_card(card: CardPresentation, rect: Rect2, highlighted: bool = false) -> void:
	SimpleCardPainter.paint(self, card, rect, highlighted)

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
			_draw_glow(rect.grow(3 if availability & FieldPresentation.LEGAL_GOLD else 0), Color("40c7ff"))

func _draw_glow(rect: Rect2, color: Color) -> void:
	draw_rect(rect.grow(4), Color(color, 0.24), false, 7)
	draw_rect(rect.grow(2), color, false, 2)

func _draw_casting(cards: Array[CardPresentation], _slots: int, region: Rect2, own: bool) -> void:
	if greybox:
		draw_rect(region, Color("3a3938"))
		draw_rect(region, Color("c9b894"), false, 1)
	else:
		draw_rect(region, Color("303540"), true)
	if cards.is_empty():
		_draw_label("施法区（%s）" % ("我方" if own else "对手"), region.grow(-8), 22, Color("f1dfb3"), true)
	# Casting is a compact horizontal track.  At 720p its height is only about
	# 32px, so using a square card would collapse into an unreadable 16px tile.
	# Keep a short card strip instead and fit several cards without leaving the
	# track bounds.
	var card_height: float = clampf(region.size.y - 8.0, 18.0, 26.0)
	var card_width: float = minf(110.0, maxf(34.0, (region.size.x - 64.0) / maxi(cards.size(), 1) - 6.0))
	var gap: float = 6.0
	var total_width: float = cards.size() * card_width + maxi(0, cards.size() - 1) * gap
	var start_x: float = region.position.x + maxf(8.0, (region.size.x - total_width) * 0.5)
	for index: int in range(cards.size()):
		var x: float = start_x + index * (card_width + gap)
		if not own:
			x = region.end.x - (start_x - region.position.x) - card_width - index * (card_width + gap)
		_draw_card(cards[index], Rect2(Vector2(x, region.position.y + (region.size.y - card_height) * 0.5), Vector2(card_width, card_height)), not own)

func _visible_groups(_region: Rect2) -> int:
	return layout.formation_slot_count

func _tile_side() -> float:
	return minf(layout.tile_max_size, size.y * layout.tile_height_ratio)

func _draw_hand(height: float) -> void:
	var extent: Vector2 = _hand_card_size(height)
	var width: float = extent.x
	var count: int = field.hand.size()
	if count == 0 or hand_collapsed:
		return
	var stride: float = _hand_stride(width, count)
	var total: float = width + (count - 1) * stride
	var start_x: float = (size.x - total + width) * 0.5
	var raised_index: int = -1
	for index: int in _fan_draw_order(count):
		if field.hand[index].instance_id == _hover_id:
			raised_index = index
		else:
			_draw_hand_card(index, start_x, stride, extent, false)
	# A raised card is painted last so neighbours cannot cover its face or hit area.
	if raised_index >= 0:
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
	return Vector2(card_height * 160.0 / 224.0, card_height)

func _hand_stride(width: float, count: int) -> float:
	return maxf(0.0, minf(width * layout.hand_card_spacing_ratio, (size.x * layout.hand_width_ratio - width) / maxi(1, count - 1)))

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
	_card_hits.append({"rect": pose * rect, "rest_rect": rest_pose * rect, "card": card, "pose": pose, "rest_pose": rest_pose, "local_rect": rect})
	draw_set_transform_matrix(pose)
	draw_rect(Rect2(rect.position + Vector2(3, 5), rect.size), Color(0, 0, 0, 0.3))
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
			_scroll_formations(mouse.position, -1 if mouse.button_index == MOUSE_BUTTON_WHEEL_UP else 1)
		elif mouse.pressed and mouse.button_index == MOUSE_BUTTON_RIGHT:
			_dragged = null
			_drag_active = false
			if selected_id.is_empty():
				hand_toggle_requested.emit()
			else:
				selection_cleared.emit()
			accept_event()
		elif mouse.button_index == MOUSE_BUTTON_LEFT:
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
				for hit: Dictionary in _ring_hits:
					var host: String = str(hit["host"])
					var ring: int = int(hit["ring"])
					if target_rings.has(host) and target_rings[host].has(ring) and (hit["rect"] as Rect2).has_point(mouse.position):
						ring_selected.emit(host, ring)
						_dragged = null
						_drag_active = false
						accept_event()
						return
				var selected: CardPresentation = card_at(mouse.position)
				if selected != null:
					card_selected.emit(selected)
				else:
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
		if _card_hits.is_empty():
			return
		_keyboard_index = wrapi(_keyboard_index + (-1 if event.is_action_pressed(&"ui_left") else 1), 0, _card_hits.size())
		var card: CardPresentation = _card_hits[_keyboard_index]["card"]
		_hover_id = card.instance_id
		card_hovered.emit(card)
		queue_redraw()
		accept_event()
	elif event.is_action_pressed(&"ui_accept") and _keyboard_index >= 0 and _keyboard_index < _card_hits.size():
		card_selected.emit(_card_hits[_keyboard_index]["card"] as CardPresentation)
		accept_event()

func _scroll_formations(point: Vector2, direction: int) -> void:
	var region: Rect2 = _own_region if _own_region.has_point(point) else _opponent_region
	var capacity: int = _visible_groups(region)
	if _own_region.has_point(point):
		_own_start = clampi(_own_start + direction, 0, maxi(0, field.own_formations.size() - capacity))
	elif _opponent_region.has_point(point):
		_opponent_start = clampi(_opponent_start + direction, 0, maxi(0, field.opponent_formations.size() - capacity))
	_keyboard_index = -1
	_clear_hover()

func zone_at(point: Vector2) -> Dictionary:
	for index: int in range(_zone_hits.size() - 1, -1, -1):
		var hit: Dictionary = _zone_hits[index]
		if hit["rect"].has_point(point):
			return hit
	return {}
