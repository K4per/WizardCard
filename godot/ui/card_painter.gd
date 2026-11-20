class_name CardPainter
extends RefCounted

const PAPER: Color = Color("f6eed8")
const INK: Color = Color("08131f")
const PAPER_EDGE: Color = Color("c9a361")

static func fitted_rect(bounds: Rect2) -> Rect2:
	var width: float = minf(bounds.size.x, bounds.size.y * 0.75)
	var dimensions: Vector2 = Vector2(width, width / 0.75)
	return Rect2(bounds.get_center() - dimensions * 0.5, dimensions)

static func artwork_rect(card: CardPresentation, face: Rect2) -> Rect2:
	if card == null or card.hidden or card.face_down:
		return Rect2()
	return DeckCardPainter.artwork_rect(face)

static func paint_miniature(surface: CanvasItem, card: CardPresentation, face: Rect2, highlighted: bool = false) -> void:
	DeckCardPainter.paint(surface, card, face, highlighted, Color("efc97e"), true)

static func paint(surface: CanvasItem, card: CardPresentation, bounds: Rect2, _view: String = "field", highlighted: bool = false) -> void:
	DeckCardPainter.paint(surface, card, bounds, highlighted, Color("64cfe6"), true)

static func paint_art(surface: CanvasItem, card: CardPresentation, bounds: Rect2, highlighted: bool = false) -> void:
	if card == null or not bounds.has_area():
		return
	surface.draw_rect(bounds, DeckCardPainter.CARD_FILL)
	if card.hidden or card.face_down:
		DeckCardPainter.paint_covered(surface, bounds)
	else:
		DeckCardPainter.paint_artwork(surface, card, bounds.grow(-bounds.size.x * 0.085))
	surface.draw_rect(bounds.grow(-0.5), Color("64cfe6") if highlighted else PAPER_EDGE, false, 2.0 if highlighted else 1.0)
