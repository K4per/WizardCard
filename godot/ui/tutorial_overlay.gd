extends CanvasLayer

signal continue_requested

const TextCatalog = preload("res://app/text.gd")
const GAP := 8.0

@onready var _text: TextCatalog = get_node("/root/Text") as TextCatalog
@onready var surface: Control = %Surface
@onready var bar: PanelContainer = %HintBar
@onready var hint: Label = %Hint
@onready var _blocked_hint: Label = %BlockedHint
@onready var _instructions: VBoxContainer = %Instructions
@onready var _row: HBoxContainer = %Row
@onready var advance: Button = %Continue
@onready var _timer: Timer = %BlockedTimer

var _lesson: Dictionary = {}
var _resolve: Callable
var _obstructions: Array[Control] = []
var highlights: Array[Rect2] = []
var _last_layout: Array = []
var _bar_size: Vector2 = Vector2.ZERO
var _pulse: float = 0.0

func _ready() -> void:
	advance.text = _text.t("ui.lesson.continue")
	advance.add_theme_font_override(&"font", Typography.HEADING)
	advance.add_theme_stylebox_override(&"normal", DeckTheme.box(Color("215568"), DeckTheme.ACCENT, 8))
	advance.add_theme_stylebox_override(&"hover", DeckTheme.box(Color("2b6779"), DeckTheme.ACCENT, 8))
	advance.pressed.connect(continue_requested.emit)
	surface.resized.connect(_scale_text)
	_scale_text()
	surface.draw.connect(_draw_highlights)
	bar.minimum_size_changed.connect(_resize_bar)
	_timer.timeout.connect(_clear_blocked)
	visibility_changed.connect(_visibility_changed)
	set_process(false)

func _scale_text() -> void:
	var factor: float = clampf(minf(surface.size.x / 1600.0, surface.size.y / 900.0), 0.94, 1.32)
	Typography.override_size(hint, &"font_size", roundi(24 * factor))
	Typography.override_size(_blocked_hint, &"font_size", roundi(21 * factor))
	Typography.override_size(advance, &"font_size", roundi(23 * factor))

func configure(lesson: Dictionary, resolver: Callable, obstructions: Array[Control]) -> void:
	# Editor lessons identify their steps by hint key, match snapshots carry a step ID.
	var changed_step: bool = _lesson.get("lessonId") != lesson.get("lessonId") or _lesson.get("step", _lesson.get("hintKey")) != lesson.get("step", lesson.get("hintKey"))
	_lesson = lesson
	_resolve = resolver
	_obstructions = obstructions
	visible = not lesson.is_empty() and not bool(lesson.get("complete", false))
	if not visible:
		_timer.stop()
		_blocked_hint.hide()
		highlights.clear()
		return
	advance.visible = bool(lesson.get("canContinue", false))
	var key: String = str(lesson.get("hintKey", ""))
	hint.text = _text.t(key, lesson.get("params", {})) if not key.is_empty() else ""
	if changed_step:
		_timer.stop()
		_blocked_hint.hide()
	_refresh_geometry()

func _visibility_changed() -> void:
	set_process(visible)

func show_blocked(key: String = "") -> void:
	if not visible:
		return
	if key.is_empty():
		key = str(_lesson.get("blockedHint", ""))
	_blocked_hint.text = _text.t(key if not key.is_empty() else "tutorial.lesson.blocked")
	_blocked_hint.show()
	_timer.start()
	_refresh_geometry()

func _clear_blocked() -> void:
	_blocked_hint.hide()
	_refresh_geometry()

func _process(delta: float) -> void:
	_refresh_geometry()
	if not UiFactory.reduced_motion and not highlights.is_empty():
		_pulse = fmod(_pulse + delta * 4.0, TAU)
		surface.queue_redraw()

func _draw_highlights() -> void:
	var strength: float = 1.0 if UiFactory.reduced_motion else 0.78 + 0.22 * sin(_pulse)
	for rect: Rect2 in highlights:
		surface.draw_rect(rect, Color(0.04, 0.1, 0.12, 0.9), false, 5.0)
		surface.draw_rect(rect, Color(0.4, 0.95, 1.0, strength), false, 3.0)

func _refresh_geometry() -> void:
	if not visible or not _resolve.is_valid():
		return
	var targets: Array[Rect2] = []
	for id: String in _lesson.get("highlights", []):
		var resolved: Array[Rect2] = _resolve.call(id)
		targets.append_array(resolved)
	if highlights != targets:
		highlights = targets
		surface.queue_redraw()
	var blocked: Array[Rect2] = []
	for rect: Rect2 in targets:
		blocked.append(rect.grow(4))
	for control: Control in _obstructions:
		if control.is_visible_in_tree():
			blocked.append(control.get_global_transform_with_canvas() * Rect2(Vector2.ZERO, control.size))
	var viewport: Rect2 = Rect2(Vector2.ZERO, surface.size).grow(-GAP)
	var anchor: String = str(_lesson.get("hintAnchor", "auto"))
	var layout_inputs: Array = [viewport, blocked, anchor, hint.text, _blocked_hint.text if _blocked_hint.visible else "", advance.visible, hint.get_theme_font_size(&"font_size"), advance.get_combined_minimum_size()]
	if _last_layout == layout_inputs:
		return
	_last_layout = layout_inputs
	if hint.text.is_empty() and not advance.visible and not _blocked_hint.visible:
		bar.hide()
		return
	var placement: Rect2 = _place_bar(viewport, anchor, blocked)
	if placement.has_area():
		_bar_size = placement.size
		bar.show()
		bar.position = placement.position
		_resize_bar()
	else:
		bar.hide()

func _resize_bar() -> void:
	# Wrapped labels can release their previous minimum height only after the
	# row has received its new width from the container.
	bar.size = _bar_size

func _bar_extent(width: float) -> Vector2:
	var button: Vector2 = advance.get_combined_minimum_size() if advance.visible else Vector2.ZERO
	var margins: Vector2 = bar.get_theme_stylebox(&"panel").get_minimum_size()
	var label_width: float = maxf(1.0, width - margins.x - button.x - (_row.get_theme_constant(&"separation") if advance.visible else 0))
	var height: float = _label_height(hint, label_width)
	if _blocked_hint.visible:
		height += _instructions.get_theme_constant(&"separation") + _label_height(_blocked_hint, label_width)
	return Vector2(width, ceilf(maxf(height, button.y)) + margins.y)

func _label_height(label: Label, width: float) -> float:
	var font: Font = label.get_theme_font(&"font")
	var font_size: int = label.get_theme_font_size(&"font_size")
	var text_size: Vector2 = font.get_multiline_string_size(label.text, HORIZONTAL_ALIGNMENT_LEFT, width, font_size, -1, TextServer.BREAK_MANDATORY | TextServer.BREAK_WORD_BOUND | TextServer.BREAK_ADAPTIVE)
	# Label includes theme line spacing between wrapped lines.
	var lines: int = maxi(1, roundi(text_size.y / maxf(1.0, font.get_height(font_size))))
	return lines * label.get_line_height() + (lines - 1) * label.get_theme_constant(&"line_spacing")

func _place_bar(viewport: Rect2, preferred: String, blocked: Array[Rect2]) -> Rect2:
	var wide: Vector2 = _bar_extent(minf(1240.0, viewport.size.x * 0.78))
	var narrow: Vector2 = _bar_extent(minf(440.0, viewport.size.x * 0.27))
	var anchors: Array[String] = ["top", "bottom", "left", "right"]
	if preferred in anchors:
		anchors.erase(preferred)
		anchors.push_front(preferred)
	for anchor: String in anchors:
		var extent: Vector2 = wide if anchor in ["top", "bottom"] else narrow
		var origin: Vector2 = viewport.get_center() - extent * 0.5
		match anchor:
			"top": origin.y = viewport.position.y
			"bottom": origin.y = viewport.end.y - extent.y
			"left": origin.x = viewport.position.x
			"right": origin.x = viewport.end.x - extent.x
		var candidate: Rect2 = Rect2(origin.round(), extent)
		if _fits(candidate, viewport, blocked):
			return candidate
	# A centred edge can be occupied even when a corner or a gap between
	# highlighted cards has room. Test the edges of each occupied rectangle.
	var xs: Array[float] = [viewport.position.x, viewport.end.x - narrow.x]
	var ys: Array[float] = [viewport.position.y, viewport.end.y - narrow.y]
	for rect: Rect2 in blocked:
		xs.append(rect.end.x + GAP)
		xs.append(rect.position.x - GAP - narrow.x)
		ys.append(rect.end.y + GAP)
		ys.append(rect.position.y - GAP - narrow.y)
	for y: float in ys:
		for x: float in xs:
			var candidate: Rect2 = Rect2(Vector2(x, y), narrow)
			if _fits(candidate, viewport, blocked):
				return candidate
	return Rect2()

func _fits(candidate: Rect2, viewport: Rect2, blocked: Array[Rect2]) -> bool:
	if not viewport.encloses(candidate):
		return false
	for rect: Rect2 in blocked:
		if rect.intersects(candidate):
			return false
	return true
