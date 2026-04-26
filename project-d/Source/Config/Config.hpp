#pragma once
#include "Structs.hpp"

namespace Config
{
    struct AppConfig {
        Structs::AimConfig Aim;
        Structs::KmboxConfig Kmbox;
        Structs::VisualsConfig Visuals;
        Structs::RadarConfig Radar;
        int Language = 0;
        bool DebugEnabled = false;
        bool DebugPerf = false;
        bool DebugTrigger = false;
        bool DebugVisCheck = false;
        bool DebugAutowall = false;
        bool DebugSpectatorList = false;
        bool DebugRadar = false;

        static AppConfig& Get()
        {
            static AppConfig instance;
            return instance;
        }

        bool Init(const std::string& configFile = "config.json")
        {
            std::string configDir = "configs";
            if (!std::filesystem::exists(configDir))
            {
                if (!std::filesystem::create_directory(configDir))
                {
                    LOG_ERROR("Could not create config directory: {}", configDir);
                    return false;
                }
            }

            std::string fullPath = configDir + "/" + configFile;
            if (!CheckFileExists(fullPath))
            {
                LOG_ERROR("Configuration initialization failed. If you did not have one, fill out the newly created file.");
                return false;
            }

            if (!LoadFromFile(fullPath))
            {
                LOG_ERROR("Couldn't load the specified config file");
                return false;
            }

            return true;
        }

        bool SaveToFile(const std::string& filename)
        {
            std::string fullPath = filename;
            LOG_INFO("Attempting to save config to file: {}", fullPath);
            nlohmann::json j;

            j["Aim"]["Trigger"] = Aim.Trigger;
            j["Aim"]["TriggerKey"] = Aim.TriggerKey;
            j["Aim"]["TriggerKeyMode"] = Aim.TriggerKeyMode;
            j["Aim"]["TriggerDelay"] = Aim.TriggerDelay;
            j["Aim"]["TriggerSecondKeyEnabled"] = Aim.TriggerSecondKeyEnabled;
            j["Aim"]["TriggerSecondKey"] = Aim.TriggerSecondKey;
            j["Aim"]["TriggerSecondKeyMode"] = Aim.TriggerSecondKeyMode;
            j["Aim"]["TriggerMinIntervalMs"] = Aim.TriggerMinIntervalMs;
            j["Aim"]["TriggerDetectMode"] = Aim.TriggerDetectMode;
            j["Aim"]["TriggerUnifiedHitboxRadiusPx"] = Aim.TriggerUnifiedHitboxRadiusPx;
            j["Aim"]["TriggerHitboxScale"] = Aim.TriggerHitboxScale;
            j["Aim"]["TriggerHitboxAddPx"] = Aim.TriggerHitboxAddPx;
            j["Aim"]["TriggerHeadRadiusPx"] = Aim.TriggerHeadRadiusPx;
            j["Aim"]["TriggerHeadScale"] = Aim.TriggerHeadScale;
            j["Aim"]["TriggerTorsoScale"] = Aim.TriggerTorsoScale;
            j["Aim"]["TriggerArmsScale"] = Aim.TriggerArmsScale;
            j["Aim"]["TriggerLegsScale"] = Aim.TriggerLegsScale;
            j["Aim"]["TriggerHeadSphereDebug"] = Aim.TriggerHeadSphereDebug;
            j["Aim"]["TriggerBoneMask"] = Aim.TriggerBoneMask;
            j["Aim"]["TriggerHitGroupMask"] = Aim.TriggerHitGroupMask;
            j["Aim"]["TriggerHitboxDebug"] = Aim.TriggerHitboxDebug;
            j["Aim"]["TriggerHitboxDebugColor"] = { Aim.TriggerHitboxDebugColor.x, Aim.TriggerHitboxDebugColor.y, Aim.TriggerHitboxDebugColor.z, Aim.TriggerHitboxDebugColor.w };
            j["Aim"]["TriggerHitboxDebugActiveColor"] = { Aim.TriggerHitboxDebugActiveColor.x, Aim.TriggerHitboxDebugActiveColor.y, Aim.TriggerHitboxDebugActiveColor.z, Aim.TriggerHitboxDebugActiveColor.w };
            j["Aim"]["TriggerHitboxDebugThickness"] = Aim.TriggerHitboxDebugThickness;
            j["Aim"]["Flick"] = Aim.Flick;
            j["Aim"]["FlickKey"] = Aim.FlickKey;
            j["Aim"]["FlickKeyMode"] = Aim.FlickKeyMode;
            j["Aim"]["BlockTriggerWhenFlashed"] = Aim.BlockTriggerWhenFlashed;
            j["Aim"]["BlockAimbotWhenFlashed"] = Aim.BlockAimbotWhenFlashed;

            j["Aim"]["Aimbot"] = Aim.Aimbot;

            j["Aim"]["DrawFov"] = Aim.DrawFov;
            j["Aim"]["AimbotFovColor"] = { Aim.AimbotFovColor.x, Aim.AimbotFovColor.y, Aim.AimbotFovColor.z, Aim.AimbotFovColor.w };

            j["Aim"]["AimFriendly"] = Aim.AimFriendly;
            j["Aim"]["AimVisible"] = Aim.AimVisible;

            j["Aim"]["AimbotKey"] = Aim.AimbotKey;
            j["Aim"]["AimbotKeyMode"] = Aim.AimbotKeyMode;
            j["Aim"]["AimbotSecondKeyEnabled"] = Aim.AimbotSecondKeyEnabled;
            j["Aim"]["AimbotSecondKey"] = Aim.AimbotSecondKey;
            j["Aim"]["AimbotSecondKeyMode"] = Aim.AimbotSecondKeyMode;
            j["Aim"]["AimbotHitGroupMask"] = Aim.AimbotHitGroupMask;

            j["Aim"]["DeadzonePx"] = Aim.DeadzonePx;

            j["Aim"]["AimbotFov"] = Aim.AimbotFov;
            j["Aim"]["AimbotSmooth"] = Aim.AimbotSmooth;
            j["Aim"]["WeaponProfileEditorIndex"] = Aim.WeaponProfileEditorIndex;
            j["Aim"]["TriggerProfileEditorIndex"] = Aim.TriggerProfileEditorIndex;
            j["Aim"]["FlickProfileEditorIndex"] = Aim.FlickProfileEditorIndex;
            j["Aim"]["FlickSpecialEditorIndex"] = Aim.FlickSpecialEditorIndex;
            j["Aim"]["TriggerSpecialEditorIndex"] = Aim.TriggerSpecialEditorIndex;

            auto writeWeaponProfile = [&](const char* name, const Structs::AimWeaponProfile& profile)
            {
                nlohmann::json& out = j["Aim"]["WeaponProfiles"][name];
                out["Fov"] = profile.Fov;
                out["Smooth"] = profile.Smooth;
                out["SprayAxisStrengthX"] = profile.SprayAxisStrengthX;
                out["SprayAxisStrengthY"] = profile.SprayAxisStrengthY;
                out["SprayAxisMaxStepX"] = profile.SprayAxisMaxStepX;
                out["SprayAxisMaxStepY"] = profile.SprayAxisMaxStepY;
                out["SprayAxisDeadzoneX"] = profile.SprayAxisDeadzoneX;
                out["SprayAxisDeadzoneY"] = profile.SprayAxisDeadzoneY;
                out["CurveStrength"] = profile.CurveStrength;
                out["BoneMask"] = profile.BoneMask;
                out["TargetStrategy"] = profile.TargetStrategy;
                out["TargetSwitchDelayMs"] = profile.TargetSwitchDelayMs;
            };

            writeWeaponProfile("Pistol", Aim.WeaponProfiles[Structs::AimWeapon_Pistol]);
            writeWeaponProfile("Smg", Aim.WeaponProfiles[Structs::AimWeapon_Smg]);
            writeWeaponProfile("Shotgun", Aim.WeaponProfiles[Structs::AimWeapon_Shotgun]);
            writeWeaponProfile("Rifle", Aim.WeaponProfiles[Structs::AimWeapon_Rifle]);
            writeWeaponProfile("Sniper", Aim.WeaponProfiles[Structs::AimWeapon_Sniper]);
            writeWeaponProfile("Lmg", Aim.WeaponProfiles[Structs::AimWeapon_Lmg]);

            auto writeTriggerProfile = [&](const char* name, const Structs::TriggerWeaponProfile& profile)
            {
                nlohmann::json& out = j["Aim"]["TriggerProfiles"][name];
                out["HitboxRadiusPx"] = profile.HitboxRadiusPx;
                out["PreFireDelayMs"] = profile.PreFireDelayMs;
                out["PostFireIntervalMs"] = profile.PostFireIntervalMs;
                out["TimeoutForceFireMs"] = profile.TimeoutForceFireMs;
                out["BoneMask"] = profile.BoneMask;
            };

            writeTriggerProfile("Pistol", Aim.TriggerProfiles[Structs::AimWeapon_Pistol]);
            writeTriggerProfile("Smg", Aim.TriggerProfiles[Structs::AimWeapon_Smg]);
            writeTriggerProfile("Shotgun", Aim.TriggerProfiles[Structs::AimWeapon_Shotgun]);
            writeTriggerProfile("Rifle", Aim.TriggerProfiles[Structs::AimWeapon_Rifle]);
            writeTriggerProfile("Sniper", Aim.TriggerProfiles[Structs::AimWeapon_Sniper]);
            writeTriggerProfile("Lmg", Aim.TriggerProfiles[Structs::AimWeapon_Lmg]);

            auto writeFlickProfile = [&](const char* name, const Structs::FlickWeaponProfile& profile)
            {
                nlohmann::json& out = j["Aim"]["FlickProfiles"][name];
                out["Fov"] = profile.Fov;
                out["Smooth"] = profile.Smooth;
                out["FollowSmooth"] = profile.FollowSmooth;
                out["MaxFlickTimeMs"] = profile.MaxFlickTimeMs;
                out["RestartIntervalMs"] = profile.RestartIntervalMs;
                out["BoneMask"] = profile.BoneMask;
                out["DynamicFovEnabled"] = profile.DynamicFovEnabled;
                out["AutowallEnabled"] = profile.AutowallEnabled;
                out["AutowallKillshotOnly"] = profile.AutowallKillshotOnly;
            };

            writeFlickProfile("Pistol", Aim.FlickProfiles[Structs::AimWeapon_Pistol]);
            writeFlickProfile("Smg", Aim.FlickProfiles[Structs::AimWeapon_Smg]);
            writeFlickProfile("Shotgun", Aim.FlickProfiles[Structs::AimWeapon_Shotgun]);
            writeFlickProfile("Rifle", Aim.FlickProfiles[Structs::AimWeapon_Rifle]);
            writeFlickProfile("Sniper", Aim.FlickProfiles[Structs::AimWeapon_Sniper]);
            writeFlickProfile("Lmg", Aim.FlickProfiles[Structs::AimWeapon_Lmg]);

            auto writeFlickSpecialProfile = [&](const char* name, const Structs::FlickWeaponProfile& profile)
            {
                nlohmann::json& out = j["Aim"]["FlickSpecialProfiles"][name];
                out["Fov"] = profile.Fov;
                out["Smooth"] = profile.Smooth;
                out["FollowSmooth"] = profile.FollowSmooth;
                out["MaxFlickTimeMs"] = profile.MaxFlickTimeMs;
                out["RestartIntervalMs"] = profile.RestartIntervalMs;
                out["BoneMask"] = profile.BoneMask;
                out["DynamicFovEnabled"] = profile.DynamicFovEnabled;
                out["AutowallEnabled"] = profile.AutowallEnabled;
                out["AutowallKillshotOnly"] = profile.AutowallKillshotOnly;
            };

            writeFlickSpecialProfile("DesertEagle", Aim.FlickSpecialProfiles[Structs::TriggerSpecial_Deagle]);
            writeFlickSpecialProfile("R8Revolver", Aim.FlickSpecialProfiles[Structs::TriggerSpecial_Revolver]);

            auto writeTriggerSpecial = [&](const char* name, const Structs::TriggerSpecialProfile& profile)
            {
                nlohmann::json& out = j["Aim"]["TriggerSpecialProfiles"][name];
                out["HitboxRadiusPx"] = profile.HitboxRadiusPx;
                out["PreFireDelayMs"] = profile.PreFireDelayMs;
                out["PostFireIntervalMs"] = profile.PostFireIntervalMs;
                out["TimeoutForceFireMs"] = profile.TimeoutForceFireMs;
                out["HoldFireMs"] = profile.HoldFireMs;
                out["BoneMask"] = profile.BoneMask;
            };

            writeTriggerSpecial("DesertEagle", Aim.TriggerSpecialProfiles[Structs::TriggerSpecial_Deagle]);
            writeTriggerSpecial("R8Revolver", Aim.TriggerSpecialProfiles[Structs::TriggerSpecial_Revolver]);

            j["Kmbox"]["Enabled"] = Kmbox.Enabled;
            j["Kmbox"]["Ip"] = Kmbox.Ip;
            j["Kmbox"]["Port"] = Kmbox.Port;
            j["Kmbox"]["Uuid"] = Kmbox.Uuid;

            j["Visuals"]["Enabled"] = Visuals.Enabled;
            j["Visuals"]["TeamCheck"] = Visuals.TeamCheck;
            j["Visuals"]["VisibleCheck"] = Visuals.VisibleCheck;
            j["Visuals"]["VisCheckDebug"] = Visuals.VisCheckDebug;
            j["Visuals"]["Legit"] = Visuals.Legit;
            j["Visuals"]["VisCheckDebugMode"] = Visuals.VisCheckDebugMode;
            j["Visuals"]["VisCheckDebugMaxDistance"] = Visuals.VisCheckDebugMaxDistance;
            j["Visuals"]["VisCheckDebugMaxItems"] = Visuals.VisCheckDebugMaxItems;
            j["Visuals"]["VisCheckDebugColor"] = {
                Visuals.VisCheckDebugColor.x,
                Visuals.VisCheckDebugColor.y,
                Visuals.VisCheckDebugColor.z,
                Visuals.VisCheckDebugColor.w
            };

            j["Visuals"]["Background"] = Visuals.Background;
            j["Visuals"]["Hitmarker"] = Visuals.Hitmarker;
            j["Visuals"]["HitmarkerColor"] = { Visuals.HitmarkerColor.x, Visuals.HitmarkerColor.y, Visuals.HitmarkerColor.z, Visuals.HitmarkerColor.w };
            j["Visuals"]["Watermark"] = Visuals.Watermark;
            j["Visuals"]["WatermarkColor"] = { Visuals.WatermarkColor.x, Visuals.WatermarkColor.y, Visuals.WatermarkColor.z, Visuals.WatermarkColor.w };

            j["Visuals"]["Name"] = Visuals.Name;
            j["Visuals"]["NameColor"] = { Visuals.NameColor.x, Visuals.NameColor.y, Visuals.NameColor.z, Visuals.NameColor.w };
            j["Visuals"]["Box"] = Visuals.Box;
            j["Visuals"]["BoxColor"] = { Visuals.BoxColor.x, Visuals.BoxColor.y, Visuals.BoxColor.z, Visuals.BoxColor.w };
            j["Visuals"]["BoxColorVisible"] = { Visuals.BoxColorVisible.x, Visuals.BoxColorVisible.y, Visuals.BoxColorVisible.z, Visuals.BoxColorVisible.w };
            j["Visuals"]["Health"] = Visuals.Health;
            j["Visuals"]["Armor"] = Visuals.Armor;
            j["Visuals"]["Money"] = Visuals.Money;
            j["Visuals"]["MoneyColor"] = { Visuals.MoneyColor.x, Visuals.MoneyColor.y, Visuals.MoneyColor.z, Visuals.MoneyColor.w };
            j["Visuals"]["Weapon"] = Visuals.Weapon;
            j["Visuals"]["WeaponColor"] = { Visuals.WeaponColor.x, Visuals.WeaponColor.y, Visuals.WeaponColor.z, Visuals.WeaponColor.w };
            j["Visuals"]["Bones"] = Visuals.Bones;
            j["Visuals"]["BonesColor"] = { Visuals.BonesColor.x, Visuals.BonesColor.y, Visuals.BonesColor.z, Visuals.BonesColor.w };
            j["Visuals"]["BonesColorVisible"] = { Visuals.BonesColorVisible.x, Visuals.BonesColorVisible.y, Visuals.BonesColorVisible.z, Visuals.BonesColorVisible.w };
            j["Visuals"]["SoundEsp"] = Visuals.SoundEsp;
            j["Visuals"]["C4"] = Visuals.C4;
            j["Visuals"]["C4Color"] = { Visuals.C4Color.x, Visuals.C4Color.y, Visuals.C4Color.z, Visuals.C4Color.w };
            j["Visuals"]["C4PanelPosX"] = Visuals.C4PanelPosX;
            j["Visuals"]["C4PanelPosY"] = Visuals.C4PanelPosY;
            j["Visuals"]["SpectatorList"] = Visuals.SpectatorList;
            j["Visuals"]["SpectatorListColor"] = { Visuals.SpectatorListColor.x, Visuals.SpectatorListColor.y, Visuals.SpectatorListColor.z, Visuals.SpectatorListColor.w };
            j["Visuals"]["SpectatorListPanelPosX"] = Visuals.SpectatorListPanelPosX;
            j["Visuals"]["SpectatorListPanelPosY"] = Visuals.SpectatorListPanelPosY;
            j["Visuals"]["Defuser"] = Visuals.Defuser;
            j["Visuals"]["GrenadeHelper"] = Visuals.GrenadeHelper;
            j["Visuals"]["GrenadeHelperFilterByWeapon"] = Visuals.GrenadeHelperFilterByWeapon;
            j["Visuals"]["GrenadeHelperDrawStand"] = Visuals.GrenadeHelperDrawStand;
            j["Visuals"]["GrenadeHelperDrawAim"] = Visuals.GrenadeHelperDrawAim;
            j["Visuals"]["GrenadeHelperManualTypeOverride"] = Visuals.GrenadeHelperManualTypeOverride;
            j["Visuals"]["GrenadeHelperManualType"] = Visuals.GrenadeHelperManualType;
            j["Visuals"]["GrenadeHelperStandTolerance"] = Visuals.GrenadeHelperStandTolerance;
            j["Visuals"]["GrenadeHelperFocusRadius"] = Visuals.GrenadeHelperFocusRadius;
            j["Visuals"]["GrenadeHelperMaxStandDrawDistance"] = Visuals.GrenadeHelperMaxStandDrawDistance;
            j["Visuals"]["GrenadeHelperLooseGuideDistance"] = Visuals.GrenadeHelperLooseGuideDistance;
            j["Visuals"]["GrenadeHelperTopHintOffsetX"] = Visuals.GrenadeHelperTopHintOffsetX;
            j["Visuals"]["GrenadeHelperTopHintOffsetY"] = Visuals.GrenadeHelperTopHintOffsetY;
            j["Visuals"]["GrenadeHelperStandColor"] = { Visuals.GrenadeHelperStandColor.x, Visuals.GrenadeHelperStandColor.y, Visuals.GrenadeHelperStandColor.z, Visuals.GrenadeHelperStandColor.w };
            j["Visuals"]["GrenadeHelperAimColor"] = { Visuals.GrenadeHelperAimColor.x, Visuals.GrenadeHelperAimColor.y, Visuals.GrenadeHelperAimColor.z, Visuals.GrenadeHelperAimColor.w };
            j["Visuals"]["GrenadeHelperGuideLineColor"] = { Visuals.GrenadeHelperGuideLineColor.x, Visuals.GrenadeHelperGuideLineColor.y, Visuals.GrenadeHelperGuideLineColor.z, Visuals.GrenadeHelperGuideLineColor.w };
            j["Visuals"]["GrenadeHelperFontColor"] = { Visuals.GrenadeHelperFontColor.x, Visuals.GrenadeHelperFontColor.y, Visuals.GrenadeHelperFontColor.z, Visuals.GrenadeHelperFontColor.w };
            j["Visuals"]["GrenadeHelperFontSize"] = Visuals.GrenadeHelperFontSize;
            j["Visuals"]["GrenadeHelperTopHintColor"] = { Visuals.GrenadeHelperTopHintColor.x, Visuals.GrenadeHelperTopHintColor.y, Visuals.GrenadeHelperTopHintColor.z, Visuals.GrenadeHelperTopHintColor.w };
            j["Visuals"]["GrenadeHelperTopHintFontSize"] = Visuals.GrenadeHelperTopHintFontSize;

            j["Radar"]["Enabled"] = Radar.Enabled;
            j["Radar"]["Host"] = Radar.Host;
            j["Radar"]["StaticPort"] = Radar.StaticPort;
            j["Radar"]["IngestPort"] = Radar.IngestPort;
            j["Radar"]["PublishIntervalMs"] = Radar.PublishIntervalMs;
            j["Radar"]["HttpTimeoutMs"] = Radar.HttpTimeoutMs;
            j["Radar"]["ReconnectBaseMs"] = Radar.ReconnectBaseMs;
            j["Radar"]["ReconnectMaxMs"] = Radar.ReconnectMaxMs;

            j["Info"]["Language"] = std::clamp(Language, 0, 1);
            j["Info"]["DebugEnabled"] = DebugEnabled;
            j["Info"]["DebugPerf"] = DebugPerf;
            j["Info"]["DebugTrigger"] = DebugTrigger;
            j["Info"]["DebugVisCheck"] = DebugVisCheck;
            j["Info"]["DebugAutowall"] = DebugAutowall;
            j["Info"]["DebugSpectatorList"] = DebugSpectatorList;
            j["Info"]["DebugRadar"] = DebugRadar;

            std::ofstream file(fullPath);
            if (file.is_open())
            {
                file << j.dump(4);  // Write JSON with pretty print
                file.close();
                LOG_INFO("Config saved successfully to file: {}", fullPath);

                return true;
            }
            else
            {
                LOG_ERROR("Failed to open file for saving: {}", fullPath);
            }

            return false;
        }

        bool DeleteConfigFile(const std::string& filename)
        {
            std::string fullPath = filename;
            LOG_INFO("Attempting to delete config file: {}", fullPath);
            if (std::filesystem::remove(fullPath))
            {
                LOG_INFO("Config file deleted successfully: {}", fullPath);
                return true;
            }
            else
            {
                LOG_ERROR("Failed to delete config file: {}", fullPath);
            }
            return false;
        }

        std::vector<std::string> ListConfigs(const std::string& directory)
        {
            std::vector<std::string> configs;
            for (const auto& entry : std::filesystem::directory_iterator(directory))
            {
                if (entry.is_regular_file() && entry.path().extension() == ".json")
                {
                    configs.push_back(entry.path().filename().string());
                }
            }
            return configs;
        }

        bool LoadFromFile(const std::string& filename)
        {
            std::ifstream file(filename);
            if (file.is_open())
            {
                nlohmann::json j;
                try {
                    file >> j;
                }
                catch (nlohmann::json::parse_error& e) {
                    LOG_ERROR("JSON parse error in config file: {}", e.what());
                    return false;
                }

                LoadConfigSection(j, "Aim", Aim);
                LoadConfigSection(j, "Kmbox", Kmbox);
                LoadConfigSection(j, "Visuals", Visuals);
                LoadConfigSection(j, "Radar", Radar);
                LoadWeaponProfiles(j);
                DebugVisCheck = Visuals.VisCheckDebug;
                if (j.contains("Info") && j["Info"].is_object())
                {
                    const auto& info = j["Info"];
                    const bool hasDebugVisCheck = info.contains("DebugVisCheck");
                    if (info.contains("Language"))
                    {
                        if (info["Language"].is_number_integer())
                            Language = info["Language"].get<int>();
                        else if (info["Language"].is_number())
                            Language = static_cast<int>(info["Language"].get<double>());
                    }

                    auto readInfoBool = [&](const char* key, bool& outValue)
                    {
                        if (!info.contains(key))
                            return;

                        const auto& node = info[key];
                        if (node.is_boolean())
                            outValue = node.get<bool>();
                        else if (node.is_number_integer())
                            outValue = node.get<int>() != 0;
                        else if (node.is_number())
                            outValue = node.get<double>() != 0.0;
                    };

                    readInfoBool("DebugEnabled", DebugEnabled);
                    readInfoBool("DebugPerf", DebugPerf);
                    readInfoBool("DebugTrigger", DebugTrigger);
                    readInfoBool("DebugVisCheck", DebugVisCheck);
                    readInfoBool("DebugAutowall", DebugAutowall);
                    readInfoBool("DebugSpectatorList", DebugSpectatorList);
                    readInfoBool("DebugRadar", DebugRadar);
                    if (!hasDebugVisCheck)
                        DebugVisCheck = Visuals.VisCheckDebug;
                }
                Language = std::clamp(Language, 0, 1);

                LOG_INFO("Loaded config from file: {}", filename);
                return true;
            }
            else
            {
                LOG_ERROR("Could not open config file: {}", filename);
                return false;
            }
        }

        bool LoadFromClipboard()
        {
            if (OpenClipboard(nullptr))
            {
                HANDLE clipboardData = GetClipboardData(CF_TEXT);
                if (clipboardData)
                {
                    char* clipboardText = static_cast<char*>(GlobalLock(clipboardData));
                    if (clipboardText)
                    {
                        try
                        {
                            nlohmann::json j = nlohmann::json::parse(clipboardText);
                            LoadConfigSection(j, "Aim", Aim);
                            LoadConfigSection(j, "Kmbox", Kmbox);
                            LoadConfigSection(j, "Visuals", Visuals);
                            LoadConfigSection(j, "Radar", Radar);
                            LoadWeaponProfiles(j);
                            DebugVisCheck = Visuals.VisCheckDebug;
                            if (j.contains("Info") && j["Info"].is_object())
                            {
                                const auto& info = j["Info"];
                                const bool hasDebugVisCheck = info.contains("DebugVisCheck");
                                if (info.contains("Language"))
                                {
                                    if (info["Language"].is_number_integer())
                                        Language = info["Language"].get<int>();
                                    else if (info["Language"].is_number())
                                        Language = static_cast<int>(info["Language"].get<double>());
                                }

                                auto readInfoBool = [&](const char* key, bool& outValue)
                                {
                                    if (!info.contains(key))
                                        return;

                                    const auto& node = info[key];
                                    if (node.is_boolean())
                                        outValue = node.get<bool>();
                                    else if (node.is_number_integer())
                                        outValue = node.get<int>() != 0;
                                    else if (node.is_number())
                                        outValue = node.get<double>() != 0.0;
                                };

                                readInfoBool("DebugEnabled", DebugEnabled);
                                readInfoBool("DebugPerf", DebugPerf);
                                readInfoBool("DebugTrigger", DebugTrigger);
                                readInfoBool("DebugVisCheck", DebugVisCheck);
                                readInfoBool("DebugAutowall", DebugAutowall);
                                readInfoBool("DebugSpectatorList", DebugSpectatorList);
                                readInfoBool("DebugRadar", DebugRadar);
                                if (!hasDebugVisCheck)
                                    DebugVisCheck = Visuals.VisCheckDebug;
                            }
                            Language = std::clamp(Language, 0, 1);
                            LOG_INFO("Loaded config from clipboard");
                            GlobalUnlock(clipboardData);
                            CloseClipboard();
                            return true;
                        }
                        catch (nlohmann::json::parse_error& e)
                        {
                            LOG_ERROR("JSON parse error in clipboard data: {}", e.what());
                        }
                    }
                    GlobalUnlock(clipboardData);
                }
                CloseClipboard();
            }
            return false;
        }

    private:
        bool CheckFileExists(const std::string& filename)
        {
            if (!std::filesystem::exists(filename))
            {
                if (CreateDefaultConfigFile(filename))
                {
                    LOG_WARN("Created default config file: {}", filename);
                    return true;
                }
                else
                {
                    LOG_ERROR("Could not create config file: {}", filename);
                    return false;
                }
            }
            return true;  // File exists
        }

        bool CreateDefaultConfigFile(const std::string& filename)
        {
            std::ofstream file(filename);
            if (file.is_open())
            {
                nlohmann::json j;

                j["Kmbox"]["Enabled"] = false;
                j["Kmbox"]["Ip"] = "";
                j["Kmbox"]["Port"] = 0;
                j["Kmbox"]["Uuid"] = "";

                j["Aim"]["Trigger"] = false;
                j["Aim"]["TriggerKey"] = 0;
                j["Aim"]["TriggerKeyMode"] = 1;
                j["Aim"]["TriggerDelay"] = 0;
                j["Aim"]["TriggerSecondKeyEnabled"] = false;
                j["Aim"]["TriggerSecondKey"] = 0;
                j["Aim"]["TriggerSecondKeyMode"] = 1;
                j["Aim"]["TriggerMinIntervalMs"] = 35;
                j["Aim"]["TriggerDetectMode"] = Structs::TriggerDetect_BoneHitbox;
                j["Aim"]["TriggerUnifiedHitboxRadiusPx"] = 4.5f;
                j["Aim"]["TriggerHitboxScale"] = 1.0f;
                j["Aim"]["TriggerHitboxAddPx"] = 0.0f;
                j["Aim"]["TriggerHeadRadiusPx"] = 9.0f;
                j["Aim"]["TriggerHeadScale"] = 1.15f;
                j["Aim"]["TriggerTorsoScale"] = 1.20f;
                j["Aim"]["TriggerArmsScale"] = 0.90f;
                j["Aim"]["TriggerLegsScale"] = 1.00f;
                j["Aim"]["TriggerHeadSphereDebug"] = true;
                j["Aim"]["TriggerBoneMask"] = Structs::AimAllBoneMask;
                j["Aim"]["TriggerHitGroupMask"] =
                    Structs::AimHit_Head |
                    Structs::AimHit_UpperChest |
                    Structs::AimHit_Torso |
                    Structs::AimHit_Pelvis |
                    Structs::AimHit_Arms |
                    Structs::AimHit_Legs;
                j["Aim"]["TriggerHitboxDebug"] = false;
                j["Aim"]["TriggerHitboxDebugColor"] = { 1.0f, 0.55f, 0.2f, 0.9f };
                j["Aim"]["TriggerHitboxDebugActiveColor"] = { 0.2f, 1.0f, 0.35f, 0.95f };
                j["Aim"]["TriggerHitboxDebugThickness"] = 1.0f;
                j["Aim"]["Flick"] = false;
                j["Aim"]["FlickKey"] = 0;
                j["Aim"]["FlickKeyMode"] = 1;
                j["Aim"]["BlockTriggerWhenFlashed"] = false;
                j["Aim"]["BlockAimbotWhenFlashed"] = false;

                j["Aim"]["Aimbot"] = false;

                j["Aim"]["DrawFov"] = false;
                j["Aim"]["AimbotFovColor"] = { 1.0f, 1.0f, 1.0f, 1.0f };

                j["Aim"]["AimFriendly"] = false;
                j["Aim"]["AimVisible"] = false;

                j["Aim"]["AimbotKey"] = 0;
                j["Aim"]["AimbotKeyMode"] = 1;
                j["Aim"]["AimbotSecondKeyEnabled"] = false;
                j["Aim"]["AimbotSecondKey"] = 0;
                j["Aim"]["AimbotSecondKeyMode"] = 1;
                j["Aim"]["AimbotHitGroupMask"] =
                    Structs::AimHit_Head |
                    Structs::AimHit_UpperChest |
                    Structs::AimHit_Torso;
                j["Aim"]["DeadzonePx"] = 1.2f;

                j["Aim"]["AimbotFov"] = 6.5f;
                j["Aim"]["AimbotSmooth"] = 16.0f;
                j["Aim"]["WeaponProfileEditorIndex"] = 0;
                j["Aim"]["TriggerProfileEditorIndex"] = 0;
                j["Aim"]["FlickProfileEditorIndex"] = 0;
                j["Aim"]["FlickSpecialEditorIndex"] = 0;
                j["Aim"]["TriggerSpecialEditorIndex"] = 0;

                auto writeDefaultWeaponProfile = [&](
                    const char* name,
                    float fov,
                    float smooth,
                    float axisStrengthX,
                    float axisStrengthY,
                    float axisMaxStepX,
                    float axisMaxStepY,
                    float axisDeadzoneX,
                    float axisDeadzoneY,
                    float curve,
                    std::uint64_t boneMask,
                    int strategy,
                    int switchDelay)
                {
                    nlohmann::json& p = j["Aim"]["WeaponProfiles"][name];
                    p["Fov"] = fov;
                    p["Smooth"] = smooth;
                    p["SprayAxisStrengthX"] = axisStrengthX;
                    p["SprayAxisStrengthY"] = axisStrengthY;
                    p["SprayAxisMaxStepX"] = axisMaxStepX;
                    p["SprayAxisMaxStepY"] = axisMaxStepY;
                    p["SprayAxisDeadzoneX"] = axisDeadzoneX;
                    p["SprayAxisDeadzoneY"] = axisDeadzoneY;
                    p["CurveStrength"] = curve;
                    p["BoneMask"] = boneMask;
                    p["TargetStrategy"] = strategy;
                    p["TargetSwitchDelayMs"] = switchDelay;
                };

                writeDefaultWeaponProfile("Pistol", 5.5f, 14.0f, 1.30f, 1.55f, 6.0f, 8.0f, 0.08f, 0.06f, 0.18f, Structs::AimDefaultAimbotBoneMask, Structs::AimStrategy_Crosshair, 100);
                writeDefaultWeaponProfile("Smg", 7.5f, 17.0f, 1.22f, 1.42f, 5.5f, 7.0f, 0.08f, 0.06f, 0.20f, Structs::AimDefaultAimbotBoneMask, Structs::AimStrategy_Crosshair, 90);
                writeDefaultWeaponProfile("Shotgun", 9.0f, 12.0f, 1.30f, 1.55f, 6.0f, 8.0f, 0.08f, 0.06f, 0.16f, Structs::AimDefaultAimbotBoneMask, Structs::AimStrategy_Distance, 75);
                writeDefaultWeaponProfile("Rifle", 6.0f, 18.0f, 1.30f, 1.55f, 6.0f, 8.0f, 0.08f, 0.06f, 0.22f, Structs::AimDefaultAimbotBoneMask, Structs::AimStrategy_Crosshair, 120);
                writeDefaultWeaponProfile("Sniper", 3.8f, 23.0f, 1.05f, 1.10f, 4.0f, 5.0f, 0.05f, 0.04f, 0.27f, Structs::AimDefaultAimbotBoneMask, Structs::AimStrategy_Crosshair, 160);
                writeDefaultWeaponProfile("Lmg", 6.8f, 20.0f, 1.28f, 1.60f, 6.5f, 9.0f, 0.09f, 0.07f, 0.22f, Structs::AimDefaultAimbotBoneMask, Structs::AimStrategy_Hybrid, 130);

                auto writeDefaultTriggerProfile = [&](const char* name, float hitboxPx, int preDelay, int postDelay, int timeoutMs, std::uint64_t boneMask)
                {
                    nlohmann::json& p = j["Aim"]["TriggerProfiles"][name];
                    p["HitboxRadiusPx"] = hitboxPx;
                    p["PreFireDelayMs"] = preDelay;
                    p["PostFireIntervalMs"] = postDelay;
                    p["TimeoutForceFireMs"] = timeoutMs;
                    p["BoneMask"] = boneMask;
                };

                writeDefaultTriggerProfile("Pistol", 4.8f, 45, 90, 0, Structs::AimAllBoneMask);
                writeDefaultTriggerProfile("Smg", 4.4f, 30, 45, 0, Structs::AimAllBoneMask);
                writeDefaultTriggerProfile("Shotgun", 6.2f, 20, 280, 0, Structs::AimAllBoneMask);
                writeDefaultTriggerProfile("Rifle", 4.2f, 35, 65, 0, Structs::AimAllBoneMask);
                writeDefaultTriggerProfile("Sniper", 3.8f, 45, 650, 0, Structs::AimAllBoneMask);
                writeDefaultTriggerProfile("Lmg", 4.2f, 40, 70, 0, Structs::AimAllBoneMask);

                auto writeDefaultFlickProfile = [&](const char* name, float fov, float smooth, float followSmooth, int maxFlickTimeMs, int restartIntervalMs, bool dynamicFovEnabled, bool autowallEnabled, bool autowallKillshotOnly, std::uint64_t boneMask)
                {
                    nlohmann::json& p = j["Aim"]["FlickProfiles"][name];
                    p["Fov"] = fov;
                    p["Smooth"] = smooth;
                    p["FollowSmooth"] = followSmooth;
                    p["MaxFlickTimeMs"] = maxFlickTimeMs;
                    p["RestartIntervalMs"] = restartIntervalMs;
                    p["BoneMask"] = boneMask;
                    p["DynamicFovEnabled"] = dynamicFovEnabled;
                    p["AutowallEnabled"] = autowallEnabled;
                    p["AutowallKillshotOnly"] = autowallKillshotOnly;
                };

                writeDefaultFlickProfile("Pistol", 6.8f, 14.0f, 22.0f, 210, 120, true, false, false, Structs::AimDefaultAimbotBoneMask);
                writeDefaultFlickProfile("Smg", 7.6f, 15.5f, 22.0f, 220, 120, true, false, false, Structs::AimDefaultAimbotBoneMask);
                writeDefaultFlickProfile("Shotgun", 9.5f, 11.0f, 20.0f, 200, 120, true, false, false, Structs::AimDefaultAimbotBoneMask);
                writeDefaultFlickProfile("Rifle", 6.2f, 16.0f, 24.0f, 230, 120, true, false, false, Structs::AimDefaultAimbotBoneMask);
                writeDefaultFlickProfile("Sniper", 4.2f, 22.0f, 30.0f, 260, 140, false, false, false, Structs::AimDefaultAimbotBoneMask);
                writeDefaultFlickProfile("Lmg", 6.8f, 17.0f, 24.0f, 240, 120, true, false, false, Structs::AimDefaultAimbotBoneMask);

                nlohmann::json& flickSpecial = j["Aim"]["FlickSpecialProfiles"];
                flickSpecial["DesertEagle"] = j["Aim"]["FlickProfiles"]["Pistol"];
                flickSpecial["R8Revolver"] = j["Aim"]["FlickProfiles"]["Pistol"];

                j["Aim"]["TriggerSpecialProfiles"]["DesertEagle"]["HitboxRadiusPx"] = 4.7f;
                j["Aim"]["TriggerSpecialProfiles"]["DesertEagle"]["PreFireDelayMs"] = 50;
                j["Aim"]["TriggerSpecialProfiles"]["DesertEagle"]["PostFireIntervalMs"] = 425;
                j["Aim"]["TriggerSpecialProfiles"]["DesertEagle"]["TimeoutForceFireMs"] = 0;
                j["Aim"]["TriggerSpecialProfiles"]["DesertEagle"]["HoldFireMs"] = 8;
                j["Aim"]["TriggerSpecialProfiles"]["DesertEagle"]["BoneMask"] = Structs::AimAllBoneMask;

                j["Aim"]["TriggerSpecialProfiles"]["R8Revolver"]["HitboxRadiusPx"] = 4.9f;
                j["Aim"]["TriggerSpecialProfiles"]["R8Revolver"]["PreFireDelayMs"] = 55;
                j["Aim"]["TriggerSpecialProfiles"]["R8Revolver"]["PostFireIntervalMs"] = 420;
                j["Aim"]["TriggerSpecialProfiles"]["R8Revolver"]["TimeoutForceFireMs"] = 0;
                j["Aim"]["TriggerSpecialProfiles"]["R8Revolver"]["HoldFireMs"] = 235;
                j["Aim"]["TriggerSpecialProfiles"]["R8Revolver"]["BoneMask"] = Structs::AimAllBoneMask;

                j["Visuals"]["Enabled"] = false;
                j["Visuals"]["TeamCheck"] = false;
                j["Visuals"]["VisibleCheck"] = false;
                j["Visuals"]["VisCheckDebug"] = false;
                j["Visuals"]["Legit"] = false;
                j["Visuals"]["VisCheckDebugMode"] = 1;
                j["Visuals"]["VisCheckDebugMaxDistance"] = 3200.0f;
                j["Visuals"]["VisCheckDebugMaxItems"] = 1400;
                j["Visuals"]["VisCheckDebugColor"] = { 0.25f, 0.85f, 1.0f, 0.65f };

                j["Visuals"]["Background"] = false;
                j["Visuals"]["Hitmarker"] = false;
                j["Visuals"]["HitmarkerColor"] = { 1.0f, 1.0f, 1.0f, 1.0f };
                j["Visuals"]["Watermark"] = false;
                j["Visuals"]["WatermarkColor"] = { 1.0f, 1.0f, 1.0f, 1.0f };

                j["Visuals"]["Name"] = false;
                j["Visuals"]["NameColor"] = { 1.0f, 1.0f, 1.0f, 1.0f };
                j["Visuals"]["Box"] = false;
                j["Visuals"]["BoxColor"] = { 1.0f, 1.0f, 1.0f, 1.0f };
                j["Visuals"]["BoxColorVisible"] = { 1.0f, 1.0f, 1.0f, 1.0f };
                j["Visuals"]["Health"] = false;
                j["Visuals"]["Armor"] = true;
                j["Visuals"]["Money"] = true;
                j["Visuals"]["MoneyColor"] = { 0.65f, 1.0f, 0.65f, 1.0f };
                j["Visuals"]["Weapon"] = false;
                j["Visuals"]["WeaponColor"] = { 1.0f, 1.0f, 1.0f, 1.0f };
                j["Visuals"]["Bones"] = false;
                j["Visuals"]["BonesColor"] = { 1.0f, 1.0f, 1.0f, 1.0f };
                j["Visuals"]["BonesColorVisible"] = { 0.45f, 1.0f, 0.55f, 1.0f };
                j["Visuals"]["SoundEsp"] = false;
                j["Visuals"]["C4"] = true;
                j["Visuals"]["C4Color"] = { 1.0f, 0.55f, 0.35f, 1.0f };
                j["Visuals"]["C4PanelPosX"] = 0.02f;
                j["Visuals"]["C4PanelPosY"] = 0.06f;
                j["Visuals"]["SpectatorList"] = false;
                j["Visuals"]["SpectatorListColor"] = { 0.65f, 0.85f, 1.0f, 1.0f };
                j["Visuals"]["SpectatorListPanelPosX"] = 0.02f;
                j["Visuals"]["SpectatorListPanelPosY"] = 0.16f;
                j["Visuals"]["Defuser"] = true;
                j["Visuals"]["GrenadeHelper"] = false;
                j["Visuals"]["GrenadeHelperFilterByWeapon"] = true;
                j["Visuals"]["GrenadeHelperDrawStand"] = true;
                j["Visuals"]["GrenadeHelperDrawAim"] = true;
                j["Visuals"]["GrenadeHelperManualTypeOverride"] = false;
                j["Visuals"]["GrenadeHelperManualType"] = 0;
                j["Visuals"]["GrenadeHelperStandTolerance"] = 35.0f;
                j["Visuals"]["GrenadeHelperFocusRadius"] = 20.0f;
                j["Visuals"]["GrenadeHelperMaxStandDrawDistance"] = 2000.0f;
                j["Visuals"]["GrenadeHelperLooseGuideDistance"] = 200.0f;
                j["Visuals"]["GrenadeHelperTopHintOffsetX"] = 0.5f;
                j["Visuals"]["GrenadeHelperTopHintOffsetY"] = 0.03f;
                j["Visuals"]["GrenadeHelperStandColor"] = { 1.0f, 1.0f, 0.0f, 1.0f };
                j["Visuals"]["GrenadeHelperAimColor"] = { 0.0f, 1.0f, 0.0f, 1.0f };
                j["Visuals"]["GrenadeHelperGuideLineColor"] = { 1.0f, 1.0f, 1.0f, 0.75f };
                j["Visuals"]["GrenadeHelperFontColor"] = { 0.96f, 0.96f, 0.96f, 1.0f };
                j["Visuals"]["GrenadeHelperFontSize"] = 16.0f;
                j["Visuals"]["GrenadeHelperTopHintColor"] = { 1.0f, 1.0f, 1.0f, 1.0f };
                j["Visuals"]["GrenadeHelperTopHintFontSize"] = 30.0f;

                j["Radar"]["Enabled"] = false;
                j["Radar"]["Host"] = "127.0.0.1";
                j["Radar"]["StaticPort"] = 36364;
                j["Radar"]["IngestPort"] = 36365;
                j["Radar"]["PublishIntervalMs"] = 66;
                j["Radar"]["HttpTimeoutMs"] = 3000;
                j["Radar"]["ReconnectBaseMs"] = 500;
                j["Radar"]["ReconnectMaxMs"] = 5000;

                j["Info"]["Language"] = 0;
                j["Info"]["DebugEnabled"] = false;
                j["Info"]["DebugPerf"] = false;
                j["Info"]["DebugTrigger"] = false;
                j["Info"]["DebugVisCheck"] = false;
                j["Info"]["DebugAutowall"] = false;
                j["Info"]["DebugSpectatorList"] = false;
                j["Info"]["DebugRadar"] = false;

                file << j.dump(4);  // Write JSON with pretty print
                file.close();
                return true;
            }
            return false;
        }

        void LoadWeaponProfiles(const nlohmann::json& j)
        {
            if (!j.contains("Aim"))
                return;

            const auto& aimSection = j["Aim"];
            const bool hasAimbotBoneMask = aimSection.contains("AimbotBoneMask");
            const bool hasTriggerBoneMask = aimSection.contains("TriggerBoneMask");
            const bool hasUnifiedTriggerRadius = aimSection.contains("TriggerUnifiedHitboxRadiusPx");
            const bool hasHeadTriggerRadius = aimSection.contains("TriggerHeadRadiusPx");
            const std::uint64_t legacyRootAimbotBoneMask = hasAimbotBoneMask
                ? aimSection["AimbotBoneMask"].get<std::uint64_t>()
                : 0ull;
            const std::uint64_t legacyRootTriggerBoneMask = hasTriggerBoneMask
                ? aimSection["TriggerBoneMask"].get<std::uint64_t>()
                : 0ull;
            if (aimSection.contains("WeaponProfiles"))
            {
                const auto& profileRoot = aimSection["WeaponProfiles"];

                auto readAimProfile = [&](const char* name, Structs::AimWeaponProfile& profile) -> bool
                {
                    if (!profileRoot.contains(name))
                        return false;

                    const auto& node = profileRoot[name];
                    if (node.contains("Fov")) profile.Fov = node["Fov"].get<float>();
                    if (node.contains("Smooth")) profile.Smooth = node["Smooth"].get<float>();
                    if (node.contains("SprayAxisStrengthX")) profile.SprayAxisStrengthX = node["SprayAxisStrengthX"].get<float>();
                    if (node.contains("SprayAxisStrengthY")) profile.SprayAxisStrengthY = node["SprayAxisStrengthY"].get<float>();
                    if (node.contains("SprayAxisMaxStepX")) profile.SprayAxisMaxStepX = node["SprayAxisMaxStepX"].get<float>();
                    if (node.contains("SprayAxisMaxStepY")) profile.SprayAxisMaxStepY = node["SprayAxisMaxStepY"].get<float>();
                    if (node.contains("SprayAxisDeadzoneX")) profile.SprayAxisDeadzoneX = node["SprayAxisDeadzoneX"].get<float>();
                    if (node.contains("SprayAxisDeadzoneY")) profile.SprayAxisDeadzoneY = node["SprayAxisDeadzoneY"].get<float>();
                    if (node.contains("CurveStrength")) profile.CurveStrength = node["CurveStrength"].get<float>();
                    if (node.contains("BoneMask")) profile.BoneMask = node["BoneMask"].get<std::uint64_t>();
                    if (node.contains("TargetStrategy")) profile.TargetStrategy = node["TargetStrategy"].get<int>();
                    if (node.contains("TargetSwitchDelayMs")) profile.TargetSwitchDelayMs = node["TargetSwitchDelayMs"].get<int>();
                    return true;
                };

                Structs::AimWeaponProfile legacyDefault{};
                const bool hasLegacyDefault = readAimProfile("Default", legacyDefault);

                auto readAimProfileWithFallback = [&](const char* name, Structs::AimWeaponProfile& profile)
                {
                    if (!readAimProfile(name, profile) && hasLegacyDefault)
                        profile = legacyDefault;
                };

                readAimProfileWithFallback("Pistol", Aim.WeaponProfiles[Structs::AimWeapon_Pistol]);
                readAimProfileWithFallback("Smg", Aim.WeaponProfiles[Structs::AimWeapon_Smg]);
                readAimProfileWithFallback("Shotgun", Aim.WeaponProfiles[Structs::AimWeapon_Shotgun]);
                readAimProfileWithFallback("Rifle", Aim.WeaponProfiles[Structs::AimWeapon_Rifle]);
                readAimProfileWithFallback("Sniper", Aim.WeaponProfiles[Structs::AimWeapon_Sniper]);
                readAimProfileWithFallback("Lmg", Aim.WeaponProfiles[Structs::AimWeapon_Lmg]);

                auto profileHasBoneMask = [&](const char* name) -> bool
                {
                    if (!profileRoot.contains(name) || !profileRoot[name].is_object())
                        return false;
                    return profileRoot[name].contains("BoneMask");
                };

                if (legacyRootAimbotBoneMask != 0ull)
                {
                    if (!profileHasBoneMask("Pistol")) Aim.WeaponProfiles[Structs::AimWeapon_Pistol].BoneMask = legacyRootAimbotBoneMask;
                    if (!profileHasBoneMask("Smg")) Aim.WeaponProfiles[Structs::AimWeapon_Smg].BoneMask = legacyRootAimbotBoneMask;
                    if (!profileHasBoneMask("Shotgun")) Aim.WeaponProfiles[Structs::AimWeapon_Shotgun].BoneMask = legacyRootAimbotBoneMask;
                    if (!profileHasBoneMask("Rifle")) Aim.WeaponProfiles[Structs::AimWeapon_Rifle].BoneMask = legacyRootAimbotBoneMask;
                    if (!profileHasBoneMask("Sniper")) Aim.WeaponProfiles[Structs::AimWeapon_Sniper].BoneMask = legacyRootAimbotBoneMask;
                    if (!profileHasBoneMask("Lmg")) Aim.WeaponProfiles[Structs::AimWeapon_Lmg].BoneMask = legacyRootAimbotBoneMask;
                }
            }
            else
            {
                // Migrate legacy single profile values to all profiles.
                for (Structs::AimWeaponProfile& profile : Aim.WeaponProfiles)
                {
                    profile.Fov = Aim.AimbotFov;
                    profile.Smooth = Aim.AimbotSmooth;
                    profile.BoneMask = legacyRootAimbotBoneMask != 0ull
                        ? legacyRootAimbotBoneMask
                        : Structs::AimDefaultAimbotBoneMask;
                }
            }

            if (aimSection.contains("TriggerProfiles"))
            {
                const auto& triggerRoot = aimSection["TriggerProfiles"];

                auto readTriggerProfile = [&](const char* name, Structs::TriggerWeaponProfile& profile) -> bool
                {
                    if (!triggerRoot.contains(name))
                        return false;

                    const auto& node = triggerRoot[name];
                    if (node.contains("HitboxRadiusPx")) profile.HitboxRadiusPx = node["HitboxRadiusPx"].get<float>();
                    if (node.contains("PreFireDelayMs")) profile.PreFireDelayMs = node["PreFireDelayMs"].get<int>();
                    if (node.contains("PostFireIntervalMs")) profile.PostFireIntervalMs = node["PostFireIntervalMs"].get<int>();
                    if (node.contains("TimeoutForceFireMs")) profile.TimeoutForceFireMs = node["TimeoutForceFireMs"].get<int>();
                    if (node.contains("BoneMask")) profile.BoneMask = node["BoneMask"].get<std::uint64_t>();
                    else if (legacyRootTriggerBoneMask != 0ull) profile.BoneMask = legacyRootTriggerBoneMask;
                    return true;
                };

                Structs::TriggerWeaponProfile legacyDefault{};
                const bool hasLegacyDefault = readTriggerProfile("Default", legacyDefault);

                auto readTriggerProfileWithFallback = [&](const char* name, Structs::TriggerWeaponProfile& profile)
                {
                    if (!readTriggerProfile(name, profile) && hasLegacyDefault)
                        profile = legacyDefault;
                };

                readTriggerProfileWithFallback("Pistol", Aim.TriggerProfiles[Structs::AimWeapon_Pistol]);
                readTriggerProfileWithFallback("Smg", Aim.TriggerProfiles[Structs::AimWeapon_Smg]);
                readTriggerProfileWithFallback("Shotgun", Aim.TriggerProfiles[Structs::AimWeapon_Shotgun]);
                readTriggerProfileWithFallback("Rifle", Aim.TriggerProfiles[Structs::AimWeapon_Rifle]);
                readTriggerProfileWithFallback("Sniper", Aim.TriggerProfiles[Structs::AimWeapon_Sniper]);
                readTriggerProfileWithFallback("Lmg", Aim.TriggerProfiles[Structs::AimWeapon_Lmg]);
            }
            else
            {
                // Migrate legacy trigger delay/interval fields.
                for (Structs::TriggerWeaponProfile& profile : Aim.TriggerProfiles)
                {
                    profile.PreFireDelayMs = (std::max)(0, Aim.TriggerDelay);
                    profile.PostFireIntervalMs = (std::max)(0, Aim.TriggerMinIntervalMs);
                    profile.BoneMask = legacyRootTriggerBoneMask != 0ull ? legacyRootTriggerBoneMask : Structs::AimAllBoneMask;
                }
            }

            if (aimSection.contains("FlickProfiles"))
            {
                const auto& flickRoot = aimSection["FlickProfiles"];

                auto readFlickProfile = [&](const char* name, Structs::FlickWeaponProfile& profile) -> bool
                {
                    if (!flickRoot.contains(name))
                        return false;

                    const auto& node = flickRoot[name];
                    if (node.contains("Fov")) profile.Fov = node["Fov"].get<float>();
                    if (node.contains("Smooth")) profile.Smooth = node["Smooth"].get<float>();
                    if (node.contains("FollowSmooth")) profile.FollowSmooth = node["FollowSmooth"].get<float>();
                    if (node.contains("MaxFlickTimeMs")) profile.MaxFlickTimeMs = node["MaxFlickTimeMs"].get<int>();
                    if (node.contains("RestartIntervalMs")) profile.RestartIntervalMs = node["RestartIntervalMs"].get<int>();
                    if (node.contains("BoneMask")) profile.BoneMask = node["BoneMask"].get<std::uint64_t>();
                    if (node.contains("DynamicFovEnabled")) profile.DynamicFovEnabled = node["DynamicFovEnabled"].get<bool>();
                    if (node.contains("AutowallEnabled")) profile.AutowallEnabled = node["AutowallEnabled"].get<bool>();
                    if (node.contains("AutowallKillshotOnly")) profile.AutowallKillshotOnly = node["AutowallKillshotOnly"].get<bool>();
                    return true;
                };

                Structs::FlickWeaponProfile legacyDefault{};
                const bool hasLegacyDefault = readFlickProfile("Default", legacyDefault);

                auto readFlickProfileWithFallback = [&](const char* name, Structs::FlickWeaponProfile& profile)
                {
                    if (!readFlickProfile(name, profile) && hasLegacyDefault)
                        profile = legacyDefault;
                };

                readFlickProfileWithFallback("Pistol", Aim.FlickProfiles[Structs::AimWeapon_Pistol]);
                readFlickProfileWithFallback("Smg", Aim.FlickProfiles[Structs::AimWeapon_Smg]);
                readFlickProfileWithFallback("Shotgun", Aim.FlickProfiles[Structs::AimWeapon_Shotgun]);
                readFlickProfileWithFallback("Rifle", Aim.FlickProfiles[Structs::AimWeapon_Rifle]);
                readFlickProfileWithFallback("Sniper", Aim.FlickProfiles[Structs::AimWeapon_Sniper]);
                readFlickProfileWithFallback("Lmg", Aim.FlickProfiles[Structs::AimWeapon_Lmg]);

                Aim.FlickSpecialProfiles[Structs::TriggerSpecial_Deagle] = Aim.FlickProfiles[Structs::AimWeapon_Pistol];
                Aim.FlickSpecialProfiles[Structs::TriggerSpecial_Revolver] = Aim.FlickProfiles[Structs::AimWeapon_Pistol];
            }

            if (aimSection.contains("FlickSpecialProfiles"))
            {
                const auto& flickSpecialRoot = aimSection["FlickSpecialProfiles"];

                auto readFlickSpecialProfile = [&](const char* name, Structs::FlickWeaponProfile& profile) -> bool
                {
                    if (!flickSpecialRoot.contains(name))
                        return false;

                    const auto& node = flickSpecialRoot[name];
                    if (node.contains("Fov")) profile.Fov = node["Fov"].get<float>();
                    if (node.contains("Smooth")) profile.Smooth = node["Smooth"].get<float>();
                    if (node.contains("FollowSmooth")) profile.FollowSmooth = node["FollowSmooth"].get<float>();
                    if (node.contains("MaxFlickTimeMs")) profile.MaxFlickTimeMs = node["MaxFlickTimeMs"].get<int>();
                    if (node.contains("RestartIntervalMs")) profile.RestartIntervalMs = node["RestartIntervalMs"].get<int>();
                    if (node.contains("BoneMask")) profile.BoneMask = node["BoneMask"].get<std::uint64_t>();
                    if (node.contains("DynamicFovEnabled")) profile.DynamicFovEnabled = node["DynamicFovEnabled"].get<bool>();
                    if (node.contains("AutowallEnabled")) profile.AutowallEnabled = node["AutowallEnabled"].get<bool>();
                    if (node.contains("AutowallKillshotOnly")) profile.AutowallKillshotOnly = node["AutowallKillshotOnly"].get<bool>();
                    return true;
                };

                readFlickSpecialProfile("DesertEagle", Aim.FlickSpecialProfiles[Structs::TriggerSpecial_Deagle]);
                readFlickSpecialProfile("R8Revolver", Aim.FlickSpecialProfiles[Structs::TriggerSpecial_Revolver]);
            }

            if (aimSection.contains("TriggerSpecialProfiles"))
            {
                const auto& specialRoot = aimSection["TriggerSpecialProfiles"];

                auto readTriggerSpecial = [&](const char* name, Structs::TriggerSpecialProfile& profile)
                {
                    if (!specialRoot.contains(name))
                        return;

                    const auto& node = specialRoot[name];
                    if (node.contains("HitboxRadiusPx")) profile.HitboxRadiusPx = node["HitboxRadiusPx"].get<float>();
                    if (node.contains("PreFireDelayMs")) profile.PreFireDelayMs = node["PreFireDelayMs"].get<int>();
                    if (node.contains("PostFireIntervalMs")) profile.PostFireIntervalMs = node["PostFireIntervalMs"].get<int>();
                    if (node.contains("TimeoutForceFireMs")) profile.TimeoutForceFireMs = node["TimeoutForceFireMs"].get<int>();
                    if (node.contains("HoldFireMs")) profile.HoldFireMs = node["HoldFireMs"].get<int>();
                    if (node.contains("BoneMask")) profile.BoneMask = node["BoneMask"].get<std::uint64_t>();
                    else if (legacyRootTriggerBoneMask != 0ull) profile.BoneMask = legacyRootTriggerBoneMask;
                };

                readTriggerSpecial("DesertEagle", Aim.TriggerSpecialProfiles[Structs::TriggerSpecial_Deagle]);
                readTriggerSpecial("R8Revolver", Aim.TriggerSpecialProfiles[Structs::TriggerSpecial_Revolver]);
            }
            else if (legacyRootTriggerBoneMask != 0ull)
            {
                for (Structs::TriggerSpecialProfile& profile : Aim.TriggerSpecialProfiles)
                    profile.BoneMask = legacyRootTriggerBoneMask;
            }

            Aim.WeaponProfileEditorIndex = std::clamp(Aim.WeaponProfileEditorIndex, 0, Structs::AimWeapon_Count - 1);
            Aim.TriggerProfileEditorIndex = std::clamp(Aim.TriggerProfileEditorIndex, 0, Structs::AimWeapon_Count - 1);
            Aim.FlickProfileEditorIndex = std::clamp(Aim.FlickProfileEditorIndex, 0, Structs::AimWeapon_Count - 1);
            Aim.FlickSpecialEditorIndex = std::clamp(Aim.FlickSpecialEditorIndex, 0, Structs::TriggerSpecial_Count - 1);
            Aim.TriggerSpecialEditorIndex = std::clamp(Aim.TriggerSpecialEditorIndex, 0, Structs::TriggerSpecial_Count - 1);
            Aim.TriggerDetectMode = std::clamp(Aim.TriggerDetectMode, 0, static_cast<int>(Structs::TriggerDetectModeNames.size()) - 1);

            constexpr std::uint32_t kLegacyAllHitGroups =
                Structs::AimHit_Head |
                Structs::AimHit_UpperChest |
                Structs::AimHit_Torso |
                Structs::AimHit_Pelvis |
                Structs::AimHit_Arms |
                Structs::AimHit_Legs;

            auto legacyHitGroupToBoneMask = [](const std::uint32_t legacyMask) -> std::uint64_t
            {
                auto legacyGroupOfBone = [](const int boneId) -> std::uint32_t
                {
                    switch (boneId)
                    {
                    case 6:
                        return Structs::AimHit_Head;
                    case 5:
                        return Structs::AimHit_UpperChest;
                    case 4:
                    case 2:
                        return Structs::AimHit_Torso;
                    case 0:
                        return Structs::AimHit_Pelvis;
                    case 8:
                    case 9:
                    case 10:
                    case 13:
                    case 14:
                    case 15:
                        return Structs::AimHit_Arms;
                    case 22:
                    case 23:
                    case 24:
                    case 25:
                    case 26:
                    case 27:
                        return Structs::AimHit_Legs;
                    default:
                        break;
                    }

                    return 0u;
                };

                std::uint64_t mask = 0ull;
                for (const int boneId : Structs::AimBoneIds)
                {
                    if ((legacyMask & legacyGroupOfBone(boneId)) != 0u)
                        mask |= Structs::BoneMaskFromBoneId(boneId);
                }
                return mask;
            };

            Aim.AimbotHitGroupMask &= kLegacyAllHitGroups;
            Aim.TriggerHitGroupMask &= kLegacyAllHitGroups;
            if (Aim.AimbotHitGroupMask == 0)
                Aim.AimbotHitGroupMask = Structs::AimHit_Head;
            if (Aim.TriggerHitGroupMask == 0)
                Aim.TriggerHitGroupMask = kLegacyAllHitGroups;

            if (!hasAimbotBoneMask)
            {
                const std::uint64_t migrated = legacyHitGroupToBoneMask(Aim.AimbotHitGroupMask);
                Aim.AimbotBoneMask = migrated != 0ull ? migrated : Structs::AimDefaultAimbotBoneMask;
            }
            if (!hasTriggerBoneMask)
            {
                const std::uint64_t migrated = legacyHitGroupToBoneMask(Aim.TriggerHitGroupMask);
                Aim.TriggerBoneMask = migrated != 0ull ? migrated : Structs::AimAllBoneMask;
            }

            Aim.AimbotBoneMask &= Structs::AimAllBoneMask;
            Aim.TriggerBoneMask &= Structs::AimAllBoneMask;
            if (Aim.AimbotBoneMask == 0ull)
                Aim.AimbotBoneMask = Structs::AimDefaultAimbotBoneMask;
            if (Aim.TriggerBoneMask == 0ull)
                Aim.TriggerBoneMask = Structs::AimAllBoneMask;

            if (!hasUnifiedTriggerRadius)
            {
                Aim.TriggerUnifiedHitboxRadiusPx = std::clamp(
                    Aim.TriggerProfiles[Structs::AimWeapon_Rifle].HitboxRadiusPx,
                    0.5f,
                    40.0f
                );
            }
            if (!hasHeadTriggerRadius)
            {
                Aim.TriggerHeadRadiusPx = std::clamp(
                    Aim.TriggerUnifiedHitboxRadiusPx * 1.40f,
                    0.5f,
                    80.0f
                );
            }

            Aim.TriggerUnifiedHitboxRadiusPx = std::clamp(Aim.TriggerUnifiedHitboxRadiusPx, 0.5f, 40.0f);
            Aim.TriggerHitboxScale = std::clamp(Aim.TriggerHitboxScale, 0.25f, 3.0f);
            Aim.TriggerHitboxAddPx = std::clamp(Aim.TriggerHitboxAddPx, -20.0f, 40.0f);
            Aim.TriggerHeadRadiusPx = std::clamp(Aim.TriggerHeadRadiusPx, 0.5f, 80.0f);
            Aim.TriggerHeadScale = std::clamp(Aim.TriggerHeadScale, 0.20f, 10.0f);
            Aim.TriggerTorsoScale = std::clamp(Aim.TriggerTorsoScale, 0.20f, 10.0f);
            Aim.TriggerArmsScale = std::clamp(Aim.TriggerArmsScale, 0.20f, 10.0f);
            Aim.TriggerLegsScale = std::clamp(Aim.TriggerLegsScale, 0.20f, 10.0f);
            Aim.TriggerHitboxDebugThickness = std::clamp(Aim.TriggerHitboxDebugThickness, 0.5f, 4.0f);

            for (Structs::AimWeaponProfile& profile : Aim.WeaponProfiles)
            {
                profile.Fov = (std::max)(0.1f, profile.Fov);
                profile.Smooth = (std::max)(1.0f, profile.Smooth);
                profile.SprayAxisStrengthX = std::clamp(profile.SprayAxisStrengthX, 0.50f, 2.50f);
                profile.SprayAxisStrengthY = std::clamp(profile.SprayAxisStrengthY, 0.50f, 2.50f);
                profile.SprayAxisMaxStepX = std::clamp(profile.SprayAxisMaxStepX, 1.0f, 20.0f);
                profile.SprayAxisMaxStepY = std::clamp(profile.SprayAxisMaxStepY, 1.0f, 20.0f);
                profile.SprayAxisDeadzoneX = std::clamp(profile.SprayAxisDeadzoneX, 0.0f, 1.0f);
                profile.SprayAxisDeadzoneY = std::clamp(profile.SprayAxisDeadzoneY, 0.0f, 1.0f);
                profile.CurveStrength = std::clamp(profile.CurveStrength, 0.0f, 1.0f);
                profile.BoneMask &= Structs::AimAllBoneMask;
                if (profile.BoneMask == 0ull)
                    profile.BoneMask = Aim.AimbotBoneMask;
                profile.TargetStrategy = std::clamp(profile.TargetStrategy, 0, (int)Structs::AimTargetStrategyNames.size() - 1);
                profile.TargetSwitchDelayMs = (std::max)(0, profile.TargetSwitchDelayMs);
            }

            for (Structs::TriggerWeaponProfile& profile : Aim.TriggerProfiles)
            {
                profile.HitboxRadiusPx = std::clamp(profile.HitboxRadiusPx, 0.5f, 30.0f);
                profile.PreFireDelayMs = std::clamp(profile.PreFireDelayMs, 0, 2000);
                profile.PostFireIntervalMs = std::clamp(profile.PostFireIntervalMs, 0, 2000);
                profile.TimeoutForceFireMs = std::clamp(profile.TimeoutForceFireMs, 0, 5000);
                profile.BoneMask &= Structs::AimAllBoneMask;
                if (profile.BoneMask == 0ull)
                    profile.BoneMask = Aim.TriggerBoneMask;
            }

            for (Structs::FlickWeaponProfile& profile : Aim.FlickProfiles)
            {
                profile.Fov = std::clamp(profile.Fov, 0.1f, 60.0f);
                profile.Smooth = std::clamp(profile.Smooth, 1.0f, 100.0f);
                profile.FollowSmooth = std::clamp(profile.FollowSmooth, 1.0f, 100.0f);
                profile.MaxFlickTimeMs = std::clamp(profile.MaxFlickTimeMs, 10, 5000);
                profile.RestartIntervalMs = std::clamp(profile.RestartIntervalMs, 0, 5000);
                profile.BoneMask &= Structs::AimAllBoneMask;
                if (profile.BoneMask == 0ull)
                    profile.BoneMask = Aim.AimbotBoneMask;
            }

            for (Structs::FlickWeaponProfile& profile : Aim.FlickSpecialProfiles)
            {
                profile.Fov = std::clamp(profile.Fov, 0.1f, 60.0f);
                profile.Smooth = std::clamp(profile.Smooth, 1.0f, 100.0f);
                profile.FollowSmooth = std::clamp(profile.FollowSmooth, 1.0f, 100.0f);
                profile.MaxFlickTimeMs = std::clamp(profile.MaxFlickTimeMs, 10, 5000);
                profile.RestartIntervalMs = std::clamp(profile.RestartIntervalMs, 0, 5000);
                profile.BoneMask &= Structs::AimAllBoneMask;
                if (profile.BoneMask == 0ull)
                    profile.BoneMask = Aim.AimbotBoneMask;
            }

            for (Structs::TriggerSpecialProfile& profile : Aim.TriggerSpecialProfiles)
            {
                profile.HitboxRadiusPx = std::clamp(profile.HitboxRadiusPx, 0.5f, 30.0f);
                profile.PreFireDelayMs = std::clamp(profile.PreFireDelayMs, 0, 2000);
                profile.PostFireIntervalMs = std::clamp(profile.PostFireIntervalMs, 0, 3000);
                profile.TimeoutForceFireMs = std::clamp(profile.TimeoutForceFireMs, 0, 5000);
                profile.HoldFireMs = std::clamp(profile.HoldFireMs, 0, 1200);
                profile.BoneMask &= Structs::AimAllBoneMask;
                if (profile.BoneMask == 0ull)
                    profile.BoneMask = Aim.TriggerBoneMask;
            }
        }

        template<typename T>
        void LoadConfigSection(const nlohmann::json& j, const std::string& sectionName, T& configSection)
        {
            if (j.contains(sectionName))
            {
                const auto& section = j[sectionName];
                for (const auto& [key, value] : section.items())
                {
                    if (section.contains(key))
                    {
                        if constexpr (std::is_same_v<T, Structs::AimConfig>)
                        {
                            if (key == "Trigger") configSection.Trigger = value.get<bool>();
                            else if (key == "TriggerKey") configSection.TriggerKey = value.get<int>();
                            else if (key == "TriggerKeyMode") configSection.TriggerKeyMode = value.get<int>();
							else if (key == "TriggerDelay") configSection.TriggerDelay = value.get<int>();
                            else if (key == "TriggerSecondKeyEnabled") configSection.TriggerSecondKeyEnabled = value.get<bool>();
                            else if (key == "TriggerSecondKey") configSection.TriggerSecondKey = value.get<int>();
                            else if (key == "TriggerSecondKeyMode") configSection.TriggerSecondKeyMode = value.get<int>();
                            else if (key == "TriggerMinIntervalMs") configSection.TriggerMinIntervalMs = value.get<int>();
                            else if (key == "TriggerDetectMode") configSection.TriggerDetectMode = value.get<int>();
                            else if (key == "TriggerUnifiedHitboxRadiusPx") configSection.TriggerUnifiedHitboxRadiusPx = value.get<float>();
                            else if (key == "TriggerHitboxScale") configSection.TriggerHitboxScale = value.get<float>();
                            else if (key == "TriggerHitboxAddPx") configSection.TriggerHitboxAddPx = value.get<float>();
                            else if (key == "TriggerHeadRadiusPx") configSection.TriggerHeadRadiusPx = value.get<float>();
                            else if (key == "TriggerHeadScale") configSection.TriggerHeadScale = value.get<float>();
                            else if (key == "TriggerTorsoScale") configSection.TriggerTorsoScale = value.get<float>();
                            else if (key == "TriggerArmsScale") configSection.TriggerArmsScale = value.get<float>();
                            else if (key == "TriggerLegsScale") configSection.TriggerLegsScale = value.get<float>();
                            else if (key == "TriggerHeadSphereDebug") configSection.TriggerHeadSphereDebug = value.get<bool>();
                            else if (key == "TriggerBoneMask") configSection.TriggerBoneMask = value.get<std::uint64_t>();
                            else if (key == "TriggerHitGroupMask") configSection.TriggerHitGroupMask = value.get<std::uint32_t>();
                            else if (key == "TriggerHitboxDebug") configSection.TriggerHitboxDebug = value.get<bool>();
                            else if (key == "TriggerHitboxDebugColor") configSection.TriggerHitboxDebugColor = ImVec4(value[0].get<float>(), value[1].get<float>(), value[2].get<float>(), value[3].get<float>());
                            else if (key == "TriggerHitboxDebugActiveColor") configSection.TriggerHitboxDebugActiveColor = ImVec4(value[0].get<float>(), value[1].get<float>(), value[2].get<float>(), value[3].get<float>());
                            else if (key == "TriggerHitboxDebugThickness") configSection.TriggerHitboxDebugThickness = value.get<float>();
                            else if (key == "Flick") configSection.Flick = value.get<bool>();
                            else if (key == "FlickKey") configSection.FlickKey = value.get<int>();
                            else if (key == "FlickKeyMode") configSection.FlickKeyMode = value.get<int>();
                            else if (key == "BlockTriggerWhenFlashed") configSection.BlockTriggerWhenFlashed = value.get<bool>();
                            else if (key == "BlockAimbotWhenFlashed") configSection.BlockAimbotWhenFlashed = value.get<bool>();

                            else if (key == "Aimbot") configSection.Aimbot = value.get<bool>();

                            else if (key == "DrawFov") configSection.DrawFov = value.get<bool>();
                            else if (key == "AimbotFovColor") configSection.AimbotFovColor = ImVec4(value[0].get<float>(), value[1].get<float>(), value[2].get<float>(), value[3].get<float>());

                            else if (key == "AimFriendly") configSection.AimFriendly = value.get<bool>();
                            else if (key == "AimVisible") configSection.AimVisible = value.get<bool>();

                            else if (key == "AimbotKey") configSection.AimbotKey = value.get<int>();
							else if (key == "AimbotKeyMode") configSection.AimbotKeyMode = value.get<int>();
                            else if (key == "AimbotSecondKeyEnabled") configSection.AimbotSecondKeyEnabled = value.get<bool>();
                            else if (key == "AimbotSecondKey") configSection.AimbotSecondKey = value.get<int>();
                            else if (key == "AimbotSecondKeyMode") configSection.AimbotSecondKeyMode = value.get<int>();
                            else if (key == "AimbotBoneMask") configSection.AimbotBoneMask = value.get<std::uint64_t>();
                            else if (key == "AimbotHitGroupMask") configSection.AimbotHitGroupMask = value.get<std::uint32_t>();
                            else if (key == "DeadzonePx") configSection.DeadzonePx = value.get<float>();

                            else if (key == "AimbotFov") configSection.AimbotFov = value.get<float>();
                            else if (key == "AimbotSmooth") configSection.AimbotSmooth = value.get<float>();
                            else if (key == "WeaponProfileEditorIndex") configSection.WeaponProfileEditorIndex = value.get<int>();
                            else if (key == "TriggerProfileEditorIndex") configSection.TriggerProfileEditorIndex = value.get<int>();
                            else if (key == "FlickProfileEditorIndex") configSection.FlickProfileEditorIndex = value.get<int>();
                            else if (key == "FlickSpecialEditorIndex") configSection.FlickSpecialEditorIndex = value.get<int>();
                            else if (key == "TriggerSpecialEditorIndex") configSection.TriggerSpecialEditorIndex = value.get<int>();
                        }
                        else if constexpr (std::is_same_v<T, Structs::KmboxConfig>)
                        {
                            if (key == "Enabled") configSection.Enabled = value.get<bool>();
                            else if (key == "Ip") configSection.Ip = value.get<std::string>();
                            else if (key == "Port") configSection.Port = value.get<unsigned short>();
                            else if (key == "Uuid") configSection.Uuid = value.get<std::string>();
                        }
                        else if constexpr (std::is_same_v<T, Structs::VisualsConfig>)
                        {
                            if (key == "Enabled") configSection.Enabled = value.get<bool>();
                            else if (key == "TeamCheck") configSection.TeamCheck = value.get<bool>();
                            else if (key == "VisibleCheck") configSection.VisibleCheck = value.get<bool>();
                            else if (key == "VisCheckDebug") configSection.VisCheckDebug = value.get<bool>();
                            else if (key == "Legit") configSection.Legit = value.get<bool>();
                            else if (key == "VisCheckDebugMode") configSection.VisCheckDebugMode = value.get<int>();
                            else if (key == "VisCheckDebugMaxDistance") configSection.VisCheckDebugMaxDistance = value.get<float>();
                            else if (key == "VisCheckDebugMaxItems") configSection.VisCheckDebugMaxItems = value.get<int>();
                            else if (key == "VisCheckDebugColor") configSection.VisCheckDebugColor = ImVec4(value[0].get<float>(), value[1].get<float>(), value[2].get<float>(), value[3].get<float>());

                            else if (key == "Background") configSection.Background = value.get<bool>();
                            else if (key == "Hitmarker") configSection.Hitmarker = value.get<bool>();
                            else if (key == "HitmarkerColor") configSection.HitmarkerColor = ImVec4(value[0].get<float>(), value[1].get<float>(), value[2].get<float>(), value[3].get<float>());
                            else if (key == "Watermark") configSection.Watermark = value.get<bool>();

                            else if (key == "Name") configSection.Name = value.get<bool>();
                            else if (key == "Box") configSection.Box = value.get<bool>();
                            else if (key == "Health") configSection.Health = value.get<bool>();
                            else if (key == "Armor") configSection.Armor = value.get<bool>();
                            else if (key == "Money") configSection.Money = value.get<bool>();
                            else if (key == "Weapon") configSection.Weapon = value.get<bool>();
                            else if (key == "Bones") configSection.Bones = value.get<bool>();
                            else if (key == "SoundEsp") configSection.SoundEsp = value.get<bool>();
                            else if (key == "C4") configSection.C4 = value.get<bool>();
                            else if (key == "C4PanelPosX") configSection.C4PanelPosX = value.get<float>();
                            else if (key == "C4PanelPosY") configSection.C4PanelPosY = value.get<float>();
                            else if (key == "SpectatorList") configSection.SpectatorList = value.get<bool>();
                            else if (key == "SpectatorListPanelPosX") configSection.SpectatorListPanelPosX = value.get<float>();
                            else if (key == "SpectatorListPanelPosY") configSection.SpectatorListPanelPosY = value.get<float>();
                            else if (key == "Defuser") configSection.Defuser = value.get<bool>();
                            else if (key == "GrenadeHelper") configSection.GrenadeHelper = value.get<bool>();
                            else if (key == "GrenadeHelperFilterByWeapon") configSection.GrenadeHelperFilterByWeapon = value.get<bool>();
                            else if (key == "GrenadeHelperDrawStand") configSection.GrenadeHelperDrawStand = value.get<bool>();
                            else if (key == "GrenadeHelperDrawAim") configSection.GrenadeHelperDrawAim = value.get<bool>();
                            else if (key == "GrenadeHelperManualTypeOverride") configSection.GrenadeHelperManualTypeOverride = value.get<bool>();
                            else if (key == "GrenadeHelperManualType") configSection.GrenadeHelperManualType = value.get<int>();
                            else if (key == "GrenadeHelperStandTolerance") configSection.GrenadeHelperStandTolerance = value.get<float>();
                            else if (key == "GrenadeHelperFocusRadius") configSection.GrenadeHelperFocusRadius = value.get<float>();
                            else if (key == "GrenadeHelperMaxStandDrawDistance") configSection.GrenadeHelperMaxStandDrawDistance = value.get<float>();
                            else if (key == "GrenadeHelperLooseGuideDistance") configSection.GrenadeHelperLooseGuideDistance = value.get<float>();
                            else if (key == "GrenadeHelperTopHintOffsetX") configSection.GrenadeHelperTopHintOffsetX = value.get<float>();
                            else if (key == "GrenadeHelperTopHintOffsetY") configSection.GrenadeHelperTopHintOffsetY = value.get<float>();
                            else if (key == "GrenadeHelperStandColor") configSection.GrenadeHelperStandColor = ImVec4(value[0].get<float>(), value[1].get<float>(), value[2].get<float>(), value[3].get<float>());
                            else if (key == "GrenadeHelperAimColor") configSection.GrenadeHelperAimColor = ImVec4(value[0].get<float>(), value[1].get<float>(), value[2].get<float>(), value[3].get<float>());
                            else if (key == "GrenadeHelperGuideLineColor") configSection.GrenadeHelperGuideLineColor = ImVec4(value[0].get<float>(), value[1].get<float>(), value[2].get<float>(), value[3].get<float>());
                            else if (key == "GrenadeHelperFontColor") configSection.GrenadeHelperFontColor = ImVec4(value[0].get<float>(), value[1].get<float>(), value[2].get<float>(), value[3].get<float>());
                            else if (key == "GrenadeHelperFontSize") configSection.GrenadeHelperFontSize = value.get<float>();
                            else if (key == "GrenadeHelperTopHintColor") configSection.GrenadeHelperTopHintColor = ImVec4(value[0].get<float>(), value[1].get<float>(), value[2].get<float>(), value[3].get<float>());
                            else if (key == "GrenadeHelperTopHintFontSize") configSection.GrenadeHelperTopHintFontSize = value.get<float>();

                            else if (key == "WatermarkColor") configSection.WatermarkColor = ImVec4(value[0].get<float>(), value[1].get<float>(), value[2].get<float>(), value[3].get<float>());
                            else if (key == "NameColor") configSection.NameColor = ImVec4(value[0].get<float>(), value[1].get<float>(), value[2].get<float>(), value[3].get<float>());
                            else if (key == "BoxColor") configSection.BoxColor = ImVec4(value[0].get<float>(), value[1].get<float>(), value[2].get<float>(), value[3].get<float>());
                            else if (key == "BoxColorVisible") configSection.BoxColorVisible = ImVec4(value[0].get<float>(), value[1].get<float>(), value[2].get<float>(), value[3].get<float>());
                            else if (key == "MoneyColor") configSection.MoneyColor = ImVec4(value[0].get<float>(), value[1].get<float>(), value[2].get<float>(), value[3].get<float>());
                            else if (key == "WeaponColor") configSection.WeaponColor = ImVec4(value[0].get<float>(), value[1].get<float>(), value[2].get<float>(), value[3].get<float>());
                            else if (key == "BonesColor") configSection.BonesColor = ImVec4(value[0].get<float>(), value[1].get<float>(), value[2].get<float>(), value[3].get<float>());
                            else if (key == "BonesColorVisible") configSection.BonesColorVisible = ImVec4(value[0].get<float>(), value[1].get<float>(), value[2].get<float>(), value[3].get<float>());
                            else if (key == "C4Color") configSection.C4Color = ImVec4(value[0].get<float>(), value[1].get<float>(), value[2].get<float>(), value[3].get<float>());
                            else if (key == "SpectatorListColor") configSection.SpectatorListColor = ImVec4(value[0].get<float>(), value[1].get<float>(), value[2].get<float>(), value[3].get<float>());
                        }
                        else if constexpr (std::is_same_v<T, Structs::RadarConfig>)
                        {
                            if (key == "Enabled") configSection.Enabled = value.get<bool>();
                            else if (key == "Host") configSection.Host = value.get<std::string>();
                            else if (key == "StaticPort") configSection.StaticPort = value.get<int>();
                            else if (key == "IngestPort") configSection.IngestPort = value.get<int>();
                            else if (key == "PublishIntervalMs") configSection.PublishIntervalMs = value.get<int>();
                            else if (key == "HttpTimeoutMs") configSection.HttpTimeoutMs = value.get<int>();
                            else if (key == "ReconnectBaseMs") configSection.ReconnectBaseMs = value.get<int>();
                            else if (key == "ReconnectMaxMs") configSection.ReconnectMaxMs = value.get<int>();
                        }
                    }
                }
            }
            else
            {
                LOG_ERROR("Config file missing '{}' section", sectionName);
            }
        }
    };
}

// Global config instance
inline Config::AppConfig& config = Config::AppConfig::Get();
