class_name DeckTheme
extends RefCounted

const BACKGROUND: Color = Color("14202b")
const PANEL: Color = Color("182631")
const CONTROL: Color = Color("1c2d39")
const HOVER: Color = Color("263c49")
const TEXT: Color = Color("e4edf0")
const MUTED: Color = Color("b7c6ce")
const BORDER: Color = Color("4a6370")
const ACCENT: Color = Color("8bc8db")

static func box(fill: Color = PANEL, border: Color = BORDER, padding: int = 10) -> StyleBoxFlat:
	var style: StyleBoxFlat = StyleBoxFlat.new()
	style.bg_color = fill
	style.border_color = border
	style.set_border_width_all(1)
	style.set_corner_radius_all(3)
	style.set_content_margin_all(padding)
	return style

static func create() -> Theme:
	var result: Theme = Theme.new()
	result.default_font = Typography.BODY
	result.default_font_size = 15
	for kind: StringName in [&"Label", &"Button", &"OptionButton", &"LineEdit", &"CheckBox", &"CheckButton", &"PopupMenu", &"TabBar", &"TooltipLabel"]:
		result.set_color(&"font_color", kind, TEXT)
		result.set_color(&"font_hover_color", kind, TEXT)
		result.set_color(&"font_pressed_color", kind, TEXT)
		result.set_color(&"font_disabled_color", kind, Color("71838e"))
		result.set_color(&"font_focus_color", kind, TEXT)
		result.set_color(&"font_shadow_color", kind, Color.TRANSPARENT)
		result.set_constant(&"shadow_offset_x", kind, 0)
		result.set_constant(&"shadow_offset_y", kind, 0)
	result.set_color(&"default_color", &"RichTextLabel", TEXT)
	result.set_font(&"bold_font", &"RichTextLabel", Typography.HEADING)
	result.set_stylebox(&"panel", &"TooltipPanel", box(PANEL, ACCENT, 8))
	result.set_color(&"font_color", &"LineEdit", TEXT)
	result.set_color(&"font_placeholder_color", &"LineEdit", MUTED)
	result.set_color(&"caret_color", &"LineEdit", ACCENT)
	result.set_color(&"selection_color", &"LineEdit", Color("345563"))
	result.set_stylebox(&"panel", &"Panel", box())
	result.set_stylebox(&"panel", &"PanelContainer", box())
	result.set_stylebox(&"panel", &"PopupMenu", box())
	result.set_stylebox(&"hover", &"PopupMenu", box(HOVER, ACCENT, 6))
	for kind: StringName in [&"Button", &"OptionButton"]:
		result.set_stylebox(&"normal", kind, box(CONTROL, BORDER, 8))
		result.set_stylebox(&"hover", kind, box(HOVER, ACCENT, 8))
		result.set_stylebox(&"pressed", kind, box(CONTROL, ACCENT, 8))
		result.set_stylebox(&"disabled", kind, box(CONTROL, BORDER, 8))
		var focus: StyleBoxFlat = box(Color.TRANSPARENT, ACCENT, 8)
		focus.set_border_width_all(2)
		result.set_stylebox(&"focus", kind, focus)
		result.set_font_size(&"font_size", kind, 14)
	result.set_stylebox(&"normal", &"LineEdit", box(CONTROL, BORDER, 8))
	result.set_stylebox(&"focus", &"LineEdit", box(CONTROL, ACCENT, 8))
	result.set_stylebox(&"read_only", &"LineEdit", box(CONTROL, BORDER, 8))
	result.set_stylebox(&"normal", &"RichTextLabel", StyleBoxEmpty.new())
	result.set_stylebox(&"scroll", &"VScrollBar", box(BACKGROUND, BACKGROUND, 0))
	result.set_stylebox(&"grabber", &"VScrollBar", box(BORDER, BORDER, 0))
	result.set_stylebox(&"grabber_highlight", &"VScrollBar", box(MUTED, MUTED, 0))
	result.set_stylebox(&"grabber_pressed", &"VScrollBar", box(ACCENT, ACCENT, 0))
	result.set_constant(&"separation", &"HBoxContainer", 8)
	result.set_constant(&"separation", &"VBoxContainer", 8)
	return result

static func field(control: Control) -> void:
	if control is OptionButton:
		(control as OptionButton).fit_to_longest_item = false
		(control as OptionButton).clip_text = true
	control.add_theme_font_size_override(&"font_size", 14)

static func button(control: Button) -> void:
	control.custom_minimum_size.y = 34
	control.add_theme_font_size_override(&"font_size", 14)
	UiFactory.bind_button(control)
