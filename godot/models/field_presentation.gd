class_name FieldPresentation
extends Resource

var own_formations: Array[FormationPresentation] = []
var opponent_formations: Array[FormationPresentation] = []
var hand: Array[CardPresentation] = []
var own_casting: Array[CardPresentation] = []
var opponent_casting: Array[CardPresentation] = []
var own_casting_slots: int = 0
var opponent_casting_slots: int = 0
var own_side_count: int = 0
var opponent_side_count: int = 0
var own_deck_count: int = 0
var opponent_deck_count: int = 0
var own_life: int = 40
var opponent_life: int = 40
var own_mana: int = 0
var opponent_mana: int = 0
var own_load: int = 0
var opponent_load: int = 0
var own_load_limit: int = 0
var opponent_load_limit: int = 0
var own_ash_count: int = 0
var opponent_ash_count: int = 0
var phase_label: String = ""
