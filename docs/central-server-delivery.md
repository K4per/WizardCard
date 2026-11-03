# 中央游戏服务器交付记录

2026-11-03。审计、设计、计划与实施均已完成；设计依据为 `docs/central-server-audit.md`、ADR 0003 及 `docs/superpowers/` 下的中央服务器 spec/plan。

## 玩家连接

房间页填写主机名 **aq.rainplay.cn**，公网端口 **23513**。受信 CA 填写本仓库 `deploy/production/aq.rainplay.cn.crt` 的绝对文件路径；证书是部署机生成的自签名信任锚，需要显式分发。随后创建房间并把八位房间码交给另一位玩家。服务端内部监听 24443；24444 健康端口仅回环，不对公网开放。

证书 SHA-256 指纹：`EF:06:79:BB:81:22:7C:00:88:FE:B1:B1:1E:6F:CF:D2:09:3E:D3:05:91:13:C3:4E:B9:76:03:6F:5E:F7:4D:67`。有效期 2026-11-03 至 2027-11-03；到期前更换证书并重新分发信任锚。私钥仅保存在服务器 `/etc/wizardcard/server.key`。

## 实现与部署

可选 `wizard_server` 为 C++17 / Asio 1.36.0 / OpenSSL 3 的多房间权威服务，中央协议 5，原 P2P 协议 4 保留回归。生产使用规则/卡池 2.6.0、mechanics 10、hash `41eac6fc263abd5e`；客户端使用 Godot 自身的证书校验 TLS，应用层不依赖网络运行时。提供 60 秒断线冻结与重连、请求去重、独立存储暂停、增量 journal、格式 5 终局复盘、重启中止与历史保留。

部署目标为 Debian 13 x86_64，2 CPU / 3921 MiB RAM。兼容二进制在 Debian 13 容器内用 GCC 14.2 / glibc 2.41 构建，已在目标机检查共享库兼容。系统服务 `wizard-server.service` 已启用开机启动，以 `wizardcard` 无登录账户运行，受 2 CPU / 4 GiB、只读系统目录和专用可写数据目录限制。

- 版本目录：`/opt/wizardcard-central-server/releases/WizardCard-CentralServer-2.6.0-hotfix.1-linux-x86_64`，`current` 为版本链接。
- 配置与证书：`/etc/wizardcard/`，私钥 `root:wizardcard 0640`。
- 持久化目录：`/var/lib/wizardcard-central-server/`，仅服务账户可写。历史目录由公开房间码、进程实例 ID 与递增序号组成，重用房间码不会覆盖旧记录。
- 就绪：`ssh WizardGameServer 'curl -fsS http://127.0.0.1:24444/ready'`。
- 查看/重启：`ssh WizardGameServer 'sudo -n systemctl status wizard-server --no-pager'`、`sudo systemctl restart wizard-server`。
- 首次发布回滚：`sudo systemctl disable --now wizard-server`，保留配置、证书及数据。后续版本可停止服务、把 `current` 指回旧版本，再启动。升级前备份数据。

## 验证证据

本机 Arch Linux，Godot 4.7.2（ed1daf0bf）。独立审查发现的九项重要问题均补充了失败回归并修复；另修复 bridge 中止状态、手动重连及既存文件失败路径。完整证据保存在实施工作区 `build/` 与交付归档；以下数字来自实际运行。

| 检查 | 结果 |
| --- | --- |
| 无服务器依赖的 headless | 273/273，28.89 秒 |
| server-debug 全量 | 276/276，79.78 秒，含真实 TLS、64 连接上限、握手超时、故障注入停机、短时玩法压力 |
| Linux Godot 全量 | 301/301，230.32 秒，包含双方客户端、TLS 背压与已验证一致复盘 |
| 合并后主工作区客户端 | 桥接库及 CLI 重建成功；前端 import 和中央房间界面 2/2 通过，6.86 秒 |
| P2P 历史及正式卡池 | 两个卡池的 normal / surrender / reconnect 全通过；normal / reconnect 各 2241 条命令，双方 digest 一致 |
| 模块/资源/卡池 | 16 targets 无环，application 与传输隔离；414 前端资源检查；正式 92 定义、40 张默认主卡组及内容 hash 校验通过 |
| core-only | 标准库内核配置、构建通过 |
| Debian 13 实际交付包 | 解包二进制真实 TLS、错误 CA/hostname、限流、SIGTERM、SIGKILL 后重启中止均通过；包内含第三方许可，不含私钥 |
| 磁盘阻塞停机 | LD_PRELOAD 注入真实 fsync 阻塞，8.03 秒退出；重启以完整持久化前缀处理 |
| 非 root 容器 | UID 10001、只读根目录、可写持久化 volume、健康探测通过 |
| 公网 TLS | `aq.rainplay.cn:23513`，TLS 1.3，证书链及 hostname 校验通过 |
| 公网真实 Godot 对局 | 房间 N8B728RC，手动重连、准备、投降退出；两份已验证复盘一致，digest `3a825d208605c8ef` |
| 远端服务重启 | 房间 9BMB5XL8 标记 aborted / server_shutdown；新实例、旧 Resume 拒绝，重启后 ready 正常 |

## 容量复验

最终运行在 Debian 13 容器内，cgroup 限制 2 CPU / 4 GiB，CPU affinity 0、1；服务端地址空间限制 4 GiB。20 房 / 40 客户端，持续 **600.496 秒**，每房约 **1.996 条有效命令/秒**。执行 **23,956** 条有效命令、**960** 次精确去重重试、**5** 次重连，慢读者被断开。回执 **p95 12.208 ms**，服务进程峰值 **65,104 KiB RSS**，未检测到串房、私牌泄漏、非预期中止或重复执行。原始报告 `build/central-capacity-final-debian13.json`。

这验证的是指定的约 2 命令/房/秒负载；不代表更高吞吐、互联网时延或长周期磁盘增长已做容量保证。历史数据不会自动清理，需由管理员备份和管理。

## CI 与范围

新增中央服务 Linux Release / Debian 13 容量与打包 CI，并接入 Linux Godot TLS 集成。Windows Godot、官方导出模板、X11 / Wayland 和 GitHub Actions 以远端 CI 结果为准，本次本机验证未宣称这些平台全部通过。初版没有账户、自动匹配、观战、分片、活局重启恢复或 Windows 服务端。

实现提交 `126fd98e5022aed44b24f9ec6296744a8ba9040d` 已快进到本地 `dev` 并推送 `origin/dev`，通过 `git ls-remote` 核对远端 SHA。GitHub Actions API 返回未认证调用的速率限制（HTTP 403），本次交付无法确认远端 CI 结论；可在仓库 Actions 页面查看实际运行状态。

计划中的 native C++ TLS probe 由真实 Python TLS 探针与 Godot 原生 TLS 集成替代；C++ 客户端协议通过注入 transport 测试，避免增加第二套产品 TLS 客户端。

## 发布校验

实际部署归档为 `WizardCard-CentralServer-2.6.0-hotfix.1-linux-x86_64.tar.gz`，SHA-256 `6c06a5303e43c15691516c71488400d791cb6daeb981715ce7a76235cd582f3b`。包内 145 个文件的清单校验通过；`bin/wizard_server` SHA-256 为 `75ca7f51052e22d32b8015aa6f111680850d7c656aec1259b268826e09b953d8`，与远端运行版本一致。构建镜像中的 214 个生产源码、构建输入和正式内容文件与最终工作树逐一比对一致。

随 Git 保存的原始报告位于 `docs/validation/central-server/`：`capacity-debian13.json`、`public-match.json`、`remote-restart.json`、`deployment.json`。测试运行日志、复盘及实际归档另保留在实施工作区的 `build/`、`dist/`；归档和公开 CA 也复制到主工作区 `dist/`。远端 `/etc/wizardcard/DEPLOYMENT.json` 记录源码提交、归档及二进制校验值，写入时再次核对了运行版本的二进制 SHA-256。
