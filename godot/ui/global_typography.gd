extends Node

const BASE_THEME: StringName = &"typography_base_theme"
const APPLIED_THEME: StringName = &"typography_applied_theme"
const BASE_SIZES: StringName = &"typography_base_sizes"
const APPLIED_SIZES: StringName = &"typography_applied_sizes"

var _applying: bool = false
var _scaled_themes: Dictionary[int, Theme] = {}
var _theme_sources: Dictionary[int, WeakRef] = {}

func _ready() -> void:
	get_tree().node_added.connect(_node_added)
	refresh.call_deferred()

func set_scale(percent: int) -> void:
	Typography.scale_percent = percent if percent in Typography.SCALES else Typography.DEFAULT_SCALE
	_scaled_themes.clear()
	_theme_sources.clear()
	refresh()

func refresh() -> void:
	if not is_inside_tree():
		return
	for identity: int in _theme_sources.keys():
		if _theme_sources[identity].get_ref() == null:
			_scaled_themes.erase(identity)
			_theme_sources.erase(identity)
	_apply_tree(get_tree().root)

func _node_added(node: Node) -> void:
	# Builders set local theme sizes after add_child(), so capture after ready.
	_watch.call_deferred(node.get_instance_id())

func _watch(instance_id: int) -> void:
	var node: Node = instance_from_id(instance_id) as Node
	if node == null or not node.is_inside_tree():
		return
	if node is Control:
		var control: Control = node as Control
		var callback: Callable = _theme_changed.bind(control)
		if not control.theme_changed.is_connected(callback):
			control.theme_changed.connect(callback)
	_apply_node(node)

func _theme_changed(control: Control) -> void:
	if not _applying:
		_apply_node(control)

func _apply_tree(node: Node) -> void:
	_apply_node(node)
	for child: Node in node.get_children():
		_apply_tree(child)

func _apply_node(node: Node) -> void:
	if node is CanvasItem:
		(node as CanvasItem).queue_redraw()
	if not node is Control:
		return
	var control: Control = node as Control
	_applying = true
	if control.theme != null:
		if not control.has_meta(APPLIED_THEME) or control.theme != control.get_meta(APPLIED_THEME):
			control.set_meta(BASE_THEME, control.theme)
		var source: Theme = control.get_meta(BASE_THEME) as Theme
		var identity: int = source.get_instance_id()
		if not _scaled_themes.has(identity):
			_scaled_themes[identity] = _scaled_theme(source)
			_theme_sources[identity] = weakref(source)
		control.set_meta(APPLIED_THEME, _scaled_themes[identity])
		control.theme = _scaled_themes[identity]
	var base: Dictionary = control.get_meta(BASE_SIZES, {})
	var applied: Dictionary = control.get_meta(APPLIED_SIZES, {})
	var names: Array[StringName] = [&"font_size"]
	if control is RichTextLabel:
		names = [&"normal_font_size", &"bold_font_size", &"italics_font_size", &"bold_italics_font_size", &"mono_font_size"]
	for item: StringName in names:
		if not control.has_theme_font_size_override(item):
			base.erase(item)
			applied.erase(item)
			continue
		var current: int = control.get_theme_font_size(item)
		if not applied.has(item) or current != int(applied[item]):
			base[item] = current
		var scaled: int = Typography.scaled_size(int(base[item]))
		applied[item] = scaled
		if current != scaled:
			control.add_theme_font_size_override(item, scaled)
	control.set_meta(BASE_SIZES, base)
	control.set_meta(APPLIED_SIZES, applied)
	_applying = false

func _scaled_theme(source: Theme) -> Theme:
	var result: Theme = source.duplicate() as Theme
	result.default_font_size = Typography.scaled_size(source.default_font_size) if source.default_font_size > 0 else Typography.scaled_size(16)
	for kind: StringName in source.get_type_list():
		for item: StringName in source.get_font_size_list(kind):
			result.set_font_size(item, kind, Typography.scaled_size(source.get_font_size(item, kind)))
	return result
