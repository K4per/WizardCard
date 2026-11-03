# 中央服务器建设前审计

2026-11-03，基于 dev 3b94801。规则引擎为 C++17，Godot 4.7.2，程序 2.6.0-hotfix.1。正式内容 assets/release-v260：92 定义、40 张默认主卡组、规则/卡池 2.6.0、mechanics 10、hash 41eac6fc263abd5e。

原 Headless 构建通过，272/272 测试通过；模块依赖 13 targets 无环，应用层不依赖网络；卡池源检查和正式内容验证通过。独立进程 P2P 的 normal/surrender/reconnect 在历史及正式卡池均通过，双方复盘 digest 一致。隔离工作区再次构建并运行 272/272 测试通过（28.93 秒）。

当前网络 HostSession 仅支持一个本地权威玩家与一个远端玩家；Tcp 为一个监听器/连接；无多房间服务器、TLS 和中心客户端。Peer::persist 每次 revision 生成并同步写入全量复盘，诊断采样确认消耗在 JSON 序列化/写入，不能沿用为多房间热路径。现有查看者投影、独立动作查询、确定性格式 5 复盘、规则计时及技术暂停可复用。

根 assets 为历史 2.5.5/mechanics 9，部分默认测试、安装与网络文档使用历史值。生产服务需明确绑定 assets/release-v260，保留历史回归。UI 中 result 推断结束和 host 推断玩家身份的路径需要迁移为显式生命周期/查看者。

部署主机 SSH 已验证连通：Debian 13、2 核、约 4 GiB RAM、28 GiB 空闲磁盘；当前用户无免密 sudo，缺少 Docker/构建工具。使用非 root 用户目录部署并显式配置可信 TLS，不关闭证书验证。
