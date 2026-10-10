# Alpha v2.0 实施记录

启动日期2026-10-10，基线dev提交cf546ff。本记录区分规格交付、代码与实际验证；阶段计划见[迭代路线](../plans/alpha-v2.0.md)。现有未提交美术、平衡和文档资料保留。

## 当前状态

dev.1规格、架构接口、30卡迁移草案和9份40+10预设已落地；dev.2模块化、共同会话和可选桥接的本地关卡已通过；远程CI与保留客户端构建正在核对。当前正式规则/卡池仍1.0.0，不声明Alpha v2.0已可发行。

## 历史复用核对

当前dev仅有Godot二进制缓存；snapshot/v1.5-25d-art提交2e78cd6保存src/bridge、bridge.hpp、Godot本地页面与17项集成检查。历史报告记载208项本地Release检查通过，但该结果只适用于当时快照；重接入后的代码需重新验证。复用DTO、生命周期、构筑与设置持久化、音频投影和复盘冒烟，重新制作2D场地与C像素页面。

工具链本机版本核对为4.7.2.stable.official.ed1daf0bf；godot-cpp本机HEAD为507ed9d840c01a3c5b2a39af8bb4000bfac30bf5、标签10.0.0-stable。

## 待外部关卡

- 用户于2026-10-10反馈卡牌草案“需要调整，稍后提供修改意见”。30卡、阵法配方/等级和40+10预设继续保持草案状态，不能接入正式内容。
- GitHub已授权，父任务#2与13个子任务#3–#15已建立原生父子/阻塞关系。
- 真人两机、虚拟LAN、IME/DPI/声音以及发行远程CI尚未进行。

## dev.1 基线证据

- 纯核心/CLI/网络/应用在 Windows x64 MSVC 19.41、CMake 3.31.6、Release 构建成功。
- CTest 196/196 通过（53.22秒，含真实TCP、三档AI与逐步复盘）。日志：build/alpha-v2-baseline-tests.log。
- 固定种子42完整对局：结果0、摘要78116a78d64b0d90；参考记录build/alpha-v2-baseline/reference-smoke.json。后续通过同一记录逐步重放检查工程兼容。
- 卡池规则与内容版本仍1.0.0；30卡和9份预设见docs/proposals/alpha-v2/review.md，全部review-only。

## dev.2 工程与本地验收

程序标识2.0.0-dev.2，正式规则/卡池仍1.0.0，复盘格式仍2、网络协议仍1。

- 内核拆为提交编排、命令、连锁、阶段、资源、区域、效果、不变量、候选、投影和摘要模块；11个分类公共头，core.hpp为兼容入口。
- 30个稳定ID独立卡文件、显式catalog.json清单；加载校验重复ID/文件、目录逃逸、ID不符、缺失文件。旧cards.json为生成兼容产物，逻辑内容及顺序不变。
- 内容、命令编码、复盘独立库；Session/RoomSession及本地/网络适配共用公开投影与提交接口；Application不依赖具体Peer/TCP，客户端组合根注入RoomFactory。
- 纯核心WIZARD_CORE_ONLY配置与Release构建通过，不发现JSON/Catch2/网络/Godot/SFML；14目标依赖无环，AI/表现/会话公共头不可达GameState/GameEngine。
- CTest **213/213**通过（59.56秒），含新增内容清单、会话、历史桥接12项与旧复盘兼容。日志build/alpha-v2-modular-tests.log。
- Godot4.7.2 + 固定godot-cpp提交实际编译；**5/5**导入、全新目录冷导入、边界、CLI参考、Godot重放通过（48.68秒）。重放**289条**命令，最终摘要78116a78d64b0d90，与旧基线一致。内容哈希733319e247ac38ff不变。
- 首次直接--import曾稳定触发0xC0000005，空工程不复现、只注册扩展仍复现、再次打开同目录不复现、延迟退出通过。行为与[上游#111048](https://github.com/godotengine/godot/issues/111048)的文档生成/退出竞态一致（推断，未取得本机原生回溯）。保留--import并增加--frame-delay 1000；独立冷导入检查每次新建目录，未使用失败后重试隐藏错误。
- 分层CI增加内容漂移、模块依赖、纯核心与独立Godot桥接任务；远程运行结果尚未作为发行证据。

### 保留限制

Application.match()和core.hpp仍供历史客户端/离线验证兼容，不表示最终公共接口隔离全部完成。新Godot玩家入口只使用白名单DTO。此阶段仅恢复原生桥接与验证入口，未交付新版Godot页面、新规则闭环、协议2/复盘3、Windows发行包或真人验收。卡牌待修改期间，规则工作使用独立测试夹具，不采用未审定卡牌提案作为正式规则参数。
