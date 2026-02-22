#pragma once

#include <map>
#include <string>
#include <unordered_map>

namespace Localization
{
    enum class Language : int
    {
        English = 0,
        Chinese = 1
    };

    inline Language CurrentLanguage = Language::English;

    inline bool IsChinese()
    {
        return CurrentLanguage == Language::Chinese;
    }

    inline const std::unordered_map<std::string, std::string>& Dictionary()
    {
        static const std::unordered_map<std::string, std::string> kDictionary = {
            { "Aim", "自瞄" },
            { "Visuals", "视觉" },
            { "ESP", "透视" },
            { "Config", "配置" },
            { "Info", "信息" },

            { "Aimbot", "自瞄" },
            { "Trigger", "扳机" },
            { "General", "通用" },
            { "Weapon Tabs", "武器页签" },
            { "Hotkeys", "热键" },
            { "Primary Key", "主热键" },
            { "Enable Secondary Key", "启用副热键" },
            { "Secondary Key", "副热键" },
            { "Draw FOV", "绘制FOV" },
            { "Dynamic FOV", "动态FOV" },
            { "Dynamic FOV Min Px", "动态FOV最小像素" },
            { "Aim Visible", "仅可见目标" },
            { "Aim Teammates", "瞄准队友" },
            { "Block Aimbot When Flashed", "致盲时禁用自瞄" },
            { "Deadzone (px)", "死区(px)" },
            { "Aim Parts (Bone Points)", "自瞄部位(骨骼点)" },
            { "Aimbot thread interval: 2ms (~500Hz).", "自瞄线程间隔: 2ms (~500Hz)。" },
            { "Profile FOV", "配置FOV" },
            { "Profile Smooth", "配置平滑" },
            { "Curve Strength", "曲线强度" },
            { "Profile Dynamic FOV", "配置动态FOV" },
            { "Dynamic Distance Scale", "动态距离系数" },
            { "Target Strategy", "目标策略" },
            { "Target Switch Delay (ms)", "切换目标延迟(ms)" },
            { "KMBOX not connected.", "KMBOX 未连接。" },
            { "Trigger only works while holding hotkey.", "扳机仅在按住热键时生效。" },
            { "Primary Trigger Key", "主扳机热键" },
            { "Enable Secondary Trigger Key", "启用副扳机热键" },
            { "Secondary Trigger Key", "副扳机热键" },
            { "Detection", "检测" },
            { "Trigger Detect Mode", "扳机检测模式" },
            { "Unified Hitbox Radius (px)", "统一碰撞体半径(px)" },
            { "Hitbox Scale", "碰撞体缩放" },
            { "Hitbox Add (px)", "碰撞体增量(px)" },
            { "Head Radius (px)", "头部半径(px)" },
            { "Safety", "安全" },
            { "Block Trigger When Flashed", "致盲时禁用扳机" },
            { "Reloading / non-gun is always blocked.", "换弹/非枪械时始终阻止触发。" },
            { "Hitbox Debug", "碰撞体调试" },
            { "Enable Trigger Hitbox Debug", "启用扳机碰撞体调试" },
            { "Head Sphere Debug", "头部球体调试" },
            { "ESP draws 3D box per bone segment.", "ESP 按骨骼段绘制3D盒体。" },
            { "Debug Thickness", "调试线宽" },
            { "Hitbox Color", "碰撞体颜色" },
            { "Active Hitbox Color", "激活碰撞体颜色" },
            { "Trigger thread interval: 2ms (~500Hz).", "扳机线程间隔: 2ms (~500Hz)。" },
            { "Pre Fire Delay (ms)", "预开火延迟(ms)" },
            { "Post Fire Interval (ms)", "后开火间隔(ms)" },
            { "Timeout Force Fire (ms)", "超时强制开火(ms)" },
            { "Trigger Parts (Bone Points)", "扳机部位(骨骼点)" },
            { "Special Pre Fire Delay (ms)", "特殊武器预开火延迟(ms)" },
            { "Special Post Fire Interval (ms)", "特殊武器后开火间隔(ms)" },
            { "Special Timeout Force Fire (ms)", "特殊武器超时强制开火(ms)" },
            { "Special Hold Fire (ms)", "特殊武器按住开火(ms)" },

            { "Watermark", "水印" },
            { "Background", "背景" },
            { "Visual", "视觉" },
            { "VSync", "垂直同步" },
            { "Team Check", "队伍检查" },
            { "Visible Check", "可见性检查" },
            { "Hitmarker", "命中标记" },
            { "Players", "玩家" },
            { "Name", "名称" },
            { "Health", "血量" },
            { "Armor", "护甲" },
            { "Money", "金钱" },
            { "Defuser", "拆弹钳" },
            { "Box", "方框" },
            { "Box Color", "方框颜色" },
            { "Box Color Visible", "可见方框颜色" },
            { "Weapon", "武器" },
            { "Bones", "骨骼" },
            { "Bones Color", "骨骼颜色" },
            { "Bones Color Visible", "可见骨骼颜色" },
            { "World", "世界" },
            { "C4", "C4" },
            { "C4 Card X", "C4卡片X" },
            { "C4 Card Y", "C4卡片Y" },
            { "Grenade Helper", "投掷物辅助" },
            { "Enable Grenade Helper", "启用投掷物辅助" },
            { "Filter By Current Grenade", "按当前手雷筛选" },
            { "Draw Stand Positions", "绘制站位点" },
            { "Draw Aim Targets", "绘制瞄点" },
            { "Manual Grenade Type Override", "手动覆盖手雷类型" },
            { "Grenade Type", "手雷类型" },
            { "Stand Tolerance", "站位容差" },
            { "Focus Radius (px)", "聚焦半径(px)" },
            { "Max Stand Draw Distance", "站位最大绘制距离" },
            { "Guide Line Threshold", "引导线阈值" },
            { "Top Hint Offset X", "顶部提示偏移X" },
            { "Top Hint Offset Y", "顶部提示偏移Y" },
            { "Unknown", "未知" },
            { "Scoped", "开镜" },
            { "Flashed", "致盲" },
            { "Kit", "拆弹钳" },
            { "Loaded", "已加载" },
            { "Not Found", "未找到" },
            { "Loading", "加载中" },
            { "Load Failed", "加载失败" },
            { "Deagle", "沙鹰" },
            { "Dual Berettas", "双持贝瑞塔" },
            { "Five-Seven", "FN57" },
            { "Glock-18", "格洛克18" },
            { "AK-47", "AK-47" },
            { "AUG", "AUG" },
            { "AWP", "AWP" },
            { "FAMAS", "法玛斯" },
            { "G3SG1", "G3SG1" },
            { "Galil AR", "加利尔AR" },
            { "M249", "M249" },
            { "M4A4", "M4A4" },
            { "MAC-10", "MAC-10" },
            { "P90", "P90" },
            { "MP5-SD", "MP5-SD" },
            { "UMP-45", "UMP-45" },
            { "XM1014", "XM1014" },
            { "PP-Bizon", "PP-野牛" },
            { "MAG-7", "MAG-7" },
            { "Negev", "内格夫" },
            { "Sawed-Off", "截短霰弹枪" },
            { "Tec-9", "Tec-9" },
            { "Zeus x27", "宙斯电击枪" },
            { "P2000", "P2000" },
            { "MP7", "MP7" },
            { "MP9", "MP9" },
            { "Nova", "新星" },
            { "P250", "P250" },
            { "SCAR-20", "SCAR-20" },
            { "SG 553", "SG 553" },
            { "SSG 08", "SSG 08" },
            { "Knife", "刀" },
            { "Flashbang", "闪光弹" },
            { "HE Grenade", "高爆手雷" },
            { "Smoke", "烟雾弹" },
            { "Molotov", "燃烧瓶" },
            { "Decoy", "诱饵弹" },
            { "Incendiary", "燃烧弹" },
            { "Healthshot", "治疗针" },
            { "Knife (T)", "刀(T)" },
            { "M4A1-S", "M4A1-S" },
            { "USP-S", "USP-S" },
            { "CZ75 Auto", "CZ75 自动手枪" },

            { "Pistol", "手枪" },
            { "SMG", "冲锋枪" },
            { "Shotgun", "霰弹枪" },
            { "Rifle", "步枪" },
            { "Sniper", "狙击枪" },
            { "LMG", "机枪" },
            { "Crosshair Closest", "准星最近" },
            { "Distance Closest", "距离最近" },
            { "Hybrid", "混合" },
            { "Bone Hitbox", "骨骼碰撞体" },
            { "Crosshair Entity", "准星实体" },
            { "Desert Eagle", "沙漠之鹰" },
            { "R8 Revolver", "R8 左轮" },

            { "Head", "头部" },
            { "Upper Chest", "上胸" },
            { "Torso", "躯干" },
            { "Pelvis", "骨盆" },
            { "Arms", "手臂" },
            { "Legs", "腿部" },
            { "Spine", "脊柱" },
            { "Chest", "胸部" },
            { "Neck", "颈部" },
            { "L Shoulder", "左肩" },
            { "L Elbow", "左肘" },
            { "L Hand", "左手" },
            { "R Shoulder", "右肩" },
            { "R Elbow", "右肘" },
            { "R Hand", "右手" },
            { "L Thigh", "左大腿" },
            { "L Knee", "左膝" },
            { "L Foot", "左脚" },
            { "R Thigh", "右大腿" },
            { "R Knee", "右膝" },
            { "R Foot", "右脚" },

            { "Configs", "配置" },
            { "Refresh", "刷新" },
            { "Config list", "配置列表" },
            { "No configs found", "未找到配置" },
            { "Config Name", "配置名" },
            { "Load", "加载" },
            { "Save", "保存" },
            { "Delete", "删除" },
            { "Import", "导入" },

            { "Hardware", "硬件" },
            { "Game", "游戏" },
            { "Client:", "客户端:" },
            { "Cheat", "功能" },
            { "DMA:", "DMA:" },
            { "KMBOX:", "KMBOX:" },
            { "Connected", "已连接" },
            { "Disconnected", "未连接" },
            { "Overlay FPS: %.2f", "叠加层帧率: %.2f" },
            { "Host INSERT: %s", "主机 INSERT: %s" },
            { "Host LMB: %s", "主机 左键: %s" },
            { "Host RMB: %s", "主机 右键: %s" },
            { "Host X1: %s", "主机 X1: %s" },
            { "Host X2: %s", "主机 X2: %s" },
            { "Down", "按下" },
            { "Up", "抬起" },
            { "Open folder", "打开目录" },
            { "Unload", "卸载" },
            { "Build: Developer", "版本: 开发版" },
            { "Expires: Never", "到期: 永不" },
            { "Language", "语言" },
            { "English", "英文" },
            { "Chinese", "中文" },

            { "None", "无" },
            { "Release then press key", "先松开再按键" },
            { "Press any key", "按任意键" },
            { "Always On", "始终启用" },
            { "On Toggle", "切换模式" },
            { "On Key", "按键触发" }
        };

        return kDictionary;
    }

    inline std::string TranslateImpl(const std::string& text)
    {
        if (!IsChinese() || text.empty())
            return text;

        const auto& dictionary = Dictionary();
        const size_t splitPos = text.find("##");
        const std::string visible = text.substr(0, splitPos);
        if (visible.empty())
            return text;

        const auto translatedIt = dictionary.find(visible);
        if (translatedIt == dictionary.end())
            return text;

        if (splitPos == std::string::npos)
            return translatedIt->second;

        return translatedIt->second + text.substr(splitPos);
    }

    inline const char* Localize(const char* text)
    {
        if (!text || !IsChinese())
            return text;

        static std::map<std::string, std::string> cache;

        const std::string key(text);
        const auto cacheIt = cache.find(key);
        if (cacheIt != cache.end())
            return cacheIt->second.c_str();

        auto [it, inserted] = cache.emplace(key, TranslateImpl(key));
        return it->second.c_str();
    }

    inline const char* Pick(const char* english, const char* chinese)
    {
        return IsChinese() ? chinese : english;
    }
}
