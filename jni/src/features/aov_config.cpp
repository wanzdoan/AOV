// aov_config.cpp — global config + mirror variable definitions + skin tables.
#include "include/aov_config.hpp"

HackConfig cfg = {};

bool  AimSkill=false, AimSkill1=false, AimSkill2=false, AimSkill3=false;
int   aimType=AIM_OFF;
bool  EspElsu=false, Ksbp=false, Kstt=false, Killm=false;
bool  Willbp=false, Willtt=false;
bool  isCharging=false;
int   skillSlot=0;
float Pthp=0.0f;
float Rangeskill1=0.0f, Rangeskill2=0.0f, Rangeskill3=0.0f, KCachDon=0.0f, PviChieu=0.0f;
bool  g_TaThanRong=false, g_BuaXanhDo=false;
bool  bShowAvatar=false, IsShowNameInfo=false;

bool g_autoSellSel[8]   = {false,false,false,false,false,false,false,false};
int  g_autoSellOrder[8] = {0,0,0,0,0,0,0,0};
bool g_banMua = false;

// Kill-notify billboard IDs (ResBillboardCfg.iBillboardID).
// idx 0 = Tắt (off). Gaps at 39/40/41 are skipped.
const int32_t kKillNotifyTable[62] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
    24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 42, 43, 44, 45, 46, 47, 48,
    49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64
};

const char* kKillNotifyNames[62] = {
    "Tắt",
    "TV Thần Tài",
    "Hayate TDTD",
    "Ngộ Không NVT",
    "Điêu Thuyền Sailor Moon",
    "Alice Chibi",
    "Eland'orr Tuxedo",
    "Butterfly Thánh Nữ",
    "Enzo Kurapika",
    "Nakroth Killua",
    "Raz Gon",
    "Yena HCT",
    "Airi Thứ Nguyên",
    "Murad Thần Binh",
    "Grakk Thần Bí",
    "Veres LYLM",
    "Nakroth Quỷ Thương",
    "Aya Công Chúa",
    "Nakroth Producer",
    "Krixi Thời Không",
    "Nakroth Bạch Diện",
    "Murad Tiên Đế",
    "Veera Phù Thủy",
    "Liliana Ma Pháp",
    "Biron Yuji",
    "Tulen Gojo",
    "Ilumia Lưỡng Nghi",
    "Valhein Vũ Thần",
    "Violet Thứ Nguyên",
    "Capheny Càn Nguyên",
    "Allain LSV",
    "Tel'Annas Lân Quang",
    "Butterfly Bình Minh",
    "Violet Nobara",
    "Paine Megumi",
    "Yorn Conan Edogawa",
    "Hayate Siêu Trộm Kid",
    "Kaine Thợ Săn",
    "Tel'Annas TN Vũ Thần",
    "Stuart Siêu Trộm",
    "Capheny Bugcag",
    "Khung Hạ Gục #44",
    "Nakroth TN Vũ Thần",
    "Elsu Tàu Mơ",
    "Eland'orr Tàu Mơ",
    "Khung Hạ Gục #48",
    "Lauriel TN Vũ Thần",
    "Iggy Rimuru",
    "Qi Milim",
    "Aya Cinnamoroll's Dream",
    "Arthur Pompompurin's Oath",
    "Natalya Kuromi's Heart",
    "Veera My Melody's Love",
    "Khung Hạ Gục #56",
    "Khung Hạ Gục #57",
    "Khung Hạ Gục #58",
    "Khung Hạ Gục #59",
    "Khung Hạ Gục #60",
    "Khung Hạ Gục #61",
    "Khung Hạ Gục #62",
    "Khung Hạ Gục #63",
    "Khung Hạ Gục #64"
};

// Button skin IDs (ResPersonalButtonCfg.iID).
// idx 0 = Tắt (off). Gaps at 18/41/51/66 are skipped.
const int32_t kButtonSkinTable[74] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 19, 20, 21, 22, 23, 24,
    25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 42, 43, 44, 45, 46, 47,
    48, 49, 50, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 67, 68, 69, 70, 71,
    72, 73, 74, 75, 76, 77
};

const char* kButtonSkinNames[74] = {
    "Tắt",
    "Lauriel Thứ nguyên vệ thần",
    "Eternal Sailor Moon",
    "Eland'orr-Tuxedo",
    "Violet Thần long tỷ tỷ",
    "Alice - Eternal Sailor Chibi Moon",
    "Butterfly Kim ngư thần nữ",
    "Butterfly Thánh nữ khởi nguyên",
    "Raz Gon",
    "Enzo Kurapika",
    "Nakroth Killua",
    "Murad Tuyệt thế thần binh",
    "Airi Thứ nguyên Vệ thần",
    "Yena Huyền cửu thiên",
    "Veres Lưu ly Long mẫu",
    "Nakroth Quỷ thương Liệp Đế",
    "Aya Công chúa cầu vồng",
    "Nakroth Bạch diện chiến thương",
    "Krixi Phù thủy thời không",
    "Murad Thiên Luân Kiếm Thánh",
    "Liliana Ma Pháp Tối Thượng",
    "Biron Yuji Itadori",
    "Tulen Satoru Gojo",
    "Ilumia Lưỡng Nghi Long Hậu",
    "Valhein Thứ nguyên vệ thần",
    "Violet Thứ nguyên vệ thần",
    "Capheny Càn Nguyên Điện Chủ",
    "Billow Thiên Tướng - Độ Ách",
    "Bolt Baron Thiên Phủ - Tư Mệnh",
    "Veera Thất Sát - Thượng Sinh",
    "Yena Trấn Yêu Thần Lộc",
    "Tel'Annas Lân Quang Thánh Điệu",
    "Butterfly Bình minh tận thế",
    "Violet Nobara Kugisaki",
    "Paine Megumi Fushiguro",
    "Zephys Kỷ Nguyên Hổ Phách",
    "Florentino Kỷ Nguyên Hổ Phách",
    "Tel'Annas Kỷ Nguyên Hổ Phách",
    "Bijan Lữ Hành Thời Không",
    "Rouie Linh Sứ Thời không",
    "Stuart Siêu trùm phản diện",
    "Triệu Vân Chiến thần vô song",
    "Tel'Annas Thứ nguyên vệ thần",
    "Yorn Conan Edogawa",
    "Hayate Siêu đạo chích Kid",
    "Dolia (Skin mới)",
    "Yue Hỗn Độn Thần Ma",
    "Capheny Bugcag Assemble",
    "Volkath Ma ảnh thần đao",
    "Nakroth Thứ nguyên vệ thần",
    "Eland'orr Mộng giới thần chủ",
    "Iggy Rimuru Tempest",
    "Qi Milim Nava",
    "Billow T-Rex Bất Bại",
    "Zephys (Skin mới)",
    "Arthur Pompompurin's Oath",
    "Natalya Kuromi's Heart",
    "Aya Cinnamoroll's Dream",
    "Veera My Melody's Love",
    "Marja Hi Phi",
    "Omen Liệt Hỏa Thiên Cang",
    "Điêu Thuyền Nhật Nguyệt Thánh Linh",
    "Lauriel Mã Đằng Cửu Thế",
    "Dolia Mã Khởi Thiên Ca",
    "Valhein Mã Hành Vạn Lý",
    "Krixi Kimono",
    "Aoi Mikasa Ackermann",
    "Cresht Eren Jaegar",
    "Nakroth Levi",
    "Qi Annie Leonhart",
    "Paine (Skin mới)",
    "Tulen Thiên Cơ Bạch Trạch",
    "Hayate (Skin mới)",
    "Natalya (Skin mới)"
};
