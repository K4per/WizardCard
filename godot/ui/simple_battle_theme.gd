class_name SimpleBattleTheme
extends RefCounted

static func create() -> Theme:
	return preload("res://ui/simple_battle_theme.tres").duplicate(true) as Theme
