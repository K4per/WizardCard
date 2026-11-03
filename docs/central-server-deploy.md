# 中央游戏服务器部署

服务器运行 Linux x86_64、C++17 权威服务，正式卡池固定为 `assets/release-v260`（规则和卡池 2.6.0、mechanics 10、hash `41eac6fc263abd5e`）。Debian 13 是交付基线；在 Arch 或其他系统构建的二进制必须先确认目标机的 glibc 与共享库版本兼容。

游戏 TLS 监听 TCP 24443。健康和就绪 HTTP 端口 24444 只绑定服务器 loopback，不应映射或放行到公网。NAT 的公网端口可以不同，客户端填写公网端口。当前部署已验证 `aq.rainplay.cn:23513` → 服务器 `24443`，主机名验证仍使用 `aq.rainplay.cn`。实际部署证据见 `docs/central-server-delivery.md`。

## 构建与打包

需要 CMake 3.28+、Ninja、C++17 编译器、Git、Python 3、OpenSSL 3 开发文件和 `readelf`/`ldd`。

```sh
cmake --preset server-release
cmake --build --preset server-release --target wizard_server --parallel 2
python tools/package_central_server.py --build build/server-release
(cd dist && sha256sum -c WizardCard-CentralServer-2.6.0-hotfix.1-linux-x86_64.tar.gz.sha256)
```

要部署到 Debian 13，请在 Debian 13 x86_64 上直接构建，或使用 `deploy/Dockerfile` 的 Debian 13 构建阶段。Release 目录包含服务端、正式卡池、用户服务单元和校验清单，不包含 TLS 私钥。

## 用户级 systemd

在目标账户下解压归档，然后运行安装脚本：

```sh
tar -xzf WizardCard-CentralServer-2.6.0-hotfix.1-linux-x86_64.tar.gz
./WizardCard-CentralServer-2.6.0-hotfix.1-linux-x86_64/install-user-service.sh \
  WizardCard-CentralServer-2.6.0-hotfix.1-linux-x86_64.tar.gz
```

编辑 `~/.config/wizardcard-central-server/server.env`，放置证书和私钥，并将私钥权限设为 `0600`。安装脚本保留旧 release 和历史房间数据；配置仍含示例路径时只安装 unit，不会启动服务。配置完成后：

```sh
systemctl --user enable --now wizard-server.service
systemctl --user status wizard-server.service
curl --fail http://127.0.0.1:24444/ready
```

若服务必须在用户未登录时启动，可由管理员启用 user lingering：`sudo loginctl enable-linger <service-user>`。systemd 以服务账户运行，并限制写入到专用数据目录；证书目录只读。

默认数据目录是 `~/.local/share/wizardcard-central-server`。不要把 `.server.lock`、journal 或历史房间目录手动清空。升级前先停止服务并备份数据。切换回旧版本时将 `current` 符号链接指回 `releases/` 中旧目录，再重启 unit；回滚不删除数据，但不同版本之间不应假设 journal 格式可逆。

SIGTERM 会停止新连接，将未完成房间记录为中止并尝试刷盘；正常停机约 5 秒，磁盘阻塞时最迟 8 秒退出，重启扫描完整持久化前缀并记录中止。服务器重启不会恢复未完成对局；中止不产生胜者。房间历史目录使用房间码、实例 ID 与序号组成的唯一名称，重用公开房间码不会覆盖历史记录。

## 系统服务

具备 sudo 时推荐独立的 `wizardcard` 无登录账户。验证下载的 `.sha256` 后，执行包中的 `sudo sh install-system-service.sh RELEASE.tar.gz`。安装器将版本存到 `/opt/wizardcard-central-server/releases/`，通过 `current` 切换版本，写入 `/etc/systemd/system/wizard-server.service`。证书与环境配置位于 `/etc/wizardcard/`，持久化数据位于 `/var/lib/wizardcard-central-server/`。

首次安装后将证书和私钥设为 `root:wizardcard`、模式 `0640`，再执行 `sudo systemctl enable --now wizard-server`。用 `systemctl status wizard-server`、`journalctl -u wizard-server` 和回环 `/ready` 检查。服务以非 root 账户运行，仅允许写入数据目录，并限制 2 CPU/4 GiB。

回滚前停止服务，将 `/opt/wizardcard-central-server/current` 指向旧版本目录，随后启动；保留数据目录与证书。首次发布没有旧版本时，停止并禁用服务即可撤下，保留数据供诊断。

## 证书

优先使用包含目标 DNS 名的公开 CA 证书。没有公开证书时，可生成仅供明确分发信任的自签名证书：

```sh
install -d -m 0700 deploy/certs
openssl req -x509 -newkey rsa:3072 -nodes -days 365 \
  -subj '/CN=game.example.net' \
  -addext 'subjectAltName=DNS:game.example.net' \
  -keyout deploy/certs/server.key -out deploy/certs/server.crt
chmod 0600 deploy/certs/server.key
```

自签名证书同时作为客户端信任锚。通过可信渠道把 `server.crt` 分发到玩家设备，在房间页的受信 CA 字段填写其 PEM 文件路径。客户端必须使用证书 SAN 中的主机名连接；不能以 IP 代替证书主机名，也不能关闭证书校验。

本机验收：

```sh
curl --fail http://127.0.0.1:24444/ready
openssl s_client -connect game.example.net:24443 -servername game.example.net \
  -CAfile deploy/certs/server.crt -verify_hostname game.example.net -verify_return_error
```

公网验收还需从服务器外部网络执行相同 TLS 检查。健康端口不应从外部网络访问。

## Compose

在 `deploy/compose.yaml` 所在目录准备 `certs/server.crt` 和 `certs/server.key`，设置私钥为只读，然后执行：

```sh
docker compose -f deploy/compose.yaml up -d --build
docker compose -f deploy/compose.yaml ps
docker compose -f deploy/compose.yaml logs --tail=100 wizard-server
```

Compose 仅发布 24443/tcp，24444 保持容器 loopback。服务使用非 root UID、只读根文件系统、只读 secret、持久化命名 volume，并配置 2 CPU/4 GiB 上限。绑定挂载数据目录时，需确保 UID 10001 对目录可写。

## 容量与日志

结构化 stdout 日志不应记录恢复 token、私牌或随机种子。健康响应只报告 ready、驻留房间数、连接数及有效重连凭据数量。容量基线为 20 房/40 客户端、2 vCPU/4 GiB；容量报告必须记录实际持续时间、工作负载、p95 回执和峰值 RSS，不能把连接数 smoke 说成 10 分钟玩法压力测试。
