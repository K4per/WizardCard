# Alpha v2.0 实施记录

启动日期2026-10-10，基线dev提交cf546ff。本记录区分规格交付、代码与实际验证；阶段计划见[迭代路线](../plans/alpha-v2.0.md)。现有未提交美术、平衡和文档资料保留。

## 当前状态

dev.1规格、架构接口、30卡迁移草案和9份40+10预设已落地；dev.2工程拆分与兼容验证进行中。当前正式规则/卡池仍1.0.0，不声明Alpha v2.0已可发行。

## 历史复用核对

当前dev仅有Godot二进制缓存；snapshot/v1.5-25d-art提交2e78cd6保存src/bridge、bridge.hpp、Godot本地页面与17项集成检查。历史报告记载208项本地Release检查通过，但该结果只适用于当时快照；重接入后的代码需重新验证。复用DTO、生命周期、构筑与设置持久化、音频投影和复盘冒烟，重新制作2D场地与C像素页面。

工具链本机版本核对为4.7.2.stable.official.ed1daf0bf；godot-cpp本机HEAD为507ed9d840c01a3c5b2a39af8bb4000bfac30bf5、标签10.0.0-stable。

## 待外部关卡

- 30卡迁移、阵法配方/等级和40+10预设由组内审定，正式内容接入前保持草案状态。
- GitHub已授权，父任务#2与13个子任务#3–#15已建立原生父子/阻塞关系。
- 真人两机、虚拟LAN、IME/DPI/声音以及发行远程CI尚未进行。

## dev.1 基线证据

- 纯核心/CLI/网络/应用在 Windows x64 MSVC 19.41、CMake 3.31.6、Release 构建成功。
- CTest 196/196 通过（53.22秒，含真实TCP、三档AI与逐步复盘）。日志：build/alpha-v2-baseline-tests.log。
- 固定种子42完整对局：结果0、摘要78116a78d64b0d90；参考记录build/alpha-v2-baseline/reference-smoke.json。后续通过同一记录逐步重放检查工程兼容。
- 卡池规则与内容版本仍1.0.0；30卡和9份预设见docs/proposals/alpha-v2/review.md，全部review-only。
