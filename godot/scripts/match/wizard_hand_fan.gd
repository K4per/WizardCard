class_name WizardHandFan
extends Control

signal selected(card: Dictionary, generation: String, revision: String)
signal canceled
signal inspected(card: Dictionary)
signal reordered(source: String, before: String, generation: String, revision: String)
const CARD_SCENE: PackedScene = preload("res://scenes/match/hand_card.tscn")
@export var card_size: Vector2 = Vector2(100, 136)
var cards: Dictionary = {}
var order: Array[String] = []
var scroll_offset: float = 0.0
var hovered: String = ""
var generation: String = ""
var revision: String = ""
var _context: String = ""
var _motions: Array[Tween] = []

func _ready() -> void:
	clip_contents = true
	resized.connect(arrange)

func clear_cards() -> void:
	finish_motion()
	for node: WizardCardView in cards.values():
		remove_child(node)
		node.queue_free()
	cards.clear()
	order.clear()
	hovered = ""
	scroll_offset = 0
	_context = ""

func apply(skin: WizardSkin, snapshot: Dictionary, interaction: Dictionary, sorted_ids: Array[String]) -> void:
	var context: String = str(snapshot["generation"]) + ":" + str(snapshot["viewer"])
	if context != _context:
		clear_cards()
		_context = context
	if revision != str(snapshot["revision"]):
		finish_motion()
	generation = str(snapshot["generation"])
	revision = str(snapshot["revision"])
	order = sorted_ids.duplicate()
	var visible_cards: Dictionary = {}
	for raw: Variant in snapshot["cards"] as Array:
		var data: Dictionary = raw as Dictionary
		if order.has(str(data["id"])):
			visible_cards[str(data["id"])] = data
	for id: String in cards.keys():
		if not visible_cards.has(id):
			var removed: WizardCardView = cards[id] as WizardCardView
			remove_child(removed)
			removed.queue_free()
			cards.erase(id)
	for id: String in order:
		if not visible_cards.has(id):
			continue
		var data: Dictionary = visible_cards[id] as Dictionary
		var node: WizardCardView = cards.get(id) as WizardCardView
		if node == null:
			node = CARD_SCENE.instantiate() as WizardCardView
			add_child(node)
			node.configure(skin, data, "hand")
			node.texture_filter = CanvasItem.TEXTURE_FILTER_LINEAR_WITH_MIPMAPS
			node.tooltip_text = ""
			cards[id] = node
			node.selected.connect(func(card: Dictionary) -> void: selected.emit(card, node.drag_generation, node.drag_revision))
			node.secondary.connect(func(_card: Dictionary) -> void:
				if node.drag_generation == generation and node.drag_revision == revision:
					canceled.emit())
			node.reordered.connect(func(source: String, before: String) -> void: reordered.emit(source, before, node.drag_generation, node.drag_revision))
			node.mouse_entered.connect(_hover.bind(id))
			node.mouse_exited.connect(_hover.bind(""))
		node.card = data
		node.definition = data.get("definition", {}) as Dictionary
		node.highlighted = str(interaction.get("selected", "0")) == id
		node.candidate = (interaction.get("candidates", []) as Array).has(id)
		(node as WizardHandCard).interaction_state = "selected" if node.highlighted else (("cost" if int(interaction.get("step", 0)) == 2 else "candidate") if node.candidate else "")
		node.drag_enabled = true
		node.drag_context = "match"
		node.drag_generation = generation
		node.drag_revision = revision
		node.queue_redraw()
	arrange()

func _hover(id: String) -> void:
	hovered = id
	arrange()
	if cards.has(id):
		inspected.emit((cards[id] as WizardCardView).card)

func arrange() -> void:
	if order.is_empty():
		return
	var spacing: float = clampf((size.x - card_size.x - 60) / maxf(1, order.size() - 1), 38, 110)
	var width: float = (order.size() - 1) * spacing + card_size.x
	scroll_offset = clampf(scroll_offset, 0, maxf(0, width - size.x + 50))
	var start: float = maxf(25, (size.x - width) * 0.5) - scroll_offset
	for index: int in range(order.size()):
		var id: String = order[index]
		var node: WizardCardView = cards.get(id) as WizardCardView
		if node == null:
			continue
		var ratio: float = (float(index) / maxf(1, order.size() - 1) - 0.5) * 2
		node.size = card_size
		node.pivot_offset = Vector2(card_size.x * 0.5, card_size.y)
		node.position = Vector2(start + index * spacing, 24 + absf(ratio) * 8)
		node.rotation = ratio * 0.09
		node.scale = Vector2.ONE
		if id == hovered:
			node.position.y -= 22
			node.rotation = 0
			node.scale = Vector2(1.1, 1.1)
			move_child(node, -1)
		else:
			move_child(node, mini(index, get_child_count() - 1))
	if cards.has(hovered):
		move_child(cards[hovered] as Node, -1)

func _gui_input(event: InputEvent) -> void:
	if event is InputEventMouseButton:
		var button: InputEventMouseButton = event as InputEventMouseButton
		if button.pressed and button.button_index in [MOUSE_BUTTON_WHEEL_UP, MOUSE_BUTTON_WHEEL_DOWN]:
			scroll_offset += 70 * (1 if button.button_index == MOUSE_BUTTON_WHEEL_DOWN else -1)
			arrange()
			accept_event()

func _can_drop_data(_position: Vector2, data: Variant) -> bool:
	return data is Dictionary and str((data as Dictionary).get("generation", "")) == generation and str((data as Dictionary).get("revision", "")) == revision and str((data as Dictionary).get("context", "")) == "match"

func _drop_data(_position: Vector2, data: Variant) -> void:
	reordered.emit(str((data as Dictionary)["id"]), "", generation, revision)

func draw_card(id: String, reduced: bool) -> void:
	var node: WizardCardView = cards.get(id) as WizardCardView
	if node == null:
		return
	var destination: Vector2 = node.position
	node.position.y += 45 if reduced else 150
	node.modulate.a = 0.2
	var tween: Tween = create_tween().set_trans(Tween.TRANS_CUBIC).set_ease(Tween.EASE_OUT)
	tween.tween_property(node, "position", destination, 0.18 if reduced else 0.4)
	tween.parallel().tween_property(node, "modulate:a", 1.0, 0.18 if reduced else 0.4)
	_motions.append(tween)

func finish_motion() -> void:
	for tween: Tween in _motions:
		if tween.is_valid():
			tween.kill()
	_motions.clear()
	for node: WizardCardView in cards.values():
		node.modulate.a = 1
	arrange()

func set_paused(paused: bool) -> void:
	for tween: Tween in _motions:
		if tween.is_valid():
			if paused:
				tween.pause()
			else:
				tween.play()
