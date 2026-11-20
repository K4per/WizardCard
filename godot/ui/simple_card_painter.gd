class_name SimpleCardPainter
extends RefCounted

static func paint(surface: CanvasItem, card: CardPresentation, rect: Rect2, highlighted: bool = false) -> void:
	DeckCardPainter.paint(surface, card, rect, highlighted)
