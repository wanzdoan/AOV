#pragma once
// =============================================================================
//  aov_config.hpp — Global hack config + feature flags for AOV module.
//  Ported from Mod's Config.hpp. All features read these at runtime.
//  Game: com.garena.game.kgvn 1.63.1.14
// =============================================================================

#include <cstdint>

// ---- Aim targeting mode enum ------------------------------------------------
enum AimType {
    AIM_LOW_HP_PCT = 0,   // Target enemy with lowest HP%
    AIM_LOW_HP     = 1,   // Target enemy with lowest absolute HP
    AIM_NEAREST    = 2,   // Target nearest enemy
    AIM_NEAREST_CROSS = 3,// Target nearest to crosshair/aim ray
    AIM_OFF        = 999  // Aim disabled
};

// ---- Global hack config struct (written by menu, read by hooks) -------------
struct HackConfig {
    bool  hackMapV2;        // +0x00  map hack V2 (LVActorLinker.SetVisible)
    bool  timeHoiChieu;     // +0x01  show skill cooldowns in name label
    uint8_t _p2[2];
    bool  rangeEnable;      // +0x04  camera zoom-out enable
    bool  unlockFps120;     // +0x05  unlock 120fps
    bool  aimEnable;        // +0x06  aimbot enable
    bool  drawAimRay;       // +0x07  draw aim ray (Vẽ Tia Aim)
    bool  aimC1;            // +0x08  aim C1 (slot 1)
    bool  aimC2;            // +0x09  aim C2 (slot 2)
    bool  aimUlti;          // +0x0A  aim ulti (slot 3)
    uint8_t _p0B[5];
    int32_t pthp;           // +0x10  HP% threshold for self-cast (slot 9)
    float range1;           // +0x14  Rangeskill1
    float range2;           // +0x18  Rangeskill2
    float range3;           // +0x1C  Rangeskill3
    float kCachDon;         // +0x20  K.Cách Đón (lead distance)
    int32_t aimType;        // +0x24  aim mode (AimType enum)
    float rangeValue;       // +0x28  camera zoom-out value
    uint8_t _p2C[8];
    bool  unlockAllSkins;   // +0x34  unlock all skins
    uint8_t _p35[7];
    int32_t killNotifyIdx;  // +0x3C  kill-notify spinner index
    int32_t killNotifySkinId;//+0x40  actual broadcastID
    int32_t buttonSkinIdx;  // +0x44  button skin spinner index
    int32_t buttonSkinId;   // +0x48  actual PersonalButtonID
    bool  autoBanDo;        // +0x4C  auto sell/rebuy equipment
    uint8_t _tail[3];
};

extern HackConfig cfg;
inline HackConfig* GetHackConfig() { return &cfg; }

// ---- Hot-path mirror globals (also written by menu) -------------------------
// Aim
extern bool  AimSkill, AimSkill1, AimSkill2, AimSkill3;
extern int   aimType;
// ESP / Auto
extern bool  EspElsu;   // Vẽ Tia Aim (draw aim ray)
extern bool  Ksbp;      // Auto Bộc Phá (auto flash/execute)
extern bool  Kstt;      // Auto Trừng Trị (auto smite)
extern bool  Killm;     // is HP under Punish execute threshold (cached)
extern bool  Willbp;    // request: cast slot-5 as Bộc Phá
extern bool  Willtt;    // request: cast slot-5 as Trừng Trị
// Aim ranges
extern float Rangeskill1, Rangeskill2, Rangeskill3, KCachDon, PviChieu;
// Aim state (produced by CSkillButtonManager.UpdateLogic)
extern bool  isCharging;
extern int   skillSlot;
extern float Pthp;      // HP% threshold for auto slot-9 cast
// Trừng Trị target flags
extern bool  g_TaThanRong;   // punish Dark Slayer + Dragon
extern bool  g_BuaXanhDo;    // punish blue + red buff
// Map hack helpers
extern bool  bShowAvatar;
extern bool  IsShowNameInfo;
// Auto bán đồ state (stored separately, not in cfg to avoid overlap)
extern bool g_autoSellSel[8];
extern int  g_autoSellOrder[8];
extern bool g_banMua;

// ---- Skin-id lookup tables & names (defined in aov_config.cpp) ---------------
extern const int32_t kKillNotifyTable[62];
extern const char*   kKillNotifyNames[62];
extern const int32_t kButtonSkinTable[74];
extern const char*   kButtonSkinNames[74];
