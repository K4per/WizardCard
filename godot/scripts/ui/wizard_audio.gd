class_name WizardAudio
extends Node

const MAX_VOICES: int = 6
var directory: String = ""
var entries: Dictionary = {}
var streams: Dictionary = {}
var last_played: Dictionary = {}
var voices: Array[AudioStreamPlayer] = []
var volume: float = 0.64
var errors: PackedStringArray = []

func initialize(assets: String) -> void:
	directory = assets.path_join("audio")
	var manifest: Dictionary = WizardSkin.read_json(directory.path_join("manifest.json"))
	for raw: Variant in manifest.get("events", []) as Array:
		var entry: Dictionary = raw as Dictionary
		entries[str(entry["event"])] = {"file": entry["file"], "gain": entry["runtimeGain"], "priority": entry["priority"], "cooldownSeconds": entry["cooldownSeconds"]}
		var filename: String = str(entry["file"])
		var stream: AudioStreamMP3 = AudioStreamMP3.new()
		stream.data = FileAccess.get_file_as_bytes(directory.path_join(filename))
		if stream.get_length() > 0:
			streams[filename] = stream
		else:
			errors.append("音效未加载：" + filename)

func apply_settings(settings: Dictionary) -> void:
	volume = float(settings["masterVolume"]) * float(settings["soundVolume"]) / 10000.0
	if volume <= 0:
		stop()

func cue(event: String) -> void:
	if entries.has(event):
		play(entries[event] as Dictionary)

func play(data: Dictionary) -> void:
	var filename: String = str(data["file"])
	if volume <= 0 or not streams.has(filename):
		return
	var now: int = Time.get_ticks_msec()
	if last_played.has(filename) and now - int(last_played[filename]) < float(data.get("cooldownSeconds", 0.09)) * 1000:
		return
	var priority: int = int(data.get("priority", 0))
	if priority == 3:
		stop()
	if voices.size() >= MAX_VOICES:
		var weakest: AudioStreamPlayer = voices[0]
		for voice: AudioStreamPlayer in voices:
			if int(voice.get_meta("priority")) < int(weakest.get_meta("priority")):
				weakest = voice
		if priority < int(weakest.get_meta("priority")):
			return
		_retire(weakest)
	var player: AudioStreamPlayer = AudioStreamPlayer.new()
	player.stream = streams[filename] as AudioStream
	player.volume_db = linear_to_db(volume * float(data.get("gain", 0.8)))
	player.set_meta("priority", priority)
	add_child(player)
	voices.append(player)
	player.finished.connect(_retire.bind(player))
	last_played[filename] = now
	player.play()

func _retire(player: AudioStreamPlayer) -> void:
	voices.erase(player)
	player.stop()
	player.stream = null
	player.queue_free()

func stop() -> void:
	for player: AudioStreamPlayer in voices.duplicate():
		_retire(player)

func _exit_tree() -> void:
	stop()
	streams.clear()
