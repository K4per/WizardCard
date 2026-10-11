class_name MatchFeedback
extends Node

var service: FrontendService
var _last_played: Dictionary[String, int] = {}
var _streams: Dictionary[String, AudioStream] = {}
var _players: Array[AudioStreamPlayer] = []

func _ready() -> void:
	for _index: int in range(8):
		var player: AudioStreamPlayer = AudioStreamPlayer.new()
		add_child(player)
		_players.append(player)

func play_ui(file_name: String, gain: float = 0.45) -> void:
	_play_file(file_name, gain, 0.09)

func present(data: Dictionary) -> void:
	for cue: Dictionary in data.get("sounds", []):
		_play_file(
			str(cue.get("file", "")),
			float(cue.get("gain", 1)),
			float(cue.get("cooldownSeconds", 0)))

func _play_file(file_name: String, gain: float, cooldown_seconds: float) -> void:
	if service == null or file_name.get_file() != file_name or not file_name.ends_with(".mp3"):
		return
	var master: float = float(service.settings.get("masterVolume", 80)) / 100.0
	var sound: float = float(service.settings.get("soundVolume", 80)) / 100.0
	if master <= 0 or sound <= 0:
		return
	var now: int = Time.get_ticks_msec()
	var cooldown: int = int(cooldown_seconds * 1000)
	if now - _last_played.get(file_name, -100000) < cooldown:
		return
	if not _streams.has(file_name):
		var path: String = service.assets_directory.path_join("audio").path_join(file_name)
		if not FileAccess.file_exists(path):
			return
		var stream: AudioStreamMP3 = AudioStreamMP3.load_from_file(path)
		if stream == null:
			return
		_streams[file_name] = stream
	var player: AudioStreamPlayer
	for candidate: AudioStreamPlayer in _players:
		if not candidate.playing:
			player = candidate
			break
	if player == null:
		return
	_last_played[file_name] = now
	player.stream = _streams[file_name]
	player.volume_db = linear_to_db(maxf(0.001, master * sound * gain))
	player.play()
