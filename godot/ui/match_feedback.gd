class_name MatchFeedback
extends Node

var service: FrontendService
var _last_played: Dictionary[String, int] = {}
var _streams: Dictionary[String, AudioStream] = {}

func present(data: Dictionary) -> void:
	if service == null:
		return
	var master: float = float(service.settings.get("masterVolume", 80)) / 100.0
	var sound: float = float(service.settings.get("soundVolume", 80)) / 100.0
	if master <= 0 or sound <= 0:
		return
	for cue: Dictionary in data.get("sounds", []):
		var file_name: String = str(cue.get("file", ""))
		if file_name.get_file() != file_name or not file_name.ends_with(".mp3"):
			continue
		var now: int = Time.get_ticks_msec()
		var cooldown: int = int(float(cue.get("cooldownSeconds", 0)) * 1000)
		if now - _last_played.get(file_name, -100000) < cooldown:
			continue
		if not _streams.has(file_name):
			var path: String = service.assets_directory.path_join("audio").path_join(file_name)
			if not FileAccess.file_exists(path):
				continue
			var stream: AudioStreamMP3 = AudioStreamMP3.load_from_file(path)
			if stream == null:
				continue
			_streams[file_name] = stream
		_last_played[file_name] = now
		var player: AudioStreamPlayer = AudioStreamPlayer.new()
		add_child(player)
		player.stream = _streams[file_name]
		player.volume_db = linear_to_db(maxf(0.001, master * sound * float(cue.get("gain", 1))))
		player.finished.connect(player.queue_free)
		player.play()
