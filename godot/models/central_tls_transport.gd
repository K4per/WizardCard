class_name CentralTlsTransport
extends RefCounted

const MAX_FRAME: int = 1024 * 1024
const MAX_RECEIVE: int = 2 * MAX_FRAME
const MAX_SEND: int = 4 * MAX_FRAME

var _service: FrontendService
var _resolve_id: int = -1
var _addresses: PackedStringArray = PackedStringArray()
var _address_index: int = 0
var _tcp: StreamPeerTCP
var _tls: StreamPeerTLS
var _hostname: String = ""
var _trusted_ca: String = ""
var _port: int = 0
var _phase: String = "idle"
var _phase_started_at: int = 0
var _retry_at: int = 0
var _receive: PackedByteArray = PackedByteArray()
var _outgoing: Array[PackedByteArray] = []
var _outgoing_bytes: int = 0
var _send_offset: int = 0
var _reported_open: bool = false

func pump(service: FrontendService) -> void:
	_service = service
	if not service.network_active:
		_close_transport()
		return
	var response: Dictionary = service.network_transport_state()
	if not bool(response.get("ok", false)):
		return
	var state: Dictionary = response.get("data", {})
	if not bool(state.get("active", false)) or not bool(state.get("external", false)):
		_close_transport()
		return
	if bool(state.get("ended", false)):
		_close_transport()
		return
	if _reported_open and not bool(state.get("open", false)):
		_close_transport()
		_retry_at = Time.get_ticks_msec() + 1500
	var hostname: String = str(state.get("hostname", ""))
	var port: int = int(state.get("port", 0))
	var trusted_ca: String = str(state.get("trustedCa", ""))
	if hostname.is_empty() or port < 1 or port > 65535:
		_fail("服务器地址或端口无效")
		return
	if hostname != _hostname or port != _port or trusted_ca != _trusted_ca:
		_close_transport()
		_hostname = hostname
		_port = port
		_trusted_ca = trusted_ca
	if _phase == "idle":
		if Time.get_ticks_msec() < _retry_at:
			return
		_start_resolve()
		return
	if _phase == "resolving":
		if _handshake_expired():
			_fail("服务器域名解析超时")
			return
		_poll_resolve()
		return
	if _phase == "tcp":
		if _handshake_expired():
			_connect_next_address()
			return
		_poll_tcp()
		return
	if _phase == "tls":
		if _handshake_expired():
			_fail("TLS 握手超时")
			return
		_poll_tls()
		return
	if _phase == "open":
		_tcp.poll()
		_tls.poll()
		if _tcp.get_status() != StreamPeerTCP.STATUS_CONNECTED or _tls.get_status() != StreamPeerTLS.STATUS_CONNECTED:
			_fail("与游戏服务器的连接已中断")
			return
		if not _reported_open:
			_reported_open = true
			_queue_response(service.network_transport_opened())
			if _phase != "open":
				return
		_queue_response(service.network_transport_drain())
		if _phase != "open":
			return
		_read_tls()
		if _phase == "open":
			_flush_tls()

func stop() -> void:
	_close_transport()
	_service = null

func _start_resolve() -> void:
	_resolve_id = IP.resolve_hostname_queue_item(_hostname, IP.TYPE_ANY)
	if _resolve_id < 0:
		_fail("服务器域名解析无法启动")
		return
	_phase = "resolving"
	_phase_started_at = Time.get_ticks_msec()

func _poll_resolve() -> void:
	var status: IP.ResolverStatus = IP.get_resolve_item_status(_resolve_id)
	if status == IP.RESOLVER_STATUS_WAITING:
		return
	if status != IP.RESOLVER_STATUS_DONE:
		_fail("服务器域名解析失败")
		return
	_addresses = IP.get_resolve_item_addresses(_resolve_id)
	_address_index = 0
	IP.erase_resolve_item(_resolve_id)
	_resolve_id = -1
	if _addresses.is_empty():
		_fail("服务器域名解析未返回地址")
		return
	_connect_next_address()

func _connect_next_address() -> void:
	if _tcp != null:
		_tcp.disconnect_from_host()
	while _address_index < _addresses.size():
		var address: String = _addresses[_address_index]
		_address_index += 1
		_tcp = StreamPeerTCP.new()
		if _tcp.connect_to_host(address, _port) == OK:
			_phase = "tcp"
			_phase_started_at = Time.get_ticks_msec()
			return
	_fail("服务器不可达或拒绝连接，请检查网络后重试")

func _poll_tcp() -> void:
	_tcp.poll()
	var status: StreamPeerTCP.Status = _tcp.get_status()
	if status == StreamPeerTCP.STATUS_CONNECTING:
		return
	if status != StreamPeerTCP.STATUS_CONNECTED:
		_connect_next_address()
		return
	var trusted: X509Certificate
	if not _trusted_ca.is_empty():
		trusted = X509Certificate.new()
		if trusted.load(_trusted_ca) != OK:
			_fail("无法加载配置的服务器 CA 证书")
			return
	var options: TLSOptions = TLSOptions.client(trusted, _hostname)
	_tls = StreamPeerTLS.new()
	var error: Error = _tls.connect_to_stream(_tcp, _hostname, options)
	if error != OK:
		_fail("无法启动 TLS 握手")
		return
	_phase = "tls"
	_phase_started_at = Time.get_ticks_msec()

func _poll_tls() -> void:
	_tcp.poll()
	_tls.poll()
	var status: StreamPeerTLS.Status = _tls.get_status()
	if status == StreamPeerTLS.STATUS_HANDSHAKING:
		return
	if status != StreamPeerTLS.STATUS_CONNECTED:
		_fail("证书校验失败：请检查信任证书、有效期和域名；可重试或恢复正式服配置")
		return
	_phase = "open"

func _read_tls() -> void:
	for _index: int in range(32):
		var available: int = _tls.get_available_bytes()
		if available <= 0:
			return
		var result: Array = _tls.get_data(mini(available, 65536))
		if int(result[0]) != OK:
			_fail("读取服务器数据失败")
			return
		_receive.append_array(result[1] as PackedByteArray)
		if _receive.size() > MAX_RECEIVE:
			_fail("服务器接收缓冲区超出上限")
			return
		if not _decode_frames():
			return

func _decode_frames() -> bool:
	while _receive.size() >= 4:
		var size: int = (int(_receive[0]) << 24) | (int(_receive[1]) << 16) | (int(_receive[2]) << 8) | int(_receive[3])
		if size < 1 or size > MAX_FRAME:
			_fail("服务器消息长度无效")
			return false
		if _receive.size() < size + 4:
			return true
		var payload: PackedByteArray = _receive.slice(4, size + 4)
		_receive = _receive.slice(size + 4)
		_queue_response(_service.network_transport_message(payload.get_string_from_utf8()))
		if _phase != "open":
			return false
	return true

func _queue_response(response: Dictionary) -> void:
	if not bool(response.get("ok", false)):
		var error: String = str(response.get("error", ""))
		if not error.is_empty():
			_fail(error)
		return
	for message: Dictionary in response.get("data", {}).get("messages", []):
		var payload: PackedByteArray = JSON.stringify(message).to_utf8_buffer()
		if payload.is_empty() or payload.size() > MAX_FRAME:
			_fail("待发送消息超出上限")
			return
		var frame: PackedByteArray = PackedByteArray([
			(payload.size() >> 24) & 255,
			(payload.size() >> 16) & 255,
			(payload.size() >> 8) & 255,
			payload.size() & 255,
		])
		frame.append_array(payload)
		if _outgoing_bytes + frame.size() > MAX_SEND:
			_fail("待发送数据超过 4 MiB")
			return
		_outgoing.append(frame)
		_outgoing_bytes += frame.size()

func _flush_tls() -> void:
	for _index: int in range(16):
		if _outgoing.is_empty():
			return
		var packet: PackedByteArray = _outgoing.front()
		var result: Array = _tls.put_partial_data(packet.slice(_send_offset, mini(_send_offset + 16384, packet.size())))
		var error: Error = int(result[0]) as Error
		if error != OK:
			_fail("发送服务器请求失败")
			return
		var sent: int = int(result[1])
		if sent == 0:
			return
		_send_offset += sent
		_outgoing_bytes -= sent
		if _send_offset == packet.size():
			_outgoing.pop_front()
			_send_offset = 0

func _fail(message: String) -> void:
	if _service != null:
		_service.network_transport_lost(message)
		_service.network_state["notice"] = message
	_close_transport()
	_retry_at = Time.get_ticks_msec() + 1500

func _handshake_expired() -> bool:
	return Time.get_ticks_msec() - _phase_started_at >= 10000

func _close_transport() -> void:
	if _resolve_id >= 0:
		IP.erase_resolve_item(_resolve_id)
		_resolve_id = -1
	if _tls != null:
		_tls.disconnect_from_stream()
	_tls = null
	if _tcp != null:
		_tcp.disconnect_from_host()
	_tcp = null
	_phase = "idle"
	_receive.clear()
	_outgoing.clear()
	_outgoing_bytes = 0
	_send_offset = 0
	_reported_open = false
	_phase_started_at = 0
	_addresses.clear()
	_address_index = 0
