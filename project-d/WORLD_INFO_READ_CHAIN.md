# ObservRadar `memory-client` 世界信息读取链路

本文档描述当前仓库中 `memory-client` 的真实世界信息读取链路，只覆盖后端内存读取与 `RadarFrame` 组装，不覆盖前端渲染逻辑。

## 1. 总入口

- 入口类：`memory-client/src/reader/game_state_reader.h` 中的 `GameStateReader`
- 主流程：`GameStateReader::poll() -> refresh_radar_frame()`
- 输出结构：`memory-client/src/reader/radar_payload.h` 中的 `RadarFrame`

`poll()` 的实际步骤：

1. 通过 `IMemoryBackend` 连接 `cs2.exe`
2. 获取 `client.dll` 基址
3. 记录一组静态入口地址
4. 调用 `refresh_radar_frame()` 读取完整一帧世界状态

## 2. 静态入口地址

当前代码从 `client.dll` 基址直接拼出以下入口地址：

- `dwEntityList`
- `dwLocalPlayerController`
- `dwLocalPlayerPawn`
- `dwPlantedC4`
- `dwGlobalVars`
- `dwGameRules`
- `dwGameEntitySystem`
- `dwWeaponC4`

对应定义位于：

- `memory-client/offsets/offsets.hpp`
- `memory-client/offsets/client_dll.hpp`

注意：

- `snapshot_.entity_list`、`snapshot_.global_vars`、`snapshot_.planted_c4` 等字段里存的是静态入口地址，不是最终对象地址
- 真正读取对象前，代码会再做一次 `read_pointer(...)` 解引用

## 3. 通用实体解析链

当前项目统一使用这一套实体解析逻辑：

1. 先读出 `entity_list = * (client_base + dwEntityList)`
2. 通过索引计算 block：
   `list_entry = *(entity_list + 0x8 * ((index & 0x7FFF) >> 9) + 0x10)`
3. 在 block 内按 stride 取实体指针

当前实现支持两种 stride：

- 主 stride：`0x70`
- 兼容 fallback：`0x78`

对应函数：

- `resolve_entity_index(...)`
- `resolve_entity_handle(...)`

句柄解析规则：

- `handle == 0` 或 `0xFFFFFFFF` 直接视为无效
- 有效句柄使用 `handle & 0x7FFF` 取实体索引

## 4. GlobalVars 读取链

`GlobalVars` 是当前后端世界信息的核心时间源和地图名来源。

真实链路：

1. `snapshot_.global_vars = client_base + dwGlobalVars`
2. `global_vars = *snapshot_.global_vars`
3. 从 `global_vars` 继续读取字段

当前代码实际使用的字段偏移：

- 地图名指针：`global_vars + 0x188`
- 当前游戏时间主偏移：`global_vars + 0x2C`
- 当前游戏时间备用偏移：`global_vars + 0x30`
- 当前 tick：`global_vars + 0x1C`
- `interval_per_tick`：`global_vars + 0x20`

实现细节：

- 地图名通过 `0x188` 取到字符串指针，再读取 C 字符串
- 时间优先读取 `0x2C`，不可用时回退到 `0x30`
- 只有“有限值且大于 0”的时间才算有效
- 地图名会经过 `normalize_map_name(...)` 标准化，并用 `looks_like_map_name(...)` 做有效性检查

这也是当前实现与旧版链路的最大差异之一：当前代码已经不再使用旧的 `GlobalVars + 0x10` 作为炸弹倒计时主时间基准。

## 5. 玩家读取链

玩家扫描范围：

- 控制器索引 `1..64`

真实读取链：

1. `controller = resolve_entity_index(entity_list, index)`
2. 优先读 `CCSPlayerController::m_hPlayerPawn`
3. 若为空，再回退读 `CBasePlayerController::m_hPawn`
4. `pawn = resolve_entity_handle(entity_list, pawn_handle)`

玩家主字段来源：

- `steam_id`：`CBasePlayerController::m_steamID`
- 名字：`CBasePlayerController::m_iszPlayerName`
- 连接状态：`CBasePlayerController::m_iConnected`
- 血量：`C_BaseEntity::m_iHealth`
- 护甲：`C_CSPlayerPawn::m_ArmorValue`
- 阵营：`C_BaseEntity::m_iTeamNum`
- 是否在买区：`C_CSPlayerPawn::m_bInBuyZone`
- 位置：`m_pGameSceneNode -> m_vecAbsOrigin`
- 朝向：`C_CSPlayerPawn::m_angEyeAngles`
- 闪白：`m_flFlashDuration + m_flFlashOverlayAlpha`

连接状态判定：

- 当前实现把 `0 / 1 / 2` 视为有效或连接中玩家
- 其他值视为非连接状态

与 `PlayerConnectedState` 的对应关系：

- `0 = PlayerConnected`
- `1 = PlayerConnecting`
- `2 = PlayerReconnecting`
- `4 = PlayerDisconnected`

## 6. 本地观察目标链

当前代码支持从本地观察者目标里判断“当前激活玩家”：

1. `local_pawn = *(client_base + dwLocalPlayerPawn)`
2. `observer_services = *(local_pawn + m_pObserverServices)`
3. `observer_target_handle = observer_services + m_hObserverTarget`
4. `active_pawn = resolve_entity_handle(entity_list, observer_target_handle)`

若某玩家的 `pawn == active_pawn`，则：

- `player.active = true`

## 7. 金钱、头甲、钳子与武器链

金钱链：

1. `money_services = *(controller + m_pInGameMoneyServices)`
2. `money = money_services + m_iAccount`

头甲/钳子链：

1. `item_services = *(pawn + m_pItemServices)`
2. `has_helmet = item_services + m_bHasHelmet`
3. `has_defuser = item_services + m_bHasDefuser`

武器链：

1. `weapon_services = *(pawn + m_pWeaponServices)`
2. 读取 `m_hActiveWeapon`
3. 读取 `m_hMyWeapons`
4. 遍历 weapon handle -> resolve entity
5. 通过 `C_EconEntity::m_AttributeManager + C_AttributeContainer::m_Item + C_EconItemView::m_iItemDefinitionIndex` 取 `definition index`
6. 用 `weapon_name_from_definition_index(...)` 转成武器名

当前会在这一轮里组装：

- `active_weapon`
- `primary_weapon`
- `secondary_weapon`
- `utilities`
- `ammo`
- `bomb`
- `bomb_active`

## 8. 计分板与队伍分数链

当前代码没有单独的队伍实体入口，而是直接在 `entity_list` 中扫描前 `1..255` 个实体，尝试把它们识别为 `C_Team`。

使用字段：

- `C_BaseEntity::m_iTeamNum`
- `C_Team::m_iScore`
- `C_Team::m_szTeamname`

最终写入：

- `radar_frame_.ct_score`
- `radar_frame_.t_score`

## 9. 投掷物与世界效果读取链

入口：

1. `game_entity_system = *(client_base + dwGameEntitySystem)`
2. `highest_entity_index = *(game_entity_system + dwGameEntitySystem_highestEntityIndex)`
3. 扫描范围会被提升到至少 `1280`
4. 最终上限是 `0x7FFF`

对应函数：

- `collect_utility(...)`

### 9.1 类型识别策略

当前实现按下面顺序识别投掷物类型：

1. 优先读 `designer_name`
2. 通过 `projectile_type_from_entity_name(...)` 直接识别
3. 对 smoke 做专门 fallback
4. 读实体缓存
5. 读 subclass 缓存
6. 读 effect 缓存
7. 读最近一次玩家手持道具缓存
8. 对燃烧瓶再用 `m_bIsIncGrenade` 兜底

当前支持的类型：

- `frag`
- `flashbang`
- `smoke`
- `firebomb`
- `decoy`
- `inferno`

### 9.2 Smoke 链

烟雾只在名字明确像烟雾弹实体时才生成，避免把别的效果误识别成烟雾。

关键字段：

- `C_BaseGrenade::m_bIsSmokeGrenade`
- `C_SmokeGrenadeProjectile::m_bDidSmokeEffect`
- `C_SmokeGrenadeProjectile::m_nSmokeEffectTickBegin`
- `C_SmokeGrenadeProjectile::m_vSmokeDetonationPos`

时间计算：

- `smoke_start_time = smoke_effect_tick_begin * interval_per_tick`
- `smoke_elapsed = current_time - smoke_start_time`

当前后端寿命上限来自 `memory-client/src/reader/radar_payload.h`：

- `kSmokeLifetimeSeconds = 15.0f`

这表示当前 memory client 仍然以 15 秒为后端有效窗口，不是 20 秒。

### 9.3 Flashbang 链

关键字段：

- `m_bExplodeEffectBegan`
- `m_nExplodeEffectTickBegin`
- `m_vecExplodeEffectOrigin`

当前仅在“爆炸已开始且 tick 差值不大于 16”时生成一次 flash 事件。

### 9.4 Inferno 链

当前 inferno 不是只靠 `fireCount > 0` 就直接接受，还做了额外过滤。

关键字段：

- `C_Inferno::m_fireCount`
- `C_Inferno::m_bFireIsBurning`
- `C_Inferno::m_firePositions`

过滤规则：

1. 至少读到有效 flame 点
2. flame 数量足够形成可信中心点
3. inferno 中心必须靠近最近记录的 molotov 落点

这正是当前代码里过滤错误火焰效果的主逻辑。

## 10. Bomb 链

当前炸弹逻辑分成三种状态：

1. 已种下
2. 正在安包
3. 携带或掉落

对应函数：

- `update_bomb_state(...)`

### 10.1 已种下的 C4

真实链路：

1. `game_rules = *(client_base + dwGameRules)`
2. `m_bBombPlanted == true`
3. `planted_c4 = *(client_base + dwPlantedC4)`
4. 若 `planted_c4` 本身还是一个指针包装，再额外解一次引用

当前读取的 planted C4 字段：

- `m_bBombDefused`
- `m_bHasExploded`
- `m_bBombTicking`
- `m_bBeingDefused`
- `m_flTimerLength`
- `m_flDefuseLength`
- `m_nBombSite`
- `m_hBombDefuser`
- `m_pGameSceneNode -> m_vecAbsOrigin`

关键说明：

- 当前代码已经不再用 `m_flC4Blow - current_time` 直接驱动前端倒计时
- 当前策略是读取 `timerLength / defuseLength` 作为总时长
- 然后用 `steady_clock` 本地递减

也就是说，当前 bomb / defuse 倒计时是“本地连续倒计时”，不是每帧重新依赖游戏里的绝对爆炸时间戳。

### 10.2 拆包状态

若 `m_bBeingDefused == true`：

- `bomb.state = "defusing"`
- `defuse_countdown` 使用本地倒计时
- 通过 defuser pawn 的 `m_pItemServices -> m_bHasDefuser` 读取是否有钳子
- 再回查 defuser 的 controller，写回 `bomb.player`

### 10.3 安包状态

未种包时，代码还会读取地上的或手上的 `weapon_c4`：

1. `weapon_c4 = *(client_base + dwWeaponC4)`
2. 若需要，再额外解一次引用
3. 读取 `C_C4` 字段：
   - `m_bStartedArming`
   - `m_bIsPlantingViaUse`
   - `m_fArmedTime`

判定条件：

- 玩家当前确实携带 C4
- 且 `started_arming || planting_via_use`

若满足：

- `bomb.state = "planting"`
- `bomb.plant_length = 3.2f`
- 倒计时主体仍然是本地 `steady_clock` 递减
- 若 `m_fArmedTime > current_time`，会用它做一次初始剩余时间同步

### 10.4 携带与掉包

若玩家武器列表里有 `weapon_c4`：

- `bomb.state = "carried"`

若玩家未携带，但 `dwWeaponC4` 仍能解析出实体：

- `bomb.state = "dropped"`

## 11. 回合状态链

回合状态由 `update_round_state(...)` 统一生成。

使用字段：

- `C_CSGameRules::m_bFreezePeriod`
- `C_CSGameRules::m_bWarmupPeriod`
- `C_CSGameRules::m_bHasMatchStarted`
- `C_CSGameRules::m_bBombPlanted`

当前输出只有三种：

- `freezetime`
- `live`
- `over`

`can_buy` 的判断不是直接读单个全局字段，而是依赖：

- 只要存活玩家里有人在买区，就认为当前可买

## 12. 当前文档结论

当前 `memory-client` 的世界信息读取链路，核心已经稳定为下面这套模型：

1. `client.dll` 静态 offsets 定位入口
2. `entity list` + handle 统一解析实体
3. `GlobalVars` 提供地图名、时间、tick、`interval_per_tick`
4. `controller -> pawn` 读取玩家状态
5. `GameEntitySystem` 扫描投掷物和世界效果
6. `GameRules + PlantedC4 / WeaponC4` 读取炸弹状态
7. 炸弹 / 拆包 / 安包倒计时统一改为“总时长 + `steady_clock` 本地递减”

如果后续代码再次调整了 `GlobalVars` 偏移、smoke 生命周期，或恢复为绝对游戏时间驱动倒计时，这份文档也需要同步更新。
