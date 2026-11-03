extends SceneTree

var _failures: int = 0

func _initialize() -> void:
	_run.call_deferred()

func _expect(condition: bool, message: String) -> void:
	if not condition:
		_failures += 1
		push_error("CENTRAL_ROOMS_UI: " + message)

func _run() -> void:
	var page: Control = load("res://pages/rooms_page.tscn").instantiate() as Control
	root.add_child(page)
	await process_frame
	_expect(page.get_node_or_null("%Address") is LineEdit, "server hostname input is present")
	_expect(page.get_node_or_null("%Port") is SpinBox, "TLS port input is present")
	_expect(page.get_node_or_null("%RoomCode") is LineEdit, "room code input is present")
	_expect(page.get_node_or_null("%TrustedCa") is LineEdit, "trusted CA path input is present")
	page.queue_free()
	await process_frame
	var match_page: MatchPage = load("res://pages/match_page.tscn").instantiate() as MatchPage
	match_page.service = FrontendService.new()
	match_page.service.match_mode = "network"
	root.add_child(match_page)
	await process_frame
	match_page.network_updated({
		"exists": true, "started": true, "blocked": false, "connected": true,
		"notice": "", "room": {"phase": "aborted", "aborted": true, "abortReason": "server_shutdown"}
	})
	_expect(match_page._notice.text.contains("无胜者"), "aborted matches explicitly show that neither player won")
	_expect(not match_page._reconnect.visible, "aborted matches do not offer reconnect")
	match_page.network_updated({
		"exists": true, "started": true, "blocked": true, "connected": true,
		"notice": "", "replayDelivered": false, "room": {"phase": "completed"}
	})
	_expect(match_page._notice.text.contains("复盘"), "completed matches show replay transfer status")
	match_page.network_updated({
		"exists": true, "started": true, "blocked": true, "connected": true,
		"room": {"phase": "playing", "storageBlocked": true}
	})
	_expect(match_page._notice.text.contains("存储"), "storage pauses explain that journal recovery is pending")
	match_page.network_updated({
		"exists": true, "started": true, "blocked": true, "connected": true,
		"room": {"phase": "playing", "technicalPaused": true}
	})
	_expect(match_page._notice.text.contains("技术暂停"), "rule-engine pauses remain distinct from storage pauses")
	match_page.network_updated({
		"exists": true, "started": true, "blocked": true, "connected": false, "host": true,
		"room": {"phase": "playing", "reconnecting": true, "reconnectRemainingMs": 54000}
	})
	_expect(match_page._notice.text.contains("54"), "disconnected clients show the server reconnect deadline")
	_expect(match_page._reconnect.visible, "either room seat can manually retry the connection")
	print("CENTRAL_ROOMS_UI: ", _failures, " failures")
	match_page.queue_free()
	await process_frame
	quit(0 if _failures == 0 else 1)
