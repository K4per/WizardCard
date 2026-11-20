class_name CardFace
extends Control

var card: CardPresentation
@export var simple_style: bool = false

func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	texture_filter = CanvasItem.TEXTURE_FILTER_LINEAR
	resized.connect(queue_redraw)

func configure(value: CardPresentation) -> void:
	card = value
	queue_redraw()

func _draw() -> void:
	var bounds: Rect2 = Rect2(Vector2.ZERO, size)
	if simple_style:
		DeckCardPainter.paint(self, card, bounds)
	else:
		CardPainter.paint(self, card, bounds, "detail")
