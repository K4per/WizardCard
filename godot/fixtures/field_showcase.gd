class_name FieldShowcase
extends RefCounted

static func create(library: Array[CardPresentation]) -> FieldPresentation:
	var field: FieldPresentation = FieldPresentation.new()
	if library.is_empty():
		return field
	var by_id: Dictionary[String, CardPresentation] = {}
	for card: CardPresentation in library:
		by_id[card.definition_id] = card
	var own: FormationPresentation = FormationPresentation.new()
	own.card = _copy(by_id.get("balance", library[0]), "formation-own")
	own.ring_count = 4
	own.occupants[0] = _copy(by_id.get("fireball", library[0]), "own-spell")
	own.occupants[0].status = "解析中"
	own.occupants[2] = CardPresentation.anonymous("own-covered")
	field.own_formations.append(own)
	var opponent: FormationPresentation = FormationPresentation.new()
	opponent.card = _copy(by_id.get("balance", library[0]), "formation-opponent")
	opponent.ring_count = 6
	opponent.occupants[1] = CardPresentation.anonymous("opponent-covered")
	field.opponent_formations.append(opponent)
	for id: String in ["fireball", "spark", "mend", "ring", "unravel"]:
		field.hand.append(_copy(by_id.get(id, library[0]), "hand-" + id))
	field.own_casting_slots = 4
	field.opponent_casting_slots = 6
	field.own_side_count = 9
	field.opponent_side_count = 9
	field.own_deck_count = 35
	field.opponent_deck_count = 35
	return field

static func _copy(card: CardPresentation, id: String) -> CardPresentation:
	var copy: CardPresentation = card.duplicate() as CardPresentation
	copy.instance_id = id
	return copy
