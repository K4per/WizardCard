extends RefCounted

static func point(canvas: FieldCanvas, card: CardPresentation) -> Vector2:
	var bounds: Rect2 = canvas.card_bounds(card).intersection(Rect2(Vector2.ZERO, canvas.size))
	for y: int in range(1, 12):
		for x: int in range(1, 32):
			var local: Vector2 = bounds.position + bounds.size * Vector2(x / 32.0, y / 12.0)
			if canvas.card_at(local) == card:
				return canvas.get_global_transform_with_canvas() * local
	return Vector2(-1, -1)
