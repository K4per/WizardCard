# 真人联机（1.1.0-alpha 候选版）

首版支持两台 Windows 电脑同局域网或虚拟局域网直连。房主兼任权威主机，没有中心服务器。双方使用相同协议、规则、卡池与内容哈希；程序版本只用于诊断。

## 操作

1. 两人进入主菜单“真人联机”。房主确认端口（默认27861），选择“创建房间”。
2. 客机输入房主网卡的IPv4地址和相同端口，选择“加入房间”。127.0.0.1仅用于同机测试；跨公网需先建立虚拟局域网。
3. 各自选择合法卡组。房主选择先后手和空阶段自动通过；更改配置会清除双方准备状态。
4. 双方点击“准备对战”。各设备固定本方视角，卡牌点选、拖放、成本选择和确认沿用本地对局。
5. 设置仅影响本机操作与表现，后台仍收包；联机自动通过由房间统一控制。动画完成与否不会影响规则提交或连接心跳。
6. 打开设置可投降或返回主菜单。普通退出先请求权威投降、保存记录；保存失败可重试或选择“明确退出”。房主退出后不会迁移主机。

允许游戏通过 Windows 防火墙的专用网络；房主地址可通过系统网络设置查看。端口占用时换一个端口，双方保持一致。不要把虚拟局域网的建立与管理视为游戏功能。

断线冻结规则与决策，2秒心跳、6秒超时、60秒重连窗口。客机自动使用内存中的凭据重连；关闭程序后不保留凭据，不能恢复中断对局。重连恢复当前裁剪快照并清除旧选择、跳过历史动画。重连超时或房主关闭后可返回菜单。

房主按接受命令保存复盘；客机在正常终局或投降后自动获取完整记录。局中客机保存按钮会明确提示不可获取完整复盘。断线中止不给客机完整记录。局内日志保留最近512条可见事件，完整复盘保留全部事件。

## 模块复用

`wizard_network` 是独立 C++17 库，只依赖规则、内容与纯交互库，没有 SFML 或 Godot 类型。`HostSession` 接收客机消息、验证命令并创建各席位快照；`GuestSession` 保存裁剪视图、请求及重连凭据。主机本人的操作也经过权威会话。客机不创建 `GameEngine`。客机的`submit`返回`pending=true`表示已排队，必须等待Accepted/View后再表现规则结果；本地与主机即时提交返回`pending=false`。

`Peer` 组合会话、TCP、心跳与复盘持久化，提供 `host/join/tick/deck/configure/ready/submit/view/save/leave`。每帧调用 `tick`，页面和动画不得停止它。`Application::viewFor/actingPlayer/submit/saveReplay` 是 SFML 的共用对局入口；本地回放验收工具仍可访问原 `MatchSession`。

Godot 接入首选将 `Peer` 封装进 GDExtension，以复制快照桥接脚本。纯会话层也可消费其他字节流适配器；替换 TCP 适配时沿用 `frame/Decoder` 及会话接口，不改变协议。Godot 页面、卡牌场景与特效待视觉方案确定后实施，不能把接口准备视为 Godot 已交付。

## 协议1

大端32位长度前缀 + UTF-8 JSON对象。单消息1MiB，接收缓冲2MiB，发送队列4MiB，JSON最大嵌套64层。传输每次轮询处理有上限，避免阻塞界面。64位标识使用十进制字符串；卡牌定义从已校验的本地卡池查询。

消息包括 Hello/Joined、Room/Deck/Ready、View、Command/Accepted/Rejected、Heartbeat、Resume/Resumed、Replay、ReplayBegin/ReplayChunk/ReplayComplete/ReplayReceived、End/Error。握手验证协议、规则、卡池及内容哈希。连接绑定席位，命令含会话、序号、请求ID、revision、决策ID；不使用客机提供的actor。保留最近256次提交结果，重复请求返回原结果，更旧请求拒绝，不会重付费用。

View仅包含本方投影、合法动作和可见事件，不含种子、完整状态摘要或对手牌库/手牌。事件有稳定编号与`eventBase`，截断历史不会重复反馈。终局复盘分块传输，每块64KiB，总上限32MiB；收到后通过正常重放验证再保存，并向房主确认收到；正常关闭等待确认及发送队列排空。

朋友对战以互信为边界：权威房主持有完整状态；协议不提供针对恶意房主的反作弊或互联网身份认证。

## 验收命令

```powershell
ctest --preset windows
python tools/network_smoke.py --exe build/windows/Release/wizard_network_probe.exe --output build/network-probe-verified
python tools/network_smoke.py --mode client --exe build/windows/Release/wizard_client.exe --output build/network-ui-verified
```

探针运行两独立进程，覆盖正常终局、投降及重连。图形冒烟通过真实卡牌操作入口完成对局，同时打开设置，生成终局截图和审计结果。两者都检查双方复盘摘要一致；它们不能代替两台实体电脑和实际虚拟局域网验收。

独立安装目录使用 `dist/WizardCard-1.1-alpha`，保留原1.0发行包。两机三局、虚拟局域网、人工视觉与声音验收通过前，不合入master、不宣称正式发行。
