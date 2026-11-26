class_name CardTile
extends Button

signal card_selected(card: CardPresentation)

var card: CardPresentation
@export var simple_style: bool = true

func _ready() -> void:
	custom_minimum_size = Vector2(176, 242)
	texture_filter = CanvasItem.TEXTURE_FILTER_LINEAR
	for state: StringName in [&"normal", &"hover", &"pressed", &"disabled", &"focus"]:
		add_theme_stylebox_override(state, StyleBoxEmpty.new())
	pressed.connect(_on_pressed)
	mouse_entered.connect(queue_redraw)
	mouse_exited.connect(queue_redraw)
	focus_entered.connect(queue_redraw)
	focus_exited.connect(queue_redraw)

func configure(value: CardPresentation) -> void:
	card = value
	tooltip_text = GameText.label("ui.card.hidden") if card.hidden else card.title
	queue_redraw()

func _draw() -> void:
	var bounds: Rect2 = Rect2(Vector2(4, 4), size - Vector2(8, 8))
	var highlighted: bool = is_hovered() or has_focus()
	if simple_style:
		DeckCardPainter.paint(self, card, bounds, highlighted)
	else:
		CardPainter.paint(self, card, bounds, "builder", highlighted)

func _on_pressed() -> void:
	card_selected.emit(card)
