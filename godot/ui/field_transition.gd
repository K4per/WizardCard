class_name FieldTransition
extends RefCounted

# Decorative projection differences only. These never change the field model,
# card poses, hit regions, or the order in which commands reach the native core.
var changed: Dictionary[String, Color] = {}
var departures: Array[Rect2] = []
var life: Array[Color] = [Color.TRANSPARENT, Color.TRANSPARENT]

func clear() -> void:
	changed.clear()
	departures.clear()
	life.assign([Color.TRANSPARENT, Color.TRANSPARENT])

func prepare(previous: FieldPresentation, next: FieldPresentation, old_hits: Array[Dictionary]) -> bool:
	clear()
	if previous.viewer != next.viewer:
		return false
	var before: Dictionary[String, String] = _signatures(previous)
	var after: Dictionary[String, String] = _signatures(next)
	for id: String in after:
		if not before.has(id):
			changed[id] = DeckTheme.ACCENT
		elif before[id] != after[id]:
			changed[id] = Color("e3c58b")
	for hit: Dictionary in old_hits:
		var card: CardPresentation = hit.get("card") as CardPresentation
		if card != null and not after.has(card.instance_id):
			departures.append(hit["rect"])
	var old_life: Array[int] = [previous.own_life, previous.opponent_life]
	var new_life: Array[int] = [next.own_life, next.opponent_life]
	for index: int in range(2):
		if old_life[index] != new_life[index]:
			life[index] = Color("e89191") if new_life[index] < old_life[index] else Color("92d3a6")
	return not changed.is_empty() or not departures.is_empty() or life[0].a > 0.0 or life[1].a > 0.0

func paint(surface: CanvasItem, hits: Array[Dictionary], own_region: Rect2, opponent_region: Rect2, strength: float) -> void:
	if strength <= 0.0:
		return
	for hit: Dictionary in hits:
		var card: CardPresentation = hit.get("card") as CardPresentation
		if card != null and changed.has(card.instance_id):
			surface.draw_rect(hit["rect"], Color(changed[card.instance_id], strength * 0.65), false, 3.0)
	for rect: Rect2 in departures:
		surface.draw_rect(rect, Color(DeckTheme.MUTED, strength * 0.3), false, 2.0)
	var regions: Array[Rect2] = [own_region, opponent_region]
	for index: int in range(2):
		if life[index].a > 0.0:
			surface.draw_rect(regions[index].grow(-2), Color(life[index], strength * 0.7), false, 4.0)

func _signatures(value: FieldPresentation) -> Dictionary[String, String]:
	var result: Dictionary[String, String] = {}
	_add_cards(result, value.hand, "hand")
	_add_cards(result, value.own_casting, "own-casting")
	_add_cards(result, value.opponent_casting, "opponent-casting")
	for formations: Array[FormationPresentation] in [value.own_formations, value.opponent_formations]:
		for formation: FormationPresentation in formations:
			_add_cards(result, [formation.card], "formation")
			for ring: int in formation.occupants:
				_add_cards(result, [formation.occupants[ring]], "analysis-%s-%d" % [formation.card.instance_id, ring])
	for host: String in value.attachments:
		for card: CardPresentation in value.attachments[host]:
			_add_cards(result, [card], "attachment-" + host)
	return result

func _add_cards(result: Dictionary[String, String], cards: Array[CardPresentation], zone: String) -> void:
	for card: CardPresentation in cards:
		if card != null and not card.instance_id.is_empty():
			result[card.instance_id] = "%s|%s|%s|%s|%s|%d" % [zone, card.face_down, card.status, card.primary_value, card.secondary_value, card.storm_points]
