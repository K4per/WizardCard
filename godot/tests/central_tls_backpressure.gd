extends SceneTree

func _initialize() -> void:
	_run.call_deferred()

func _run() -> void:
	var args: PackedStringArray = OS.get_cmdline_user_args()
	var tcp: StreamPeerTCP = StreamPeerTCP.new()
	tcp.connect_to_host("127.0.0.1", int(args[0]))
	while tcp.get_status() == StreamPeerTCP.STATUS_CONNECTING:
		tcp.poll()
		await process_frame
	var ca: X509Certificate = X509Certificate.new()
	ca.load(args[1])
	var tls: StreamPeerTLS = StreamPeerTLS.new()
	tls.connect_to_stream(tcp, "localhost", TLSOptions.client(ca, "localhost"))
	while tls.get_status() == StreamPeerTLS.STATUS_HANDSHAKING:
		tls.poll()
		await process_frame
	if tls.get_status() != StreamPeerTLS.STATUS_CONNECTED:
		quit(2)
		return
	var transport: CentralTlsTransport = CentralTlsTransport.new()
	transport._tcp = tcp
	transport._tls = tls
	var packet: PackedByteArray = PackedByteArray()
	packet.resize(1024 * 1024)
	packet.fill(42)
	for index: int in range(4):
		transport._outgoing.append(packet)
	transport._outgoing_bytes = packet.size() * 4
	var deadline: int = Time.get_ticks_msec() + 1500
	var frames: int = 0
	while Time.get_ticks_msec() < deadline:
		var before: int = Time.get_ticks_msec()
		transport._flush_tls()
		if Time.get_ticks_msec() - before > 100:
			push_error("TLS send blocked the main thread")
			quit(1)
			return
		frames += 1
		await process_frame
	var bounded: bool = frames > 10 and transport._outgoing_bytes > 0
	transport.stop()
	print("CENTRAL_TLS_BACKPRESSURE: frames=", frames, " queued bytes retained=", bounded)
	quit(0 if bounded else 1)
