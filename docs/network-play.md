# 真人联机与协议 4

当前产品使用 Godot 4.7.2 表现层和 C++17 权威规则。支持两台电脑同局域网或虚拟局域网直连；房主兼任权威端，不依赖中心服务器。双方必须同时更新，握手要求协议 4、规则 2.5.5、mechanicsRevision 9 及相同卡池内容哈希。

## 操作与恢复

创建房间后选择端口（默认 27861），客机输入房主 IPv4、IPv6 或域名及相同端口。双方选择合法卡组并准备；房主可调整先后手和空阶段自动通过，更改配置会清除准备状态。页面与设置只影响本机表现，应用根层持续驱动网络、计时和自动动作。

断线冻结决策时钟，2 秒心跳、6 秒超时、60 秒重连窗口。客机通过内存中的凭据重连；关闭程序后不保留凭据。重连获取稳定状态及事件 reset，清除旧查询，已发送而未确认的命令仍使用原序号重试。普通退出请求权威投降并保存记录；保存失败显示错误，保留权威会话，用户可重试或明确退出。

执行预算耗尽时，当前命令及其事件回滚，revision 增加并进入 technicalPaused；时钟保持冻结，普通取消暂停或重连不能恢复故障对局。权威端保留已接受命令及完整诊断，并在复盘旁保存 .diagnostic.json。保存失败时内存诊断保留，错误显示给用户。完整诊断和局中完整复盘不发送给客机。

## 会话接口

Session 提供 viewFor、clockView、queryActions、submit、eventsSince、revision、tick 和 technicalPaused。稳定查看者投影按 revision 缓存；时钟单独读取。queryActions 的 ActionIntent 包含可选来源卡牌、分步选择及分页参数，查询不更改状态；完成后仍通过 submit 权威验证。

HostSession 接收连接绑定的席位操作，不采用客机传入的 actor。GuestSession 只保存查看者投影、待确认命令、查询及重连凭据，不创建 GameEngine。Peer 组合会话、TCP、心跳与持久化。客机 submit 的 accepted=true,pending=true 仅表示传输已排队，需收到 Accepted 和 StateSnapshot 才能展示规则结果。

## 传输

大端 32 位长度前缀和 UTF-8 JSON 对象。单消息 1 MiB、接收缓冲 2 MiB、发送队列 4 MiB、嵌套深度 64。64 位标识使用十进制字符串。握手 Hello/Joined 校验协议、规则、卡池及内容哈希；程序版本只用于诊断。

- StateSnapshot：开始、重连或 revision 改变时发送完整稳定投影，不带完整动作组合和累计事件。包括可见卡牌、待决策摘要、操作摘要及技术暂停标记。
- ClockUpdate：至多每 250 ms 发送必要时钟数据；不会触发稳定状态重建。
- ActionQuery/ActionOptions：按需查询，requestId 与命令 sequence 独立。响应绑定会话、generation、revision、查看者和待决策 ID，旧响应直接丢弃。
- EventBatch：使用查看者独立 epoch/base/next 游标；私密事件不推进另一位玩家游标。游标失效返回 reset 和最近 512 条可见事件，重复包不重复呈现。
- Command/Accepted/Rejected：命令包含会话、sequence、requestId、revision、待决策 ID。保留最近 256 个结果，重复请求返回原结果，更旧或乱序请求拒绝。

其余消息为 Room/Deck/Ready、Heartbeat、Resume/Resumed、Replay、ReplayBegin/ReplayChunk/ReplayComplete/ReplayReceived、End/Error。正常终局后完整复盘每块 64 KiB、总上限 32 MiB；客机重放验证通过才保存并确认。权威房主持有完整状态，当前直连模式不提供针对恶意房主的反作弊或互联网身份认证。

## 复盘格式 5

新记录显式保存 format=5、contentSchema=1、commandSchema=1、digestSchema=2、mechanicsRevision=9、规则和卡池版本、内容哈希、初始配置、种子、接受命令及逐步摘要。摘要不包含会话历史事件，使用带长度和类型标记的 `wizardcard-state-v6` 完整运行状态编码；format 4 仅通过专用旧摘要适配器验证。

仅保留 programVersion=2.5.5-hotfix.1、rulesVersion=2.5.5、同内容哈希的格式 4 导入；通过兼容摘要逐步验证，并使用当前规则执行器。更早版本、未知 schema 或内容不匹配明确拒绝。内部能力编译不改变原始卡池内容或哈希。

## 验证

```powershell
ctest --test-dir build/headless -C Release --output-on-failure
python tools/network_smoke.py --exe build/headless/Release/wizard_network_probe.exe --output build/network-probe-verified --scenario all
```

网络探针运行两个独立进程，覆盖正常终局、投降和重连，并核对最终复盘摘要。自动化覆盖不能代替两台实体电脑和实际虚拟局域网验证。Godot 图形与选择交互的验证命令见对应工具链说明。
