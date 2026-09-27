// The First Frost: native ImGui implementation of Downloads/menu/menu.html.
// UI-only: existing feature configuration and lookup tables remain authoritative.
#include "menu.hpp"
#include "../esp/esp.hpp"
#include "../../include/globals.hpp"
#include "../../include/aov_config.hpp"
#include "include/config_store.hpp"
#include "../../include/game_actors.hpp"
#include "../../vendor/imgui/imgui.h"
#include "../../vendor/imgui/imgui_internal.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace Menu {
static bool s_visible = false, s_hidden = false;
static int s_tab = 0, s_popup = 0, s_openPopup = 0;
static float s = 1.f;
static ImVec2 s_position(-1, -1), s_button(-1, -1), s_screen(0, 0);
static ImVec2 s_card;
static float s_cardWidth, s_innerX, s_innerWidth, s_subY;
static bool s_hasSub = false;
static bool s_sliderActive = false;
static ImDrawListSplitter s_cardLayers;
static ImFont* s_regular = nullptr;
static ImFont* s_bold = nullptr;
static constexpr ImU32 BG = IM_COL32(9,9,11,255);
static constexpr ImU32 SURFACE = IM_COL32(17,17,20,255);
static constexpr ImU32 RAISED = IM_COL32(23,23,28,255);
static constexpr ImU32 BORDER = IM_COL32(36,36,44,255);
static constexpr ImU32 BORDER_LIGHT = IM_COL32(50,50,61,255);
static constexpr ImU32 WHITE = IM_COL32(255,255,255,255);
static constexpr ImU32 DIM = IM_COL32(142,142,152,255);
static constexpr ImU32 GREEN = IM_COL32(34,197,94,255);
static constexpr ImU32 YELLOW = IM_COL32(245,158,11,255);
static constexpr ImU32 RED = IM_COL32(239,68,68,255);
static const char* targets[] = {"Tắt", "% Máu Thấp Nhất", "Máu Thấp Nhất",
    "Khoảng Cách Gần Nhất", "Khoảng Cách Gần Tia Ngắm Nhất"};
static const int targetValues[] = {AIM_OFF, AIM_LOW_HP_PCT, AIM_LOW_HP, AIM_NEAREST, AIM_NEAREST_CROSS};
static const char* choiceName(int popup,int index) {
    if(popup==1) return targets[index];
    if(index==0) return "Mặc Định";
    return popup==2?kButtonSkinNames[index]:kKillNotifyNames[index];
}
static int targetIndex() {
    for (int i=0; i<5; ++i) if (aimType == targetValues[i]) return i;
    return 0;
}
static ImVec2 add(ImVec2 a, float x, float y) { return ImVec2(a.x+x*s, a.y+y*s); }
void setFonts(ImFont* regular, ImFont* bold) {
    s_regular=regular; s_bold=bold?bold:regular;
}
static ImFont* font(bool bold=false) {
    ImFont* f=bold?s_bold:s_regular;
    return f?f:ImGui::GetFont();
}
// stb_truetype uses ascent-to-descent pixel height; CSS font-size is an em.
// Match the reference's Segoe/Roboto visual size, not just its numeric size.
static float fontSize(float cssPixels) { return cssPixels*1.35f*s; }
static void text(ImVec2 p, const char* value, float size=14.f, ImU32 color=WHITE, float wrap=0.f, bool bold=false) {
    ImGui::GetWindowDrawList()->AddText(font(bold || color!=DIM), fontSize(size), p, color, value, nullptr, wrap);
}
static void trackedText(ImVec2 p,const char* value,float size,float spacing,ImU32 color=WHITE) {
    auto* face=font(true);
    while(*value) {
        unsigned int codepoint;
        int length=ImTextCharFromUtf8(&codepoint,value,nullptr);
        if(length<=0) break;
        ImGui::GetWindowDrawList()->AddText(face,fontSize(size),p,color,value,value+length);
        p.x+=face->CalcTextSizeA(fontSize(size),FLT_MAX,0,value,value+length).x+spacing*s;
        value+=length;
    }
}
static float textHeight(const char* value, float size, float width) {
    return font(size>12)->CalcTextSizeA(fontSize(size), FLT_MAX, width, value).y;
}
static void cursor(ImVec2 p) { ImGui::SetCursorScreenPos(p); }
static bool hit(const char* id, ImVec2 p, ImVec2 size) {
    cursor(p);
    return ImGui::InvisibleButton(id, size);
}
void toggle() {
    s_visible = !s_visible;
    if (s_visible) s_hidden = false;
}
bool isVisible() { return s_visible; }

void applyStyle() {
    // Reset before external ScaleAllSizes(), so rotation never compounds sizes.
    ImGuiStyle& st = ImGui::GetStyle();
    st = ImGuiStyle();
    st.WindowRounding=10; st.ChildRounding=0; st.FrameRounding=6;
    st.PopupRounding=8; st.ScrollbarRounding=4; st.GrabRounding=99;
    st.WindowBorderSize=1; st.ChildBorderSize=0; st.FrameBorderSize=1;
    st.WindowPadding=ImVec2(0,0); st.FramePadding=ImVec2(14,10);
    st.ItemSpacing=ImVec2(0,0); st.ScrollbarSize=6;
    auto color=[](ImU32 c) { return ImGui::ColorConvertU32ToFloat4(c); };
    auto* c=st.Colors;
    c[ImGuiCol_WindowBg]=color(BG); c[ImGuiCol_ChildBg]=color(BG);
    c[ImGuiCol_PopupBg]=color(IM_COL32(13,13,16,255));
    c[ImGuiCol_Text]=color(WHITE); c[ImGuiCol_TextDisabled]=color(DIM);
    c[ImGuiCol_Border]=color(BORDER); c[ImGuiCol_Separator]=color(BORDER);
    c[ImGuiCol_FrameBg]=color(RAISED); c[ImGuiCol_FrameBgHovered]=color(BORDER);
    c[ImGuiCol_FrameBgActive]=color(BORDER_LIGHT);
    c[ImGuiCol_Button]=color(RAISED); c[ImGuiCol_ButtonHovered]=color(BORDER);
    c[ImGuiCol_ButtonActive]=color(BORDER_LIGHT);
    c[ImGuiCol_CheckMark]=color(GREEN); c[ImGuiCol_SliderGrab]=color(GREEN);
    c[ImGuiCol_SliderGrabActive]=color(WHITE);
    c[ImGuiCol_ScrollbarBg]=ImVec4(0,0,0,0);
    c[ImGuiCol_ScrollbarGrab]=color(BORDER);
    c[ImGuiCol_ScrollbarGrabHovered]=color(BORDER_LIGHT);
    c[ImGuiCol_ScrollbarGrabActive]=color(DIM);
    c[ImGuiCol_ModalWindowDimBg]=ImVec4(0,0,0,.85f);
}
static void heading(const char* label) {
    ImVec2 p=ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddRectFilled(p, add(p,4,14), GREEN,2*s);
    trackedText(add(p,12,0),label,12,1.4f);
    cursor(add(p,0,30));
}
static void beginCard(const char* id) {
    ImGui::PushID(id);
    s_card=ImGui::GetCursorScreenPos();
    s_cardWidth=ImGui::GetContentRegionAvail().x;
    s_innerX=s_card.x+16*s; s_innerWidth=s_cardWidth-32*s;
    s_hasSub=false;
    auto* dl=ImGui::GetWindowDrawList();
    s_cardLayers.Split(dl,2); s_cardLayers.SetCurrentChannel(dl,1);
}
static void endCard() {
    auto* dl=ImGui::GetWindowDrawList();
    ImVec2 end(s_card.x+s_cardWidth,ImGui::GetCursorScreenPos().y);
    s_cardLayers.SetCurrentChannel(dl,0);
    dl->AddRectFilled(s_card,end,SURFACE,8*s);
    if(s_hasSub) dl->AddRectFilled(ImVec2(s_card.x+1,s_subY),ImVec2(end.x-1,end.y-1),
        IM_COL32(9,9,12,255),8*s,ImDrawFlags_RoundCornersBottom);
    dl->AddRect(s_card,end,ImGui::IsMouseHoveringRect(s_card,end)?BORDER_LIGHT:BORDER,8*s);
    s_cardLayers.Merge(dl);
    cursor(ImVec2(s_card.x,end.y+10*s));
    ImGui::PopID();
}
static void attached() {
    ImVec2 p(s_card.x,ImGui::GetCursorScreenPos().y);
    s_subY=p.y;
    s_hasSub=true;
    // Filled in the card background channel once its full height is known.
    auto* dl=ImGui::GetWindowDrawList();
    dl->AddLine(p,ImVec2(p.x+s_cardWidth,p.y),BORDER,s);
    cursor(ImVec2(s_innerX,p.y+14*s));
}
static void subHeading(const char* label) {
    ImVec2 p(s_innerX,ImGui::GetCursorScreenPos().y);
    trackedText(p,label,11,.8f,DIM);
    cursor(add(p,0,24));
}
static bool toggleRow(const char* id, const char* name, const char* desc, bool& value, bool sub=false) {
    auto* dl=ImGui::GetWindowDrawList();
    ImVec2 p(s_innerX,ImGui::GetCursorScreenPos().y);
    float tw=std::max(40*s,s_innerWidth-60*s);
    float nameH=textHeight(name,14,tw);
    float descH=desc && *desc ? textHeight(desc,12,tw) : 0;
    float h=std::max(52*s,28*s+nameH+(descH?3*s+descH:0));
    bool changed=hit(id,ImVec2(s_card.x,p.y),ImVec2(s_cardWidth,h));
    if(changed) value=!value;
    text(add(p,0,14),name,14,WHITE,tw);
    if(descH) text(ImVec2(p.x,p.y+14*s+nameH+3*s),desc,12,DIM,tw);
    ImVec2 a(p.x+s_innerWidth-44*s,p.y+(h-24*s)*.5f), b=add(a,44,24);
    ImGuiID animId=ImGui::GetID(id);
    auto* storage=ImGui::GetStateStorage();
    float t=storage->GetFloat(animId,value?1.f:0.f);
    t+=(float(value)-t)*std::min(1.f,ImGui::GetIO().DeltaTime*16);
    storage->SetFloat(animId,t);
    if(value) dl->AddRect(a,b,IM_COL32(34,197,94,42),12*s,0,5*s);
    dl->AddRectFilled(a,b,value?GREEN:IM_COL32(30,30,36,255),12*s);
    dl->AddRect(a,b,value?GREEN:IM_COL32(46,46,56,255),12*s);
    dl->AddCircleFilled(add(a,12+20*t,12),9*s,value?WHITE:IM_COL32(113,113,122,255));
    cursor(ImVec2(sub?s_innerX:s_card.x,p.y+h));
    return changed;
}
static void slider(const char* label, float& value, float min, float max, float step, const char* format) {
    ImGui::PushID(label);
    ImVec2 p(s_innerX,ImGui::GetCursorScreenPos().y);
    text(p,label,12.5f);
    char buf[40]; snprintf(buf,sizeof(buf),format,value);
    float bw=std::max(36*s,font(true)->CalcTextSizeA(fontSize(12),FLT_MAX,0,buf).x+20*s);
    ImVec2 b(p.x+s_innerWidth-bw,p.y-3*s);
    auto* dl=ImGui::GetWindowDrawList();
    dl->AddRectFilled(b,ImVec2(b.x+bw,b.y+22*s),IM_COL32(34,197,94,25),11*s);
    dl->AddRect(b,ImVec2(b.x+bw,b.y+22*s),IM_COL32(34,197,94,76),11*s);
    text(add(b,10,4),buf,12,GREEN);
    ImVec2 area=add(p,0,22);
    hit("range",area,ImVec2(s_innerWidth,30*s));
    s_sliderActive |= ImGui::IsItemActive();
    float left=p.x+9*s, width=std::max(1.f,s_innerWidth-18*s);
    if(ImGui::IsItemActive() && ImGui::IsMouseDown(0)) {
        float t=std::clamp((ImGui::GetIO().MousePos.x-left)/width,0.f,1.f);
        value=std::clamp(min+std::round(t*(max-min)/step)*step,min,max);
    }
    float t=std::clamp((value-min)/(max-min),0.f,1.f), y=area.y+15*s;
    dl->AddRectFilled(ImVec2(left,y-3*s),ImVec2(left+width,y+3*s),IM_COL32(34,34,42,255),3*s);
    dl->AddRectFilled(ImVec2(left,y-3*s),ImVec2(left+width*t,y+3*s),GREEN,3*s);
    dl->AddCircleFilled(ImVec2(left+width*t,y),9*s,GREEN);
    dl->AddCircleFilled(ImVec2(left+width*t,y),7*s,WHITE);
    cursor(add(p,0,64));
    ImGui::PopID();
}
static void ticks() {
    ImVec2 p(s_innerX,ImGui::GetCursorScreenPos().y);
    float w=(s_innerWidth-20*s)/3;
    bool* values[]={&AimSkill1,&AimSkill2,&AimSkill3};
    const char* names[]={"C1","C2","C3"};
    auto* dl=ImGui::GetWindowDrawList();
    for(int i=0;i<3;++i) {
        ImVec2 a(p.x+i*(w+10*s),p.y), b(a.x+w,a.y+40*s);
        if(hit(names[i],a,ImVec2(w,40*s))) *values[i]=!*values[i];
        bool on=*values[i];
        dl->AddRectFilled(a,b,on?IM_COL32(11,28,19,255):RAISED,6*s);
        dl->AddRect(a,b,on?IM_COL32(34,197,94,102):BORDER,6*s);
        ImVec2 box=add(a,12,11);
        dl->AddRectFilled(box,add(box,18,18),on?GREEN:BG,4*s);
        dl->AddRect(box,add(box,18,18),on?GREEN:BORDER_LIGHT,4*s);
        if(on) {
            dl->AddLine(add(box,4,9),add(box,8,13),WHITE,2*s);
            dl->AddLine(add(box,8,13),add(box,14,5),WHITE,2*s);
        }
        text(add(a,40,12),names[i],13.5f);
    }
    cfg.aimC1=AimSkill1; cfg.aimC2=AimSkill2; cfg.aimUlti=AimSkill3;
    cursor(add(p,0,54));
}
static void selector(const char* id,const char* label,const char* description,const char* button,int popup,bool inset=false) {
    ImVec2 p(inset?s_innerX:s_card.x,ImGui::GetCursorScreenPos().y);
    float w=inset?s_innerWidth:s_cardWidth;
    float buttonW=std::min(w*.48f,font(true)->CalcTextSizeA(fontSize(13),FLT_MAX,0,button).x+40*s);
    float available=w-buttonW-48*s;
    float nh=textHeight(label,13.5f,available), dh=textHeight(description,12,available);
    float h=std::max(66*s,28*s+nh+3*s+dh);
    bool clicked=hit(id,p,ImVec2(w,h));
    bool hover=ImGui::IsItemHovered();
    auto* dl=ImGui::GetWindowDrawList();
    if(inset) {
        dl->AddRectFilled(p,ImVec2(p.x+w,p.y+h),hover?BORDER:RAISED,6*s);
        dl->AddRect(p,ImVec2(p.x+w,p.y+h),hover?BORDER_LIGHT:BORDER,6*s);
    }
    text(add(p,16,14),label,13.5f,WHITE,available);
    text(ImVec2(p.x+16*s,p.y+14*s+nh+3*s),description,12,DIM,available);
    ImVec2 a(p.x+w-buttonW-14*s,p.y+(h-34*s)/2);
    dl->AddRectFilled(a,ImVec2(a.x+buttonW,a.y+34*s),SURFACE,5*s);
    dl->AddRect(a,ImVec2(a.x+buttonW,a.y+34*s),hover?BORDER_LIGHT:BORDER,5*s);
    dl->PushClipRect(a,ImVec2(a.x+buttonW-24*s,a.y+34*s),true);
    text(add(a,10,10),button,13);
    dl->PopClipRect();
    ImVec2 arrow(a.x+buttonW-15*s,a.y+17*s);
    dl->AddLine(add(arrow,-4,-2),add(arrow,0,2),DIM,1.5f*s);
    dl->AddLine(add(arrow,0,2),add(arrow,4,-2),DIM,1.5f*s);
    if(clicked) s_openPopup=popup;
    cursor(ImVec2(p.x,p.y+h+(inset?14*s:0)));
}
static void panelMap() {
    heading("TÙY CHỌN BẢN ĐỒ");
    beginCard("map");
    toggleRow("enable","Hack Map","Mở toàn bộ bản đồ và hiển thị vị trí địch",cfg.hackMapV2);
    endCard();
    beginCard("cooldowns");
    toggleRow("enable","Show Time CD","Hiển thị thời gian hồi chiêu kỹ năng địch",cfg.timeHoiChieu);
    endCard();
    beginCard("camera");
    toggleRow("enable","Cam Xa","Mở rộng tầm nhìn của camera trò chơi",cfg.rangeEnable);
    if(cfg.rangeEnable) {
        attached();
        float value=cfg.rangeValue/.036186f;
        slider("Khoảng Cách Cam Xa",value,0,30,1,"%.0f");
        cfg.rangeValue=value*.036186f;
    }
    endCard();
}
static void panelAim() {
    heading("TỰ ĐỘNG & HỖ TRỢ NGẮM");
    beginCard("aim");
    toggleRow("enable","Aim","Tự động khóa mục tiêu và hỗ trợ kỹ năng",AimSkill);
    cfg.aimEnable=AimSkill;
    if(AimSkill) {
        attached(); subHeading("CHẾ ĐỘ AIM"); ticks();
        subHeading("MỤC TIÊU ƯU TIÊN");
        selector("target","Lựa Chọn Mục Tiêu Aim","Chạm để thay đổi đối tượng khóa mục tiêu",targets[targetIndex()],1,true);
        slider("Phạm Vi Aim",PviChieu,0,25,1,"%.0f");
        Rangeskill1=Rangeskill2=Rangeskill3=PviChieu;
        cfg.range1=cfg.range2=cfg.range3=PviChieu;
        slider("Khoảng Cách Đón Đầu",KCachDon,0,5,.1f,"%.1f");
        cfg.kCachDon=KCachDon;
        ImVec2 p(s_innerX,ImGui::GetCursorScreenPos().y);
        ImGui::GetWindowDrawList()->AddLine(p,ImVec2(p.x+s_innerWidth,p.y),BORDER,s);
        toggleRow("ray","Vẽ Tia Elsu","Hiển thị đường ngắm kỹ năng",EspElsu,true);
        cfg.drawAimRay=EspElsu;
    }
    endCard();
    beginCard("execute");
    toggleRow("enable","Auto Bộc Phá","Tự động kích hoạt khi kẻ địch vào ngưỡng máu",Ksbp);
    endCard();
    beginCard("punish");
    toggleRow("enable","Auto Trừng Trị","Tự động trừng trị quái rừng khi chạm mốc máu",Kstt);
    if(Kstt) {
        attached(); subHeading("MỤC TIÊU TRỪNG TRỊ");
        toggleRow("boss","Caesar & Rồng","",g_TaThanRong,true);
        ImVec2 divider(s_innerX,ImGui::GetCursorScreenPos().y);
        ImGui::GetWindowDrawList()->AddLine(divider,ImVec2(divider.x+s_innerWidth,divider.y),BORDER,s);
        toggleRow("buff","Bùa Xanh & Bùa Đỏ","",g_BuaXanhDo,true);
    }
    endCard();
}
static void panelSkin() {
    heading("TRANG PHỤC & GIAO DIỆN");
    beginCard("skins");
    toggleRow("enable","Unlock Full Skin","Mở khóa ngoại hình và hiệu ứng trang phục",cfg.unlockAllSkins);
    endCard();
    beginCard("buttons");
    selector("choose","Unlock Nút Bấm",choiceName(2,std::clamp(cfg.buttonSkinIdx,0,73)),"Chọn",2);
    endCard();
    beginCard("notify");
    selector("choose","Unlock Thông Báo Hạ",choiceName(3,std::clamp(cfg.killNotifyIdx,0,61)),"Chọn",3);
    endCard();
}
static void statusCard(ImVec2 p,float w,float height,const char* label,const char* value,ImU32 color=WHITE) {
    auto* dl=ImGui::GetWindowDrawList();
    dl->AddRectFilled(p,ImVec2(p.x+w,p.y+height),SURFACE,8*s);
    dl->AddRect(p,ImVec2(p.x+w,p.y+height),BORDER,8*s);
    text(add(p,16,16),label,11,DIM,w-32*s);
    text(add(p,16,42),value,16,color,w-32*s);
}
static void panelStatus() {
    heading("TRẠNG THÁI HỆ THỐNG");
    bool ready=il2cppExports::il2cpp_class_from_name && il2cppExports::il2cpp_domain_get;
    char fps[32],resolution[64],frameTime[32],uptime[40],scale[32],features[40],saved[40];
    const float frameRate=ImGui::GetIO().Framerate;
    snprintf(fps,sizeof(fps),"%.0f FPS",frameRate);
    snprintf(frameTime,sizeof(frameTime),"%.2f ms",frameRate>0?1000.f/frameRate:0.f);
    const int seconds=int(ImGui::GetTime());
    snprintf(uptime,sizeof(uptime),"%02d:%02d:%02d",seconds/3600,(seconds/60)%60,seconds%60);
    snprintf(scale,sizeof(scale),"%.2f× · OpenGL ES 3",Global::uiScale);
    snprintf(resolution,sizeof(resolution),"%d × %d",Global::screenWidth,Global::screenHeight);
    int active=int(cfg.hackMapV2)+int(cfg.timeHoiChieu)+int(cfg.rangeEnable)+int(AimSkill)+
        int(Ksbp)+int(Kstt)+int(cfg.unlockAllSkins)+int(cfg.unlockFps120)+int(EspElsu)+
        int(cfg.buttonSkinIdx>0)+int(cfg.killNotifyIdx>0);
    snprintf(features,sizeof(features),"%d / 11 tính năng",active);
    std::time_t last=ConfigStore::lastSaved();
    std::tm* local=last?std::localtime(&last):nullptr;
    if(local) std::strftime(saved,sizeof(saved),"%d/%m · %H:%M:%S",local);
    else snprintf(saved,sizeof(saved),"Chưa có bản lưu");
    const char* labels[]={"TRÌNH KẾT NỐI GAME","CẢM ỨNG","TỐC ĐỘ KHUNG HÌNH","THỜI GIAN KHUNG HÌNH",
        "PHIÊN BẢN MODULE","PHIÊN BẢN GAME HỖ TRỢ","ĐỘ PHÂN GIẢI","TỈ LỆ & ĐỒ HỌA",
        "THỜI GIAN CHẠY MENU","ĐANG BẬT","LƯU CẤU HÌNH","LƯU GẦN NHẤT",
        "NÚT BẤM ĐANG CHỌN","THÔNG BÁO HẠ ĐANG CHỌN","BẢO VỆ ANTI-BAN"};
    const char* values[]={ready?"Sẵn sàng":"Đang khởi tạo",Esp::isTouchReady()?"Hoạt động":"Đang chờ",
        fps,frameTime,"v2.6.0 · First Frost","AOV 1.63.1.14 · ARM64",resolution,scale,uptime,features,
        ConfigStore::status(),saved,choiceName(2,std::clamp(cfg.buttonSkinIdx,0,73)),
        choiceName(3,std::clamp(cfg.killNotifyIdx,0,61)),"Chưa xác minh"};
    ImVec2 p=ImGui::GetCursorScreenPos();
    float avail=ImGui::GetContentRegionAvail().x;
    int columns=avail>=520*s?2:1;
    float w=(avail-(columns-1)*12*s)/columns;
    constexpr int count=sizeof(labels)/sizeof(labels[0]);
    for(int row=0;row<count;row+=columns) {
        float height=92*s;
        for(int j=row;j<std::min(row+columns,count);++j)
            height=std::max(height,58*s+textHeight(values[j],16,w-32*s));
        for(int j=row;j<std::min(row+columns,count);++j) {
            ImU32 color=j==0?(ready?GREEN:YELLOW):j==1?(Esp::isTouchReady()?GREEN:YELLOW):
                j==10?(ConfigStore::hasError()?RED:ConfigStore::enabled()?GREEN:DIM):j==14?DIM:WHITE;
            statusCard(ImVec2(p.x+(j-row)*(w+12*s),p.y),w,height,labels[j],values[j],color);
        }
        p.y+=height+12*s;
    }
    cursor(p);
    ImGui::Dummy(ImVec2(1,1));
}
// A vertical gesture cancels a pending row click. Sliders and scrollbars retain
// their own drag ownership, so adjusting a value cannot scroll its container.
static void touchScroll() {
    auto* window=ImGui::GetCurrentWindow();
    auto& io=ImGui::GetIO();
    auto* state=ImGui::GetStateStorage();
    ImGuiID id=ImGui::GetID("##touch-scroll");
    if(ImGui::IsMouseClicked(0))
        state->SetInt(id,ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)?1:0);
    int phase=state->GetInt(id);
    if(!ImGui::IsMouseDown(0)) { state->SetInt(id,0); return; }
    if(!phase || s_sliderActive || GImGui->ActiveId==ImGui::GetWindowScrollbarID(window,ImGuiAxis_Y)) return;
    ImVec2 delta=ImGui::GetMouseDragDelta(0,8*s);
    if(phase==1 && std::fabs(delta.y)>8*s && std::fabs(delta.y)>std::fabs(delta.x)) {
        phase=2; state->SetInt(id,2);
    }
    if(phase==2) {
        ImGui::ClearActiveID();
        ImGui::SetScrollY(std::clamp(ImGui::GetScrollY()-io.MouseDelta.y,0.f,ImGui::GetScrollMaxY()));
    }
}
static void modal() {
    if(s_openPopup) {
        s_popup=s_openPopup; s_openPopup=0;
        ImGui::OpenPopup("##frost-choice");
    }
    if(!ImGui::IsPopupOpen("##frost-choice")) { s_popup=0; return; }
    ImGuiIO& io=ImGui::GetIO();
    float w=std::min(460*s,io.DisplaySize.x-24*s);
    int count=s_popup==1?5:s_popup==2?74:62;
    float naturalHeight=86*s;
    for(int i=0;i<count;++i) {
        const char* name=choiceName(s_popup,i);
        naturalHeight+=std::max(44*s,textHeight(name,13.5f,w-82*s)+24*s)+6*s;
    }
    float h=std::min(naturalHeight,std::min(460*s,io.DisplaySize.y*.8f));
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x*.5f,io.DisplaySize.y*.5f),ImGuiCond_Always,ImVec2(.5f,.5f));
    ImGui::SetNextWindowSize(ImVec2(w,h));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(0,0));
    if(ImGui::BeginPopupModal("##frost-choice",nullptr,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings)) {
        ImVec2 p=ImGui::GetWindowPos();
        auto* dl=ImGui::GetWindowDrawList();
        dl->AddRectFilled(p,ImVec2(p.x+w,p.y+58*s),SURFACE,8*s,ImDrawFlags_RoundCornersTop);
        const char* title=s_popup==1?"LỰA CHỌN MỤC TIÊU AIM":s_popup==2?"UNLOCK NÚT BẤM":"UNLOCK THÔNG BÁO HẠ";
        text(add(p,20,21),title,14);
        ImVec2 close=ImVec2(p.x+w-44*s,p.y+14*s);
        bool dismiss=hit("close",close,ImVec2(30*s,30*s));
        dl->AddRectFilled(close,add(close,30,30),RAISED,6*s);
        dl->AddRect(close,add(close,30,30),BORDER,6*s);
        dl->AddLine(add(close,10,10),add(close,20,20),DIM,1.5f*s);
        dl->AddLine(add(close,20,10),add(close,10,20),DIM,1.5f*s);
        cursor(add(p,14,72));
        ImGui::BeginChild("options",ImVec2(w-28*s,h-86*s),false);
        int selected=s_popup==1?targetIndex():s_popup==2?cfg.buttonSkinIdx:cfg.killNotifyIdx;
        for(int i=0;i<count;++i) {
            ImGui::PushID(i);
            const char* name=choiceName(s_popup,i);
            ImVec2 a=ImGui::GetCursorScreenPos();
            float ow=ImGui::GetContentRegionAvail().x;
            float oh=std::max(44*s,textHeight(name,13.5f,ow-54*s)+24*s);
            bool choose=hit("option",a,ImVec2(ow,oh)), hover=ImGui::IsItemHovered();
            auto* list=ImGui::GetWindowDrawList();
            list->AddRectFilled(a,ImVec2(a.x+ow,a.y+oh),i==selected?IM_COL32(12,38,24,255):hover?BORDER:SURFACE,6*s);
            list->AddRect(a,ImVec2(a.x+ow,a.y+oh),i==selected?GREEN:hover?BORDER_LIGHT:BORDER,6*s);
            text(add(a,14,12),name,13.5f,WHITE,ow-54*s);
            if(i==selected) {
                ImVec2 check(a.x+ow-25*s,a.y+oh*.5f);
                list->AddLine(add(check,-4,0),add(check,0,4),GREEN,2*s);
                list->AddLine(add(check,0,4),add(check,7,-5),GREEN,2*s);
            }
            if(choose) {
                if(s_popup==1) cfg.aimType=aimType=targetValues[i];
                else if(s_popup==2) { cfg.buttonSkinIdx=i; cfg.buttonSkinId=kButtonSkinTable[i]; }
                else { cfg.killNotifyIdx=i; cfg.killNotifySkinId=kKillNotifyTable[i]; }
                dismiss=true;
            }
            cursor(ImVec2(a.x,a.y+oh+6*s));
            ImGui::PopID();
        }
        touchScroll();
        ImGui::EndChild();
        if(ImGui::IsKeyPressed(ImGuiKey_Escape)) dismiss=true;
        if(ImGui::IsMouseClicked(0) && !ImGui::IsMouseHoveringRect(p,ImVec2(p.x+w,p.y+h),false)) dismiss=true;
        if(dismiss) { ImGui::CloseCurrentPopup(); s_popup=0; }
        // The native input hook must consume the backdrop as well as the box.
        Global::menuRect={0,0,io.DisplaySize.x,io.DisplaySize.y};
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();
}
static void floating() {
    float r=42*s;
    if(s_button.x<0) s_button=ImVec2(60*s,60*s);
    s_button.x=std::clamp(s_button.x,r,ImGui::GetIO().DisplaySize.x-r);
    s_button.y=std::clamp(s_button.y,r,ImGui::GetIO().DisplaySize.y-r);
    ImGui::SetNextWindowPos(ImVec2(s_button.x-r,s_button.y-r));
    ImGui::SetNextWindowSize(ImVec2(2*r,2*r));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(0,0));
    ImGui::Begin("##frost-launcher",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoNav);
    ImVec2 p=ImGui::GetWindowPos();
    bool tap=hit("launcher",p,ImVec2(2*r,2*r));
    static bool moved=false;
    if(ImGui::IsItemActivated()) moved=false;
    if(ImGui::IsItemActive() && ImGui::IsMouseDragging(0,5*s)) {
        moved=true;
        s_button.x+=ImGui::GetIO().MouseDelta.x;
        s_button.y+=ImGui::GetIO().MouseDelta.y;
    }
    if(tap && !moved) toggle();
    if(!s_hidden) {
        auto* dl=ImGui::GetWindowDrawList();
        ImVec2 c(p.x+r,p.y+r);
        const bool hover=ImGui::IsItemHovered(), pressed=ImGui::IsItemActive();
        dl->AddCircleFilled(c,r,IM_COL32(34,197,94,18),64);
        dl->AddCircleFilled(c,r-3*s,IM_COL32(34,197,94,35),64);
        dl->AddCircleFilled(c,r-6*s,IM_COL32(9,17,15,250),64);
        dl->AddCircle(c,r-6*s,hover?WHITE:GREEN,64,(pressed?3.f:1.5f)*s);
        dl->AddCircle(c,r-10*s,IM_COL32(112,239,163,32),64,s);
        ImVec2 frost=add(c,0,-10);
        for(int i=0;i<6;++i) {
            float a=i*IM_PI/3.f, ca=std::cos(a), sa=std::sin(a);
            dl->AddLine(frost,add(frost,15*ca,15*sa),IM_COL32(167,243,208,255),2*s);
            for(int sign:{-1,1}) {
                float b=a+sign*IM_PI/3.f;
                ImVec2 branch=add(frost,8*ca,8*sa);
                dl->AddLine(branch,add(branch,5*std::cos(b),5*std::sin(b)),GREEN,1.7f*s);
            }
        }
        const float titleWidth=font(true)->CalcTextSizeA(fontSize(14),FLT_MAX,0,"AOV").x;
        text(ImVec2(c.x-titleWidth/2,c.y+10*s),"AOV",14,WHITE);
    }
    Global::wmRect={p.x,p.y,p.x+2*r,p.y+2*r};
    ImGui::End(); ImGui::PopStyleVar();
}
static void tabIcon(ImVec2 center,int index,ImU32 color) {
    if(index==0) {
        auto* dl=ImGui::GetWindowDrawList();
        ImVec2 points[]={add(center,-10,-6),add(center,-4,-10),add(center,4,-6),
            add(center,10,-10),add(center,10,6),add(center,4,10),add(center,-4,6),add(center,-10,10)};
        dl->AddPolyline(points,8,color,ImDrawFlags_Closed,1.6f*s);
        dl->AddLine(add(center,-4,-10),add(center,-4,6),color,1.6f*s);
        dl->AddLine(add(center,4,-6),add(center,4,10),color,1.6f*s);
    } else if(index==4) {
        ImVec2 points[]={add(center,-10,0),add(center,-6,0),add(center,-3,-8),add(center,3,8),add(center,6,0),add(center,10,0)};
        ImGui::GetWindowDrawList()->AddPolyline(points,6,color,0,1.8f*s);
    } else if(index==1) {
        auto* dl=ImGui::GetWindowDrawList();
        dl->AddCircle(center,9*s,color,24,1.8f*s);
        dl->AddCircle(center,4*s,color,16,1.8f*s);
        for(int i=0;i<4;++i) {
            float a=i*IM_PI*.5f;
            dl->AddLine(add(center,7*std::cos(a),7*std::sin(a)),
                add(center,12*std::cos(a),12*std::sin(a)),color,1.8f*s);
        }
    } else if(index==2) {
        auto* dl=ImGui::GetWindowDrawList();
        ImVec2 pts[10];
        for(int i=0;i<10;++i) {
            float a=-IM_PI*.5f+i*IM_PI/5;
            float r=i%2?4.5f:10.f;
            pts[i]=add(center,r*std::cos(a),r*std::sin(a));
        }
        dl->AddPolyline(pts,10,color,ImDrawFlags_Closed,1.8f*s);
    } else {
        auto* dl=ImGui::GetWindowDrawList();
        dl->AddCircle(center,7*s,color,24,1.8f*s);
        dl->AddCircle(center,3*s,color,16,1.8f*s);
        for(int i=0;i<8;++i) {
            float a=i*IM_PI*.25f;
            dl->AddLine(add(center,7*std::cos(a),7*std::sin(a)),
                add(center,10*std::cos(a),10*std::sin(a)),color,2.5f*s);
        }
    }
}
void render() {
    ImGui::NewFrame();
    s_sliderActive=false;
    auto& io=ImGui::GetIO();
    float sw=io.DisplaySize.x, sh=io.DisplaySize.y;
    if(sw<=0 || sh<=0) return;
    s=std::min(std::max(Global::uiScale,.85f),std::min((sw-24)/600.f,(sh-24)/400.f));
    s=std::max(s,.25f);
    if(s_screen.x>0 && (s_screen.x!=sw || s_screen.y!=sh)) {
        s_position.x*=sw/s_screen.x; s_position.y*=sh/s_screen.y;
        s_button.x*=sw/s_screen.x; s_button.y*=sh/s_screen.y;
    }
    s_screen=ImVec2(sw,sh);
    floating();
    Global::menuRect={};
    if(!s_visible) {
        if(!ImGui::IsMouseDown(0)) ConfigStore::saveIfChanged();
        return;
    }
    float w=std::min(960*s,sw-24*s), fullH=std::min(576*s,sh-24*s);
    float h=fullH;
    if(s_position.x<0) s_position=ImVec2((sw-w)/2,(sh-fullH)/2);
    s_position.x=std::clamp(s_position.x,0.f,sw-w);
    s_position.y=std::clamp(s_position.y,0.f,sh-h);
    ImGui::SetNextWindowPos(s_position); ImGui::SetNextWindowSize(ImVec2(w,h));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(0,0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,10*s);
    ImGui::Begin("THE FIRST FROST AOV",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoScrollWithMouse);
    auto* dl=ImGui::GetWindowDrawList(); ImVec2 p=ImGui::GetWindowPos();
    dl->AddRectFilled(p,ImVec2(p.x+w,p.y+56*s),SURFACE,10*s,ImDrawFlags_RoundCornersTop);
    trackedText(add(p,22,16),"THE FIRST FROST  AOV",20,.7f);
    hit("drag",p,ImVec2(w-104*s,56*s));
    if(ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {
        s_position.x+=io.MouseDelta.x; s_position.y+=io.MouseDelta.y;
    }
    for(int i=0;i<2;++i) {
        ImVec2 a(p.x+w-(86-i*34)*s,p.y+7*s);
        bool click=hit(i?"close":"minimize",a,ImVec2(34*s,42*s));
        ImVec2 center=add(a,17,21);
        dl->AddCircleFilled(center,14*s,i?IM_COL32(239,68,68,25):IM_COL32(245,158,11,25));
        dl->AddCircleFilled(center,(ImGui::IsItemHovered()?12:11)*s,i?RED:YELLOW);
        dl->AddCircleFilled(add(center,-3,-4),3*s,i?IM_COL32(252,165,165,150):IM_COL32(253,224,71,180));
        if(click) {
            s_visible=false;
            s_hidden=(i!=0); // Yellow leaves the launcher visible; red hides it.
        }
    }
    {
        float body=fullH-96*s, side=(w/s<=768?150.f:200.f)*s;
        dl->AddLine(add(p,0,56),ImVec2(p.x+w,p.y+56*s),BORDER,s);
        dl->AddRectFilled(add(p,0,56),ImVec2(p.x+side,p.y+56*s+body),IM_COL32(6,6,8,255));
        dl->AddLine(ImVec2(p.x+side,p.y+56*s),ImVec2(p.x+side,p.y+56*s+body),BORDER,s);
        const char* tabs[]={"Map","Aim & Auto","Skin","Settings","Status"};
        float tabH=(body-20*s-32*s)/5;
        bool changed=false;
        for(int i=0;i<5;++i) {
            ImVec2 a=add(p,8,66); a.y+=i*(tabH+8*s);
            bool clicked=hit(tabs[i],a,ImVec2(side-16*s,tabH));
            if(clicked) { changed=s_tab!=i; s_tab=i; }
            bool active=s_tab==i, hover=ImGui::IsItemHovered();
            ImVec2 b(a.x+side-16*s,a.y+tabH);
            if(active||hover) {
                dl->AddRectFilled(a,b,active?RAISED:SURFACE,6*s);
                dl->AddRect(a,b,active?BORDER_LIGHT:BORDER,6*s);
            }
            if(active) dl->AddRectFilled(a,ImVec2(a.x+3*s,b.y),GREEN,2*s);
            tabIcon(ImVec2(a.x+24*s,a.y+tabH/2),i,active?WHITE:DIM);
            text(ImVec2(a.x+46*s,a.y+tabH/2-9*s),tabs[i],14,active?WHITE:DIM,0,true);
        }
        cursor(ImVec2(p.x+side,p.y+56*s));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(24*s,22*s));
        ImGui::BeginChild("content",ImVec2(w-side,body),false,ImGuiWindowFlags_AlwaysUseWindowPadding);
        if(changed) ImGui::SetScrollY(0);
        if(s_tab==0) panelMap();
        else if(s_tab==1) panelAim();
        else if(s_tab==2) panelSkin();
        else if(s_tab==3) {
            heading("CẤU HÌNH TRÒ CHƠI");
            beginCard("fps");
            toggleRow("enable","Mở Khóa 120 FPS","Kích hoạt chế độ khung hình cao mượt mà",cfg.unlockFps120);
            endCard();
            beginCard("save-config");
            bool save=ConfigStore::enabled();
            if(toggleRow("enable","Lưu cấu hình","Tự lưu thay đổi và khôi phục khi mở game lần sau",save))
                ConfigStore::setEnabled(save);
            attached();
            ImVec2 note(s_innerX,ImGui::GetCursorScreenPos().y);
            const char* state=ConfigStore::status();
            text(note,state,12,ConfigStore::hasError()?RED:DIM,s_innerWidth);
            cursor(ImVec2(note.x,note.y+textHeight(state,12,s_innerWidth)+16*s));
            endCard();
        } else panelStatus();
        ImGui::Dummy(ImVec2(1,1));
        touchScroll();
        ImGui::EndChild(); ImGui::PopStyleVar();
        ImVec2 footer(p.x,p.y+fullH-40*s);
        dl->AddRectFilled(footer,ImVec2(p.x+w,p.y+fullH),SURFACE,10*s,ImDrawFlags_RoundCornersBottom);
        dl->AddLine(footer,ImVec2(p.x+w,footer.y),BORDER,s);
        char fps[32]; snprintf(fps,sizeof(fps),"%.0f",io.Framerate);
        text(ImVec2(p.x+w-102*s,footer.y+14*s),"FPS:",12,DIM);
        text(ImVec2(p.x+w-66*s,footer.y+14*s),fps,12,io.Framerate<59.5f?YELLOW:GREEN);
    }
    Global::menuRect={p.x,p.y,p.x+w,p.y+h};
    modal();
    ImGui::End(); ImGui::PopStyleVar(2);
    if(!ImGui::IsMouseDown(0)) ConfigStore::saveIfChanged();
    if(!s_visible) Global::menuRect={};
}
} // namespace Menu
