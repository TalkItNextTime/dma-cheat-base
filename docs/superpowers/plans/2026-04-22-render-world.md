# Render World Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把当前 VisCheck 线框调试叠加升级为基于现有地图三角面的半透明“渲染世界”效果。

**Architecture:** 保持 `VisCheck -> m_MapDebugTriangles -> ESP overlay` 这条现有数据链不变，只替换调试快照的数据结构和绘制算法。新增一个小型几何辅助模块负责背面剔除和深度排序，`ESP.cpp` 负责世界坐标投影和 ImGui 绘制，`Overlay.cpp` 负责 UI 命名与参数暴露。

**Tech Stack:** C++20, Dear ImGui draw list, Visual Studio/MSBuild, 现有 `VisCheck` 缓存几何, 自定义控制台测试可执行文件

---

## File Structure

- Create: `project-d/Source/Features/ESP/VisWorldDebugRender.hpp`
  责任：声明世界渲染调试用的轻量几何辅助类型与函数，只处理背面剔除、深度计算、排序，不接触 DMA、SDK、ImGui 状态。
- Create: `project-d/Source/Features/ESP/VisWorldDebugRender.cpp`
  责任：实现几何辅助函数，保证逻辑可单测。
- Create: `project-d/Tests/vis_world_debug_render_tests.cpp`
  责任：验证背面剔除、深度排序、颜色保留这些与运行环境无关的核心规则。
- Modify: `project-d/Source/Features/ESP/ESP.hpp`
  责任：把当前线段调试快照替换为三角面快照，声明新的缓存结构。
- Modify: `project-d/Source/Features/ESP/ESP.cpp`
  责任：构建三角面屏幕快照、执行 `WorldToScreen`、应用背面剔除、按深度排序、调用 `AddConvexPolyFilled` 绘制半透明面片。
- Modify: `project-d/Source/Overlay/Overlay.cpp`
  责任：把调试 UI 的文字从 `VisCheck Debug Overlay` 调整为“渲染世界”，保持现有参数入口。
- Modify: `project-d/Source/Config/Structs.hpp`
  责任：更新字段注释，使配置含义与实际功能一致。
- Modify: `project-d/Source\Config\Config.hpp`
  责任：更新默认配置说明文本，不新增第二套配置项。
- Modify: `project-d/project-d.vcxproj`
  责任：把新的 `VisWorldDebugRender.cpp/.hpp` 纳入主工程。

### Task 1: Add Testable Geometry Helper

**Files:**
- Create: `project-d/Source/Features/ESP/VisWorldDebugRender.hpp`
- Create: `project-d/Source/Features/ESP/VisWorldDebugRender.cpp`
- Create: `project-d/Tests/vis_world_debug_render_tests.cpp`

- [ ] **Step 1: Write the failing test**

```cpp
#include <iostream>
#include <string>
#include <vector>

#include "Features/ESP/VisWorldDebugRender.hpp"

namespace
{
    bool ExpectTrue(bool value, const std::string& message)
    {
        if (value)
            return true;
        std::cerr << "[FAIL] " << message << '\n';
        return false;
    }

    bool ExpectEqual(std::size_t actual, std::size_t expected, const std::string& message)
    {
        if (actual == expected)
            return true;
        std::cerr << "[FAIL] " << message << " expected=" << expected << " actual=" << actual << '\n';
        return false;
    }
}

int main()
{
    bool ok = true;

    const Vector3 camera{ 0.0f, 0.0f, 0.0f };
    const WorldDebugTriangle facing{
        { 10.0f, -10.0f, 10.0f },
        { 10.0f,  10.0f, 10.0f },
        { 10.0f,   0.0f,-10.0f }
    };
    const WorldDebugTriangle backFacing{
        facing.V0,
        facing.V2,
        facing.V1
    };

    ok &= ExpectTrue(
        !ShouldCullTriangleBackface(facing, camera),
        "front-facing triangle should remain visible");
    ok &= ExpectTrue(
        ShouldCullTriangleBackface(backFacing, camera),
        "reversed winding should be culled");

    std::vector<WorldDebugScreenTriangle> tris{
        { {0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 1.0f}, 25.0f, 0xAAAAAAAAu, 0xBBBBBBBBu },
        { {0.0f, 0.0f}, {2.0f, 0.0f}, {0.0f, 2.0f}, 400.0f, 0xCCCCCCCCu, 0xDDDDDDDDu },
        { {0.0f, 0.0f}, {3.0f, 0.0f}, {0.0f, 3.0f}, 100.0f, 0xEEEEEEEEu, 0xFFFFFFFFu }
    };

    SortWorldDebugTrianglesBackToFront(tris);

    ok &= ExpectEqual(tris.size(), 3u, "sort should keep all triangles");
    ok &= ExpectTrue(tris[0].DepthSqr == 400.0f, "furthest triangle should render first");
    ok &= ExpectTrue(tris[1].DepthSqr == 100.0f, "middle triangle should render second");
    ok &= ExpectTrue(tris[2].DepthSqr == 25.0f, "nearest triangle should render last");

    if (!ok)
        return 1;

    std::cout << "[PASS] vis_world_debug_render_tests\n";
    return 0;
}
```

- [ ] **Step 2: Run test to verify it fails**

Run:

```powershell
cl /nologo /std:c++20 /EHsc /utf-8 /Iproject-d\Source /Iproject-d\VisCheckCS2 project-d\Tests\vis_world_debug_render_tests.cpp project-d\Source\Features\ESP\VisWorldDebugRender.cpp /Fe:project-d\Build\tests\vis_world_debug_render_tests.exe
```

Expected: FAIL with missing file errors for `VisWorldDebugRender.hpp` and `VisWorldDebugRender.cpp`

- [ ] **Step 3: Write minimal implementation**

`project-d/Source/Features/ESP/VisWorldDebugRender.hpp`

```cpp
#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

#include "Math/Vector.hpp"

struct WorldDebugTriangle
{
    Vector3 V0{};
    Vector3 V1{};
    Vector3 V2{};
};

struct WorldDebugScreenTriangle
{
    Vector2 P0{};
    Vector2 P1{};
    Vector2 P2{};
    float DepthSqr = 0.0f;
    std::uint32_t FillColor = 0;
    std::uint32_t EdgeColor = 0;
};

bool ShouldCullTriangleBackface(const WorldDebugTriangle& tri, const Vector3& cameraPos);
float ComputeTriangleDepthSqr(const WorldDebugTriangle& tri, const Vector3& cameraPos);
void SortWorldDebugTrianglesBackToFront(std::vector<WorldDebugScreenTriangle>& triangles);
```

`project-d/Source/Features/ESP/VisWorldDebugRender.cpp`

```cpp
#include "VisWorldDebugRender.hpp"

#include <algorithm>

namespace
{
    Vector3 Subtract(const Vector3& lhs, const Vector3& rhs)
    {
        return { lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z };
    }

    Vector3 Cross(const Vector3& lhs, const Vector3& rhs)
    {
        return {
            lhs.y * rhs.z - lhs.z * rhs.y,
            lhs.z * rhs.x - lhs.x * rhs.z,
            lhs.x * rhs.y - lhs.y * rhs.x
        };
    }

    float Dot(const Vector3& lhs, const Vector3& rhs)
    {
        return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
    }
}

bool ShouldCullTriangleBackface(const WorldDebugTriangle& tri, const Vector3& cameraPos)
{
    const Vector3 edge01 = Subtract(tri.V1, tri.V0);
    const Vector3 edge02 = Subtract(tri.V2, tri.V0);
    const Vector3 normal = Cross(edge01, edge02);
    const Vector3 center{
        (tri.V0.x + tri.V1.x + tri.V2.x) / 3.0f,
        (tri.V0.y + tri.V1.y + tri.V2.y) / 3.0f,
        (tri.V0.z + tri.V1.z + tri.V2.z) / 3.0f
    };
    const Vector3 toCamera = Subtract(cameraPos, center);
    return Dot(normal, toCamera) <= 0.0f;
}

float ComputeTriangleDepthSqr(const WorldDebugTriangle& tri, const Vector3& cameraPos)
{
    const Vector3 center{
        (tri.V0.x + tri.V1.x + tri.V2.x) / 3.0f,
        (tri.V0.y + tri.V1.y + tri.V2.y) / 3.0f,
        (tri.V0.z + tri.V1.z + tri.V2.z) / 3.0f
    };
    const Vector3 delta = Subtract(center, cameraPos);
    return delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
}

void SortWorldDebugTrianglesBackToFront(std::vector<WorldDebugScreenTriangle>& triangles)
{
    std::sort(
        triangles.begin(),
        triangles.end(),
        [](const WorldDebugScreenTriangle& lhs, const WorldDebugScreenTriangle& rhs)
        {
            return lhs.DepthSqr > rhs.DepthSqr;
        });
}
```

- [ ] **Step 4: Run test to verify it passes**

Run:

```powershell
cl /nologo /std:c++20 /EHsc /utf-8 /Iproject-d\Source /Iproject-d\VisCheckCS2 project-d\Tests\vis_world_debug_render_tests.cpp project-d\Source\Features\ESP\VisWorldDebugRender.cpp /Fe:project-d\Build\tests\vis_world_debug_render_tests.exe
.\project-d\Build\tests\vis_world_debug_render_tests.exe
```

Expected: PASS with output `[PASS] vis_world_debug_render_tests`

- [ ] **Step 5: Commit**

```bash
git add project-d/Source/Features/ESP/VisWorldDebugRender.hpp project-d/Source/Features/ESP/VisWorldDebugRender.cpp project-d/Tests/vis_world_debug_render_tests.cpp
git commit -m "test: add world render geometry helpers"
```

### Task 2: Replace Line Snapshot With Triangle Snapshot

**Files:**
- Modify: `project-d/Source/Features/ESP/ESP.hpp`
- Modify: `project-d/Source/Features/ESP/ESP.cpp`
- Modify: `project-d/project-d.vcxproj`

- [ ] **Step 1: Write the failing integration change**

Add the new include and snapshot struct declarations in `project-d/Source/Features/ESP/ESP.hpp`:

```cpp
#include "VisWorldDebugRender.hpp"

struct VisDebugScreenTriangle
{
    Vector2 P0{};
    Vector2 P1{};
    Vector2 P2{};
    ImU32 FillColor = 0;
    ImU32 EdgeColor = 0;
    float DepthSqr = 0.0f;
};
```

Replace the old member:

```cpp
std::vector<VisDebugScreenLine> m_VisDebugOverlayLines{};
```

with:

```cpp
std::vector<VisDebugScreenTriangle> m_VisDebugOverlayTriangles{};
```

Expected compile break: `RenderVisCheckDebug`, `ClearVisCheckDebugOverlaySnapshot`, and `BuildVisCheckDebugOverlaySnapshot` still reference line-based structures.

- [ ] **Step 2: Run build to verify it fails**

Run:

```powershell
msbuild project-d\project-d.vcxproj /p:Configuration=Debug /p:Platform=x64
```

Expected: FAIL with unresolved `VisDebugScreenLine` / `m_VisDebugOverlayLines` references

- [ ] **Step 3: Write minimal implementation**

Add the helper files to `project-d/project-d.vcxproj`:

```xml
<ClInclude Include="Source\Features\ESP\VisWorldDebugRender.hpp" />
```

```xml
<ClCompile Include="Source\Features\ESP\VisWorldDebugRender.cpp" />
```

Update `project-d/Source/Features/ESP/ESP.cpp` include section:

```cpp
#include "VisWorldDebugRender.hpp"
```

Replace `RenderVisCheckDebug` with triangle-based drawing:

```cpp
void ESP::RenderVisCheckDebug(ImDrawList* drawList) const
{
    const bool visDebugEnabled = (config.DebugEnabled && config.DebugVisCheck) || config.Visuals.VisCheckDebug;
    if (!drawList || !visDebugEnabled)
        return;

    std::vector<VisDebugScreenTriangle> triangleSnapshot{};
    {
        std::lock_guard lock(m_VisDebugOverlayMutex);
        triangleSnapshot = m_VisDebugOverlayTriangles;
    }

    for (const VisDebugScreenTriangle& tri : triangleSnapshot)
    {
        const ImVec2 points[3] = {
            tri.P0.ToImVec2(),
            tri.P1.ToImVec2(),
            tri.P2.ToImVec2()
        };

        drawList->AddConvexPolyFilled(points, 3, tri.FillColor);
        drawList->AddPolyline(points, 3, tri.EdgeColor, ImDrawFlags_Closed, 1.0f);
    }
}
```

Replace `ClearVisCheckDebugOverlaySnapshot`:

```cpp
void ESP::ClearVisCheckDebugOverlaySnapshot()
{
    std::lock_guard lock(m_VisDebugOverlayMutex);
    m_VisDebugOverlayTriangles.clear();
}
```

Replace the triangle branch inside `BuildVisCheckDebugOverlaySnapshot()` with:

```cpp
std::vector<VisDebugScreenTriangle> nextTriangles{};
nextTriangles.reserve(static_cast<std::size_t>(maxItems));

for (const Candidate& candidate : candidates)
{
    const MapDebugTriangle& tri = triangleSnapshot[candidate.index];
    const WorldDebugTriangle worldTri{ tri.V0, tri.V1, tri.V2 };
    if (hasLocalEye && ShouldCullTriangleBackface(worldTri, localEye))
        continue;

    Vector2 s0{};
    Vector2 s1{};
    Vector2 s2{};
    if (!sdk.WorldToScreen(tri.V0, s0, viewMatrix) ||
        !sdk.WorldToScreen(tri.V1, s1, viewMatrix) ||
        !sdk.WorldToScreen(tri.V2, s2, viewMatrix))
    {
        continue;
    }

    const ImVec4 baseColor = [&]()
    {
        const float alpha = std::clamp(debugColor.w, 0.0f, 1.0f);
        if (tri.SourceKind == 2u)
            return ImVec4(1.0f, 0.20f, 0.20f, alpha);
        if (tri.SourceKind == 1u)
            return ImVec4(0.20f, 0.55f, 1.0f, alpha);
        return debugColor;
    }();

    const ImVec4 fillColor{
        baseColor.x,
        baseColor.y,
        baseColor.z,
        std::clamp(baseColor.w * 0.38f, 0.05f, 0.90f)
    };

    const ImVec4 edgeColor{
        baseColor.x,
        baseColor.y,
        baseColor.z,
        std::clamp(baseColor.w * 0.90f, 0.10f, 1.00f)
    };

    nextTriangles.push_back({
        s0,
        s1,
        s2,
        ImGui::ColorConvertFloat4ToU32(fillColor),
        ImGui::ColorConvertFloat4ToU32(edgeColor),
        hasLocalEye ? ComputeTriangleDepthSqr(worldTri, localEye) : candidate.distanceSqr
    });
}

SortWorldDebugTrianglesBackToFront(nextTriangles);

{
    std::lock_guard lock(m_VisDebugOverlayMutex);
    m_VisDebugOverlayTriangles = std::move(nextTriangles);
}
```

Delete the old box-edge line emission block entirely from `BuildVisCheckDebugOverlaySnapshot()`. “渲染世界”只使用地图三角面，不再混入 BVH 盒体线框。

- [ ] **Step 4: Run build to verify it passes**

Run:

```powershell
msbuild project-d\project-d.vcxproj /p:Configuration=Debug /p:Platform=x64
```

Expected: PASS with `project-d.exe` generated under `project-d\Build\Debug\`

- [ ] **Step 5: Commit**

```bash
git add project-d/Source/Features/ESP/ESP.hpp project-d/Source/Features/ESP/ESP.cpp project-d/Source/Features/ESP/VisWorldDebugRender.hpp project-d/Source/Features/ESP/VisWorldDebugRender.cpp project-d/project-d.vcxproj
git commit -m "feat: render vischeck geometry as world triangles"
```

### Task 3: Align UI And Config Naming With Render World

**Files:**
- Modify: `project-d/Source/Overlay/Overlay.cpp`
- Modify: `project-d/Source/Config/Structs.hpp`
- Modify: `project-d/Source/Config/Config.hpp`

- [ ] **Step 1: Write the failing UI wording change**

Update the UI labels in `project-d/Source/Overlay/Overlay.cpp`:

```cpp
ImAdd::CheckBox(Localization::Pick("Render World", "渲染世界"), &config.DebugVisCheck);
```

```cpp
ImGui::TextDisabled("%s", Localization::Pick("Render World uses full cached map triangles while debug is on.", "开启渲染世界后会使用缓存地图三角面进行整图覆盖。"));
```

```cpp
Localization::Pick("Render World Distance", "渲染世界距离")
Localization::Pick("Render World Max Triangles", "渲染世界最大三角数")
Localization::Pick("Render World Color", "渲染世界颜色")
```

Expected compile break: none, but the on-screen status text and config comments will still describe the feature as “VisCheck Debug Overlay”.

- [ ] **Step 2: Run build to verify current wording mismatch**

Run:

```powershell
rg -n "VisCheck Debug|VisDbg: Full|Debug now always renders full map overlay" project-d\Source -S
```

Expected: matches remain in `Structs.hpp`, `Config.hpp`, `ESP.cpp`, and `Overlay.cpp`

- [ ] **Step 3: Write minimal implementation**

Update the config comment in `project-d/Source/Config/Structs.hpp`:

```cpp
int VisCheckDebugMode = 1; // Deprecated legacy config key. Render World now always renders full cached map triangles.
```

Update default config writer in `project-d/Source/Config/Config.hpp` comment-adjacent strings only by preserving existing JSON keys and default values:

```cpp
j["Visuals"]["VisCheckDebug"] = false;
j["Visuals"]["VisCheckDebugMode"] = 1;
j["Visuals"]["VisCheckDebugMaxDistance"] = 3200.0f;
j["Visuals"]["VisCheckDebugMaxItems"] = 1400;
j["Visuals"]["VisCheckDebugColor"] = { 0.25f, 0.85f, 1.0f, 0.65f };
```

Update the on-screen debug status in `project-d/Source/Features/ESP/ESP.cpp`:

```cpp
std::snprintf(
    visDebugLine,
    sizeof(visDebugLine),
    Localization::Pick("WorldRender: tri=%zu", "渲染世界: 三角=%zu"),
    triangleCount);
```

This step intentionally keeps JSON field names unchanged and only changes user-facing naming. Do not introduce a second config key such as `RenderWorldEnabled`.

- [ ] **Step 4: Run build to verify it passes**

Run:

```powershell
msbuild project-d\project-d.vcxproj /p:Configuration=Debug /p:Platform=x64
rg -n "Render World|渲染世界|WorldRender: tri=" project-d\Source -S
```

Expected:
- MSBuild PASS
- `rg` shows updated labels in `Overlay.cpp` and status text in `ESP.cpp`

- [ ] **Step 5: Commit**

```bash
git add project-d/Source/Overlay/Overlay.cpp project-d/Source/Config/Structs.hpp project-d/Source/Config/Config.hpp project-d/Source/Features/ESP/ESP.cpp
git commit -m "chore: rename vis debug overlay to render world"
```

### Task 4: Verify End-To-End Rendering Behavior

**Files:**
- Modify: `project-d/Tests/vis_world_debug_render_tests.cpp`
- Modify: `project-d/Source/Features/ESP/ESP.cpp`

- [ ] **Step 1: Extend the failing test for color and stable order**

Append to `project-d/Tests/vis_world_debug_render_tests.cpp`:

```cpp
    ok &= ExpectTrue(tris[0].FillColor == 0xCCCCCCCCu, "sort should keep fill color paired with the furthest triangle");
    ok &= ExpectTrue(tris[2].EdgeColor == 0xBBBBBBBBu, "sort should keep edge color paired with the nearest triangle");
```

Expected failure if sorting code ever reorders depths without keeping triangle payload intact.

- [ ] **Step 2: Run tests and app build**

Run:

```powershell
cl /nologo /std:c++20 /EHsc /utf-8 /Iproject-d\Source /Iproject-d\VisCheckCS2 project-d\Tests\vis_world_debug_render_tests.cpp project-d\Source\Features\ESP\VisWorldDebugRender.cpp /Fe:project-d\Build\tests\vis_world_debug_render_tests.exe
.\project-d\Build\tests\vis_world_debug_render_tests.exe
msbuild project-d\project-d.vcxproj /p:Configuration=Debug /p:Platform=x64
```

Expected:
- test PASS
- project PASS

- [ ] **Step 3: Perform manual in-app verification**

Run the built executable and verify this checklist on a map with a loaded cache:

```text
1. 打开调试页并启用“渲染世界”
2. 画面出现半透明地图面片，而不是仅有线框
3. 远处结构先绘制，近处结构后绘制，没有整体乱序闪烁
4. 敌人骨骼与其他 ESP 元素仍然位于世界面片之上
5. 关闭“渲染世界”后，地图覆盖立即消失
```

If item 3 fails, adjust only fill alpha and backface剔除逻辑，不要重新引入 AABB 盒体线框。

- [ ] **Step 4: Commit**

```bash
git add project-d/Tests/vis_world_debug_render_tests.cpp project-d/Source/Features/ESP/ESP.cpp
git commit -m "test: verify render world ordering and payload stability"
```

## Self-Review

- Spec coverage:
  - 现有 `VisCheck` 三角面复用：Task 2
  - 从线框升级为半透明面片：Task 2
  - 深度排序与背面剔除：Task 1, Task 2
  - UI 命名改为“渲染世界”：Task 3
  - 端到端验证：Task 4
- Placeholder scan:
  - 已移除 `TODO/TBD/适当处理` 等空泛表述
  - 每个任务都包含具体文件、命令、预期结果
- Type consistency:
  - 统一使用 `WorldDebugTriangle`, `WorldDebugScreenTriangle`, `VisDebugScreenTriangle`
  - `m_VisDebugOverlayTriangles` 作为新的唯一渲染快照容器
