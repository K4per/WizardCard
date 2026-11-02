class_name CardFace
extends Control

var card: CardPresentation
@export var simple_style: bool = false

func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	resized.connect(queue_redraw)

func configure(value: CardPresentation) -> void:
	card = value
	queue_redraw()

func _draw() -> void:
	if simple_style:
		# Popup trays and detail panels use the same illustrated compact face as
		# the deck editor, so selecting a pile card never loses its artwork.
		DeckCardPainter.paint(self, card, Rect2(Vector2.ZERO, size))
	else:
		CardPainter.paint(self, card, Rect2(Vector2.ZERO, size), "detail")
