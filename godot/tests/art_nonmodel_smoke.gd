extends SceneTree

var assertions: int = 0

func check(condition: bool, message: String) -> void:
	assertions += 1
	if not condition:
		push_error(message)
		quit(2)
		assert(condition, message)

func _initialize() -> void:
	run.call_deferred()

func run() -> void:
	var library: AnimationLibrary = load("res://art/nonmodel/a-gilded-v15/animations/card_and_match.tres") as AnimationLibrary
	check(library != null and library.get_animation_list().size() == 20, "ten actions with reduced counterparts")
	var gallery: WizardArtGallery = (load("res://scenes/art/nonmodel_gallery.tscn") as PackedScene).instantiate() as WizardArtGallery
	root.add_child(gallery)
	await process_frame
	gallery.timer.stop()
	gallery.clear()
	for name: String in WizardArtGallery.NAMES:
		var effect: WizardArtEffect3D = (load("res://art/nonmodel/a-gilded-v15/effects/" + name + ".tscn") as PackedScene).instantiate() as WizardArtEffect3D
		gallery.effects.add_child(effect)
		check(effect.material != null and effect.effect_texture != null, name + " loads shader/texture")
		effect.set_paused(true)
		var old_phase: float = effect._phase
		await process_frame
		check(is_instance_valid(effect) and is_equal_approx(effect._phase, old_phase), name + " pauses")
		effect.set_paused(false)
		effect.play(true)
		if effect.looping:
			check(not effect.visible, name + " disabled in reduced mode")
		else:
			await create_timer(0.2).timeout
			check(not is_instance_valid(effect), name + " reduced event automatically ends")
	gallery.clear()
	check(gallery.effects.get_child_count() == 0, "explicit cleanup removes all loops")
	gallery.queue_free()
	await process_frame
	print("ART_NONMODEL_SMOKE assertions=", assertions)
	quit(0)
