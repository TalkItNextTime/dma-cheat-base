# 数据源对接文档

本文档面向“其他数据源”接入 ObservRadar。目标是：不改前端渲染逻辑，仅通过 WebSocket 推送标准事件包即可驱动雷达。

## 1. 对接入口

ObservRadar 宿主启动后会监听 WebSocket 端口（默认 `36365`）：

- WebSocket：`ws://127.0.0.1:36365`
- 静态页：`http://127.0.0.1:36364`

服务端代码位置：`src/socket.js`。

## 2. 协议格式

### 2.1 单包格式

```json
{
  "type": "players",
  "data": { "players": [] }
}
```

### 2.2 批量格式（推荐）

```json
[
  { "type": "map", "data": "de_mirage" },
  { "type": "round", "data": "live" },
  { "type": "players", "data": { "players": [] } }
]
```

说明：

- 服务端同时支持“单对象”与“数组”两种入站消息。
- 生产者不需要发送 `requestWelcome`，该消息仅前端连接初始化使用。

## 3. 推荐发包节奏

- 频率：`20~30 FPS`（推荐约 `24 FPS`）
- 一帧内按以下顺序发送（与 `memory-client` 保持一致）：

1. `provider`（可选）
2. `map`
3. `player`（可选，兼容旧渲染）
4. `allplayers`（可选，兼容旧渲染）
5. `grenades`（可选，兼容旧渲染）
6. `score`
7. `round`
8. `phase_countdowns`（可选）
9. `canbuy`
10. `bomb`
11. `players`
12. `smokes`
13. `flashbangs`
14. `infernos`
15. `projectiles`

## 4. 最小可运行事件集

如果你只想先跑起来，至少发送这 5 类事件：

- `map`
- `round`
- `players`
- `bomb`
- `score`

否则会出现：地图不切换、玩家不显示、状态条不更新等问题。

## 5. 字段契约

### 5.1 `map`

```json
{ "type": "map", "data": "de_mirage" }
```

要求：

- `data` 为字符串地图名。
- 支持带路径，前端会取最后一段（如 `maps/de_mirage.vpk`）。

### 5.2 `round`

```json
{ "type": "round", "data": "live" }
```

可用值：

- `freezetime`
- `live`
- `over`

### 5.3 `score`

```json
{ "type": "score", "data": { "ct": 8, "t": 6 } }
```

### 5.4 `canbuy`

```json
{ "type": "canbuy", "data": true }
```

### 5.5 `bomb`

```json
{
  "type": "bomb",
  "data": {
    "state": "planted",
    "player": "7656119....0",
    "countdown": 31.2,
    "defuseCountdown": 0,
    "plantCountdown": 0,
    "bombTicking": true,
    "timerLength": 40,
    "plantLength": 3.2,
    "site": "A",
    "hasDefuseKit": false,
    "position": { "x": 120.0, "y": -350.0, "z": 18.0 }
  }
}
```

`state` 常用值：

- `carried`
- `dropped`
- `planting`
- `planted`
- `defusing`
- `defused`
- `exploded`

### 5.6 `players`

```json
{
  "type": "players",
  "data": {
    "players": [
      {
        "id": "7656119....0",
        "steamid": "7656119....0",
        "num": 3,
        "name": "Player",
        "team": "CT",
        "health": 87,
        "armor": 100,
        "money": 5400,
        "hasHelmet": true,
        "hasDefuser": true,
        "connected": true,
        "active": false,
        "flashed": 0,
        "bomb": false,
        "ammo": { "weapon_m4a1": 25 },
        "position": { "x": 10.0, "y": 20.0, "z": 5.0 },
        "angle": 180.0,
        "active_weapon": "weapon_m4a1",
        "primaryWeapon": "weapon_m4a1",
        "secondaryWeapon": "weapon_hkp2000",
        "utilities": ["weapon_flashbang", "weapon_smokegrenade"]
      }
    ]
  }
}
```

必填建议字段：

- `id`、`num`、`name`、`team`
- `health`、`position`、`angle`
- `active_weapon`、`primaryWeapon`、`secondaryWeapon`
- `utilities`、`connected`

注意：

- `num` 建议固定在 `0~9`，与现有前端槽位一致。
- `team` 使用 `CT` / `T`。
- 武器名建议使用 `weapon_xxx` 规范，便于图标映射。

### 5.7 `smokes`

```json
{
  "type": "smokes",
  "data": [
    {
      "id": "123",
      "time": 5.2,
      "team": "CT",
      "position": { "x": 0, "y": 0, "z": 0 }
    }
  ]
}
```

说明：

- 前端按 `time` 驱动烟雾生命周期（20 秒内有效）。

### 5.8 `flashbangs`

```json
{
  "type": "flashbangs",
  "data": [
    { "id": "f1", "position": { "x": 0, "y": 0, "z": 0 } }
  ]
}
```

### 5.9 `infernos`

```json
{
  "type": "infernos",
  "data": [
    {
      "id": "i1",
      "flamesNum": 3,
      "flamesPosition": [
        { "x": 1, "y": 2, "z": 3 },
        { "x": 2, "y": 3, "z": 4 },
        { "x": 3, "y": 4, "z": 5 }
      ]
    }
  ]
}
```

### 5.10 `projectiles`

```json
{
  "type": "projectiles",
  "data": [
    {
      "id": "p1",
      "type": "smoke",
      "team": "T",
      "position": { "x": 0, "y": 0, "z": 0 }
    }
  ]
}
```

`type` 常见值：

- `frag`
- `flashbang`
- `smoke`
- `firebomb`
- `decoy`

## 6. 兼容事件（可选）

以下事件用于兼容旧渲染链路，可发可不发：

- `provider`
- `player`
- `allplayers`
- `grenades`
- `phase_countdowns`

建议新接入优先保证 `players + bomb + score + round + map`，再补兼容事件。

## 7. 发送示例（Node.js）

```js
const WebSocket = require("ws");

const ws = new WebSocket("ws://127.0.0.1:36365");

ws.on("open", () => {
  setInterval(() => {
    const packets = [
      { type: "map", data: "de_mirage" },
      { type: "round", data: "live" },
      { type: "score", data: { ct: 8, t: 6 } },
      {
        type: "bomb",
        data: {
          state: "carried",
          player: "",
          countdown: 999999,
          defuseCountdown: 0,
          plantCountdown: 0,
          bombTicking: false,
          timerLength: 40,
          plantLength: 3.2,
          site: "",
          hasDefuseKit: false,
          position: { x: 0, y: 0, z: 0 }
        }
      },
      {
        type: "players",
        data: {
          players: [
            {
              id: "1.0",
              steamid: "1.0",
              num: 0,
              name: "DemoCT",
              team: "CT",
              health: 100,
              armor: 100,
              money: 800,
              hasHelmet: true,
              hasDefuser: true,
              connected: true,
              active: true,
              flashed: 0,
              bomb: false,
              ammo: { weapon_m4a1: 30 },
              position: { x: 100, y: 100, z: 0 },
              angle: 90,
              active_weapon: "weapon_m4a1",
              primaryWeapon: "weapon_m4a1",
              secondaryWeapon: "weapon_hkp2000",
              utilities: ["weapon_flashbang"]
            }
          ]
        }
      },
      { type: "smokes", data: [] },
      { type: "flashbangs", data: [] },
      { type: "infernos", data: [] },
      { type: "projectiles", data: [] }
    ];

    ws.send(JSON.stringify(packets));
  }, 42); // 约 24 FPS
});
```

## 8. 联调检查清单

1. 打开 `http://127.0.0.1:36364/doorknock`，确认 `socket` 端口正确。
2. 页面能收到 `map`，地图从 waiting 切到 map。
3. `players` 持续更新时，点位与卡片同步刷新。
4. `bomb` 状态切换时，中部状态条与倒计时正确变化。
5. `smokes/infernos/flashbangs/projectiles` 与预期视觉一致。

## 9. 常见问题

### 9.1 连上了但页面不动

通常是没发 `map` 或 `players`，或者 `players.data.players` 结构不对。

### 9.2 只看到地图，看不到玩家

检查 `num` 是否稳定、`team` 是否为 `CT/T`、`position` 是否为数值。

### 9.3 倒计时异常

检查 `bomb` 包中的 `state / countdown / defuseCountdown / timerLength / hasDefuseKit`。

---

如需扩展字段，建议保持“新增字段向后兼容，不改旧字段语义”。
