class_name CardTile
extends Button

signal card_selected(card: CardPresentation)

var card: CardPresentation

func _ready() -> void:
	custom_minimum_size = Vector2(176, 242)
	texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	pressed.connect(_on_pressed)
	mouse_entered.connect(queue_redraw)
	mouse_exited.connect(queue_redraw)
	focus_entered.connect(queue_redraw)
	focus_exited.connect(queue_redraw)

func configure(value: CardPresentation) -> void:
	card = value
	tooltip_text = "盖伏卡" if card.hidden else card.title
	queue_redraw()

func _draw() -> void:
	CardPainter.paint(self, card, Rect2(Vector2(4, 4), size - Vector2(8, 8)), "builder", is_hovered() or has_focus())

func _on_pressed() -> void:
	card_selected.emit(card)
