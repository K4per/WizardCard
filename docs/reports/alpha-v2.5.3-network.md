# Alpha v2.5.3 联机网络调查

日期：2026-10-23

## 域名解析与地址族

客机地址经系统 `getaddrinfo` 在后台解析，使用 `AF_UNSPEC` 同时取得 IPv4/IPv6 地址，避免 DNS 查询占用对局轮询线程。连接按系统返回顺序逐个尝试；解析和连接各有超时，失败提示区分域名解析失败、没有可用地址、服务器拒绝连接和连接超时。`localhost`、IPv4 字面量和 IPv6 字面量走同一套地址解析与连接流程。

房主优先创建 IPv4 映射的 IPv6 双栈监听 socket，让 IPv4 与 IPv6 客机连接同一房间；系统无双栈支持时回退至既有 IPv4 监听，保留 IPv4-only 环境下的建房能力。联机协议、规则会话、内容校验和回放格式均未改变。

自动化覆盖新增 `localhost` 多地址解析、`127.0.0.1` 直连、`::1` / `[::1]` 直连、保留域名 `.invalid` 失败（包括大小写和末尾点号）、无效 IPv6 文本的系统解析失败、动态保留端口上的连接拒绝，以及发送队列上限。`tools/network_smoke.py` 新增 `--address`，可用两进程探针复核，例如：

```powershell
python tools/network_smoke.py --exe build/windows/Release/wizard_network_probe.exe --address localhost --scenario surrender
python tools/network_smoke.py --exe build/windows/Release/wizard_network_probe.exe --address ::1 --scenario surrender --port 32200
```

SFML 地址输入已放开字母、点号、冒号、连字符和 IPv6 zone ID 使用的百分号，长度上限为 253，页面说明同步列出域名、IPv4 和 IPv6。Godot 使用普通文本地址框，将输入去除首尾空白后提交给同一传输层，能够输入域名及 IPv6。

### 本机 DNS 代理与失败用例

初轮 CTest 中，`wizardcard-does-not-exist.invalid` 未返回解析错误，且 TCP 报告已连接。通过独立的 Windows `System.Net.Dns.GetHostAddresses` 检查，系统将该保留域名解析成 `198.18.0.133`，另一个随机 `.invalid` 名称解析成 `198.18.0.191`；本机 `Meta` 网卡的 DNS 为 `198.18.0.2`。这符合本机 DNS 代理生成 fake-IP 的行为，原用例关于系统必定返回 NXDOMAIN 的假设不成立；TCP 已连接并不等于 WizardCard 协议握手已成功。

修复在解析器中按 [RFC 6761 第 6.4 节](https://www.rfc-editor.org/rfc/rfc6761#section-6.4) 在本地识别保留的 `.invalid` 域名并返回明确失败，避免为这一无效名称联系代理生成的地址；识别覆盖大小写、完整域名末尾点号和 `invalid` 自身。测试仍要求域名解析错误及未连接状态，增加 `not:a:valid:ipv6::g` 以覆盖真实系统解析失败路径；独立系统检查确认该无效 IPv6 文本返回“不知道这样的主机”。另外检查 `getsockopt(SO_ERROR)` 调用本身的返回值，避免状态查询失败时误把初始值 0 当作连接成功。

## “超过4MiB”提示

- 来源：`Tcp::send` 抛出的发送队列硬上限错误，不是调试日志或协议版本诊断。
- 触发：尚未写入 socket 的应用层帧字节数，加上待加入的完整帧字节数（含 4 字节长度前缀），超过 `maxSend = 4 * maxMessage = 4,194,304` 字节。该数值不包括操作系统 socket 发送缓冲区。
- 可能入口：单人操作提交、Peer 每帧同步/心跳，以及联机复盘分块。调用失败会显示错误；在 `Peer::tick` 内触发时会断开当前 socket 并进入既有重连路径，因此它确实代表发送受阻，不是单纯提示。
- 正常流量：单条协议消息最大 1 MiB；复盘使用 64 KiB 数据块，并在应用层队列低于 2 MiB 时才继续填充。房间状态与视图同步按状态变化/250 毫秒时钟节奏发送。按这些限制，正常命令、状态同步和复盘发送不会一次性填满 4 MiB 队列；仍持续超限意味着本机产出速度超过链路/对端消费速度或存在异常积压，强行放大上限会继续增加内存占用并推迟反馈。
- 可复现影响：测试先建立本机 TCP 连接但不轮询发送端，在内存队列中累积多条 600 KiB 消息，再确认下一条消息被 4 MiB 上限拒绝，且错误说明连接速度不足/消息积压及重试动作。
- 处理：保留有依据的 4 MiB 内存界限，错误改为玩家可理解的“待发送数据超过4MiB：连接速度不足或消息积压，请稍后重试”。未调大消息、接收缓冲或队列上限。

## 验证记录

已通过MSBuild及421项CTest，域名、IPv4/IPv6与发送队列限制的专项均通过。最终双进程探针让双方各自验证收到的权威复盘，并比较终局结果和摘要：

| 地址 | 场景 | 结果 | 证据 |
|---|---|---|---|
| `localhost` | 正常完整对局 | 723条命令；`37941c9ac176d46e` | [DNS报告](alpha-v2.5.3-evidence/network-dns.json) |
| `localhost` | 投降终局与复盘 | 5条命令；`5e7ed7acb4405ac2` | 同上 |
| `localhost` | 断线重连后完整终局 | 813条命令；`97388c7b1fe16239`，客机`reconnected=true` | 同上 |
| `127.0.0.1` | 投降终局与复盘 | 13条命令；`94e63f9261cf535c` | [IPv4报告](alpha-v2.5.3-evidence/network-ipv4.json) |
| `::1` | 投降终局与复盘 | 14条命令；`a206d8877df5f538` | [IPv6报告](alpha-v2.5.3-evidence/network-ipv6.json) |
| `[::1]` | 投降终局与复盘 | 11条命令；`f7070a46229fe83b` | [方括号IPv6报告](alpha-v2.5.3-evidence/network-bracket-verified.json) |

初次正常整局探针在持续推进800余条操作时触及180秒验证watchdog，双方尚未终局。等待上限已改为420秒，仅作用于验证进程；解析与连接超时、协议消息上限及4MiB队列上限未放大。延长后正常整局和重连场景均完成，未出现4MiB错误；最终摘要一致。CTest内存联机完整对局同样通过。

Godot双端前端烟测通过，覆盖房间与对局入口。实体双机、外部真实DNS服务及低速/丢包链路尚未在本轮验证；解析超时错误路径已实现，未通过外部DNS故障注入实测。
