class_name WizardWidgets
extends RefCounted

var skin: WizardSkin
var audio: WizardAudio

func _init(resources: WizardSkin, player: WizardAudio = null) -> void:
	skin = resources
	audio = player

func label(parent: Node, value: String, font_size: int = WizardSkin.BODY) -> Label:
	var node: Label = Label.new()
	node.text = value
	node.add_theme_font_size_override("font_size", font_size)
	parent.add_child(node)
	return node

func button(parent: Node, value: String, callback: Callable, icon: String = "", role: String = "") -> Button:
	var node: Button = Button.new()
	node.text = value
	node.theme_type_variation = role
	if role.is_empty():
		if value in ["投降", "删除", "确认离开"]:
			node.theme_type_variation = "DangerButton"
		elif value in ["开始对局", "保存草稿", "应用并保存", "再来一局"]:
			node.theme_type_variation = "PrimaryButton"
	node.custom_minimum_size.y = 36
	if not icon.is_empty():
		node.icon = skin.texture(icon)
		node.expand_icon = true
		node.add_theme_constant_override("icon_max_width", 22)
	node.pressed.connect(func() -> void:
		if audio != null:
			audio.cue("ui_click")
		callback.call())
	parent.add_child(node)
	return node

func column(parent: Node, expand: bool = false) -> VBoxContainer:
	var node: VBoxContainer = VBoxContainer.new()
	node.add_theme_constant_override("separation", WizardSkin.GAP)
	if expand:
		node.size_flags_vertical = Control.SIZE_EXPAND_FILL
	parent.add_child(node)
	return node

func row(parent: Node, expand: bool = false) -> HBoxContainer:
	var node: HBoxContainer = HBoxContainer.new()
	node.add_theme_constant_override("separation", WizardSkin.GAP)
	if expand:
		node.size_flags_vertical = Control.SIZE_EXPAND_FILL
	parent.add_child(node)
	return node

func panel(parent: Node, id: String = "tooltip", expand: bool = false) -> PanelContainer:
	var node: PanelContainer = PanelContainer.new()
	node.add_theme_stylebox_override("panel", skin.style(id))
	if expand:
		node.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		node.size_flags_vertical = Control.SIZE_EXPAND_FILL
	parent.add_child(node)
	return node

func scroll(parent: Node) -> ScrollContainer:
	var node: ScrollContainer = ScrollContainer.new()
	node.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	node.size_flags_vertical = Control.SIZE_EXPAND_FILL
	node.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	parent.add_child(node)
	return node

func icon_label(parent: Node, icon: String, text: String, font_size: int = WizardSkin.BODY) -> HBoxContainer:
	var node: HBoxContainer = row(parent)
	var image: TextureRect = TextureRect.new()
	image.texture = skin.texture(icon)
	image.custom_minimum_size = Vector2(22, 22)
	image.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	image.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	image.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	image.mouse_filter = Control.MOUSE_FILTER_IGNORE
	node.add_child(image)
	label(node, text, font_size)
	return node

func card(parent: Node, data: Dictionary, kind: String, dimensions: Vector2) -> WizardCardView:
	var node: WizardCardView = WizardCardView.new()
	node.custom_minimum_size = dimensions
	node.configure(skin, data, kind)
	parent.add_child(node)
	return node

func option(parent: Node, values: Array[String], selected: int = 0) -> OptionButton:
	var node: OptionButton = OptionButton.new()
	for value: String in values:
		node.add_item(value)
	node.select(selected)
	parent.add_child(node)
	return node

func spacer(parent: Node) -> Control:
	var node: Control = Control.new()
	node.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	node.size_flags_vertical = Control.SIZE_EXPAND_FILL
	parent.add_child(node)
	return node

func text(parent: Node, value: String, minimum: float = 90) -> RichTextLabel:
	var node: RichTextLabel = RichTextLabel.new()
	node.text = value
	node.custom_minimum_size.y = minimum
	node.size_flags_vertical = Control.SIZE_EXPAND_FILL
	node.add_theme_font_override("normal_font", skin.font)
	node.add_theme_font_size_override("normal_font_size", WizardSkin.BODY)
	parent.add_child(node)
	return node
