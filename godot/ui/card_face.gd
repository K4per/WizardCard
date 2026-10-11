class_name CardFace
extends Control

var card: CardPresentation

func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	resized.connect(queue_redraw)

func configure(value: CardPresentation) -> void:
	card = value
	queue_redraw()

func _draw() -> void:
	CardPainter.paint(self, card, Rect2(Vector2.ZERO, size), "detail")
