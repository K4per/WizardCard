# Alpha v2.0 模块接口与协作

设计决定见 [ADR](adr/0001-alpha-v2-modular-native-engine.md)。工程阶段保持旧规则行为；新规则单独接入并验证。每个模块提供稳定公共入口、隐藏内部状态、独立测试与明确依赖。

## 模块职责

| 模块 | 接口与职责 | 不变量和测试入口 |
|---|---|---|
| core/types | ID、内容定义、命令、事件、决策、配置、只读投影 | 无JSON/文件/渲染/套接字；命令序号稳定 |
| core/engine | submit、viewFor、确定性摘要 | 副本验证后原子提交；全部候选经相同校验 |
| core/commands | 普通操作、支付、目标与响应宣告 | 失败无部分支付；命令回归 |
| core/chain | 构建、优先权、响应候选、逆序结算 | ID精确、四速封锁；chain回归 |
| core/phases | 阶段子步、触发、收尾、回位 | 暂停恢复不重复；rules回归 |
| core/resources | 魔素、荷载、容量、环位、费用与伤害计算 | 规则判断共享；rules/card_updates回归 |
| core/maintenance | 宿主关联离场、终局、状态不变量 | 关联清理完整；invariants回归 |
| core/effects | 有限效果结算及效果事件 | 无文本解释执行；alpha_cards回归 |
| content | 内容清单、单卡读取、字段/交叉引用校验 | 原顺序重组并保持哈希；content回归 |
| codec | 命令序列化 | 显式兼容序号、拒绝有损ID；round-trip回归 |
| replay | 接受命令记录、保存和严格重放 | 逐步摘要、版本/哈希验证；replay回归 |
| session | 共同本地/联网会话接口及本地适配 | 客机pending不视为已接受；会话回归 |
| application | 构筑、设置、AI/教学与会话生命周期 | generation/revision；application/decks/ai回归 |
| interaction | 操作、目标、成本、确认与可见演出提示 | 选择无支付；interaction/presentation/sound回归 |
| network/session | 主机权威、客机投影、请求去重与重连 | 连接绑定身份，隐私；network回归 |
| network/transport | 非阻塞TCP字节流 | 长度/队列上限；双进程网络探针 |
| bridge | 会话/应用接口到Godot的白名单DTO | uint64字符串；桥接边界冒烟 |
| godot | 独立场景、主题、控件、输入、音频、动画 | 仅确认事件驱动演出；真实页面冒烟 |

## 依赖方向与共同会话

core → 标准库；content/codec/interaction/ai → core；replay → content/codec/core；session → core/replay；network → session/codec/content/interaction，transport → 平台套接字；application → session/content/ai/interaction；CLI与桥接组合根选择本地/网络适配，Godot只依赖桥接公开DTO。应用公共接口不返回具体Peer或GameEngine，离线调试/复盘校验有独立入口。

dev.2共同会话接口提供viewFor、submit、revision、started、blocked、tick；actingPlayer由应用投影推导，recording/saveReplay分别由本地复盘和房间持久化入口承担，不强迫客机暴露尚未获得的记录。房间的host/join/deck/configure/ready/leave由RoomSession承担，本地会话不实现空网络方法。旧客机accepted且pending表示入队，必须等pending清除及权威修订后才消费规则事件。

内部状态头只供规则实现和离线夹具；公开视图包括裁剪CardView、PlayerView、合法动作、决策和可见事件。历史core.hpp作为迁移兼容入口，新的生产调用者使用具体头文件。

工程过渡期间Application保留match()供历史离线验证和旧客户端使用，仍可达内部状态；新Godot入口只能使用桥接白名单DTO。这个兼容入口的彻底移除依赖旧客户端调用迁移，不能将dev.2称为最终公共接口隔离已经全部完成。模块实际接口和测试入口见src/core、src/content、src/application中的README。

## 内容与兼容

每张正式卡由 cards/<type>/<id>.json 唯一维护，catalog.json 明确元数据和加载顺序。清单重组得到旧cards.json完全相同的逻辑JSON，工程阶段内容哈希不变。cards.json只作为旧工具/夹具兼容的生成产物；生成器检查漂移，C++加载器优先清单，旧单文件夹具仍可读取。

卡面文字和规则参数属于内容哈希，插画和表现资源不属于。所有加载失败包含文件/字段定位；一个文件出错不能装载部分正式卡池。新规则卡池只有组内审定后才能进入正式assets，草案存于独立审定目录。

## 工具链与桥接复用

复用snapshot/v1.5-25d-art中的桥接模型、DTO、会话生命周期和回放测试，逐项适配新的会话/内容接口；旧2.5D场景与A素材只作历史参考。

Godot4.7.2/API4.7，同版模板，godot-cpp10.0.0-stable提交507ed9d840c01a3c5b2a39af8bb4000bfac30bf5。CMake可选桥接，Windows动态运行库、独立debug/release，Linux PIC。核心CI关闭SFML与Godot；桥接和导出任务另设。

## 多人改动约定

单卡改动只修改独立定义并重新生成兼容产物；新效果在effects模块实现并补外部行为测试；场景只改对应Godot页面与复用控件；网络故障只改会话/传输。公共接口变更先同步其调用者和接口测试，按模块小批次提交。模块README只记录接口、不变量和测试入口，避免复制整份规则规格。
