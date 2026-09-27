// Desktop verification of the production ImGui view with platform services stubbed.
// run-menu-preview.ps1 generates menu_under_test.cpp by changing includes only.
#define NOMINMAX
#include <windows.h>
#include <GL/gl.h>
#include "imgui.h"
#include "imgui_internal.h"
#include <cassert>
#include <cstdio>
#include <vector>
#include <string>
#include <filesystem>
namespace Global {
float uiScale=1;
int screenWidth=1200,screenHeight=720;
struct UIRect { float x0=0,y0=0,x1=0,y1=0; };
UIRect menuRect,wmRect;
}
namespace Esp { bool isTouchReady() { return true; } }
namespace il2cppExports {
void* il2cpp_class_from_name=reinterpret_cast<void*>(1);
void* il2cpp_domain_get=reinterpret_cast<void*>(1);
}
#include "menu_under_test.cpp"

static GLuint atlas;
static void draw() {
    int w=Global::screenWidth,h=Global::screenHeight;
    glViewport(0,0,w,h);
    glClearColor(.025f,.025f,.035f,1); glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE);
    glEnable(GL_SCISSOR_TEST); glEnable(GL_TEXTURE_2D);
    glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrtho(0,w,h,0,-1,1);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    auto* data=ImGui::GetDrawData();
    for(int n=0;n<data->CmdListsCount;++n) {
        auto* list=data->CmdLists[n];
        for(auto& cmd:list->CmdBuffer) {
            if(cmd.UserCallback) continue;
            glBindTexture(GL_TEXTURE_2D,(GLuint)(uintptr_t)cmd.GetTexID());
            glScissor((int)cmd.ClipRect.x,h-(int)cmd.ClipRect.w,
                (int)(cmd.ClipRect.z-cmd.ClipRect.x),(int)(cmd.ClipRect.w-cmd.ClipRect.y));
            glBegin(GL_TRIANGLES);
            for(unsigned int k=0;k<cmd.ElemCount;++k) {
                auto& v=list->VtxBuffer[list->IdxBuffer[cmd.IdxOffset+k]+cmd.VtxOffset];
                glColor4ub((v.col>>IM_COL32_R_SHIFT)&255,(v.col>>IM_COL32_G_SHIFT)&255,
                    (v.col>>IM_COL32_B_SHIFT)&255,(v.col>>IM_COL32_A_SHIFT)&255);
                glTexCoord2f(v.uv.x,v.uv.y); glVertex2f(v.pos.x,v.pos.y);
            }
            glEnd();
        }
    }
    glDisable(GL_SCISSOR_TEST); glFinish();
}
static void frame() {
    ImGui::GetIO().DisplaySize=ImVec2((float)Global::screenWidth,(float)Global::screenHeight);
    ImGui::GetIO().DeltaTime=1.f/60;
    Menu::render(); ImGui::Render();
}
static void settle() { for(int i=0;i<4;++i) frame(); }
static void snapshot(const char* name) {
    settle(); draw();
    int w=Global::screenWidth,h=Global::screenHeight;
    std::vector<unsigned char> rgba(w*h*4);
    glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());
    for(size_t i=0;i<rgba.size();i+=4) std::swap(rgba[i],rgba[i+2]);
    BITMAPFILEHEADER file={}; BITMAPINFOHEADER info={};
    file.bfType=0x4d42; file.bfOffBits=sizeof(file)+sizeof(info); file.bfSize=file.bfOffBits+(DWORD)rgba.size();
    info.biSize=sizeof(info); info.biWidth=w; info.biHeight=h; info.biPlanes=1; info.biBitCount=32; info.biSizeImage=(DWORD)rgba.size();
    std::string path=std::string(name)+".bmp";
    FILE* f=fopen(path.c_str(),"wb"); assert(f);
    fwrite(&file,sizeof(file),1,f); fwrite(&info,sizeof(info),1,f); fwrite(rgba.data(),rgba.size(),1,f); fclose(f);
}
static void click(float x,float y) {
    auto& io=ImGui::GetIO();
    io.AddMousePosEvent(x,y); frame();
    io.AddMouseButtonEvent(0,true); frame();
    io.AddMouseButtonEvent(0,false); frame(); settle();
}
static void drag(float x,float y,float dx,float dy) {
    auto& io=ImGui::GetIO();
    io.AddMousePosEvent(x,y); frame(); io.AddMouseButtonEvent(0,true); frame();
    for(int i=1;i<=10;++i) { io.AddMousePosEvent(x+dx*i/10,y+dy*i/10); frame(); }
    io.AddMouseButtonEvent(0,false); frame(); settle();
}
static ImGuiWindow* content() {
    for(auto* w:GImGui->Windows)
        if(strstr(w->Name,"/content_")) return w;
    assert(false); return nullptr;
}
static ImGuiWindow* options() {
    for(auto* w:GImGui->Windows)
        if(strstr(w->Name,"/options_") && w->Active) return w;
    assert(false); return nullptr;
}
static void tab(int i) {
    // Actual sidebar clicks at the reference 960 x 576 size.
    click(Menu::s_position.x+85,Menu::s_position.y+66+i*(85.6f+8)+42);
    assert(Menu::s_tab==i);
}
int main() {
    std::string configDirectory="preview-state-"+std::to_string(GetTickCount64());
    std::filesystem::create_directory(configDirectory);
    ConfigStore::initialize(configDirectory.c_str());
    WNDCLASSA wc={}; wc.lpfnWndProc=DefWindowProcA; wc.hInstance=GetModuleHandle(nullptr); wc.lpszClassName="FrostPreview";
    RegisterClassA(&wc);
    HWND window=CreateWindowA(wc.lpszClassName,"Offscreen ImGui verification",WS_POPUP,0,0,2048,2048,nullptr,nullptr,wc.hInstance,nullptr);
    HDC dc=GetDC(window);
    PIXELFORMATDESCRIPTOR pf={}; pf.nSize=sizeof(pf); pf.nVersion=1;
    pf.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL; pf.iPixelType=PFD_TYPE_RGBA; pf.cColorBits=32;
    int format=ChoosePixelFormat(dc,&pf); assert(format); assert(SetPixelFormat(dc,format,&pf));
    HGLRC gl=wglCreateContext(dc); assert(gl && wglMakeCurrent(dc,gl));
    ImGui::CreateContext(); auto& io=ImGui::GetIO(); io.IniFilename=nullptr;
    auto* regular=io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/segoeui.ttf",24,nullptr,io.Fonts->GetGlyphRangesVietnamese());
    auto* bold=io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/segoeuib.ttf",24,nullptr,io.Fonts->GetGlyphRangesVietnamese());
    Menu::setFonts(regular,bold);
    unsigned char* pixels; int aw,ah; io.Fonts->GetTexDataAsRGBA32(&pixels,&aw,&ah);
    glGenTextures(1,&atlas); glBindTexture(GL_TEXTURE_2D,atlas);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,aw,ah,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
    io.Fonts->SetTexID((ImTextureID)(uintptr_t)atlas);
    Menu::applyStyle(); Menu::toggle(); settle();
    snapshot("01-map");
    float x=Menu::s_position.x,y=Menu::s_position.y;
    click(x+840,y+121); assert(cfg.hackMapV2);
    click(x+840,y+210); assert(cfg.timeHoiChieu);
    click(x+840,y+290); assert(cfg.rangeEnable);
    // Camera is zero by default, both endpoints must be reachable.
    drag(x+265,y+380,645,0); assert(cfg.rangeValue>1.08f);
    snapshot("02-map-expanded");
    tab(1);
    click(x+840,y+121); assert(AimSkill && cfg.aimEnable);
    snapshot("03-aim");
    click(x+278,y+237); assert(AimSkill1 && cfg.aimC1);
    click(x+500,y+237); assert(AimSkill2 && cfg.aimC2);
    click(x+730,y+237); assert(AimSkill3 && cfg.aimUlti);
    click(x+820,y+331); assert(Menu::s_popup==1);
    snapshot("04-target-modal");
    auto* box=ImGui::FindWindowByName("##frost-choice"); assert(box);
    click(box->Pos.x+100,box->Pos.y+72+50+20);
    assert(aimType==AIM_LOW_HP_PCT && cfg.aimType==aimType);
    drag(x+265,y+407,645,0);
    assert(PviChieu==25 && cfg.range1==25 && cfg.range2==25 && cfg.range3==25);
    assert(Rangeskill1==25 && Rangeskill2==25 && Rangeskill3==25);
    drag(x+265,y+471,325,0);
    assert(KCachDon>2 && KCachDon<3 && cfg.kCachDon==KCachDon);
    assert(content()->Scroll.y==0);
    click(x+840,y+520); assert(EspElsu && cfg.drawAimRay);
    // Scroll a row vertically: it must not toggle on release.
    bool ray=EspElsu;
    drag(x+500,y+520,0,-220);
    assert(content()->Scroll.y>0 && EspElsu==ray);
    snapshot("05-aim-scrolled");
    click(x+840,y+390); assert(Ksbp);
    click(x+840,y+465); assert(Kstt);
    auto* scroller=content();
    ImGui::SetScrollY(scroller,scroller->ScrollMax.y); settle();
    snapshot("05b-punish-expanded");
    click(x+840,y+425); assert(g_TaThanRong);
    click(x+840,y+477); assert(g_BuaXanhDo);
    tab(0); tab(1); assert(content()->Scroll.y==0);
    click(x+840,y+121); assert(!AimSkill && !cfg.aimEnable);
    tab(2);
    click(x+840,y+121); assert(cfg.unlockAllSkins);
    snapshot("06-skin");
    click(x+800,y+222); assert(Menu::s_popup==2);
    snapshot("07-button-modal");
    float backgroundScroll=content()->Scroll.y;
    auto* listBox=options();
    drag(listBox->Pos.x+100,listBox->Pos.y+250,0,-160);
    assert(options()->Scroll.y>0 && cfg.buttonSkinIdx==0);
    assert(content()->Scroll.y==backgroundScroll);
    assert(Global::menuRect.x0==0 && Global::menuRect.x1==Global::screenWidth);
    click(10,600); assert(Menu::s_popup==0 && Menu::s_tab==2);
    click(x+800,y+222); assert(Menu::s_popup==2);
    io.AddKeyEvent(ImGuiKey_Escape,true); frame();
    io.AddKeyEvent(ImGuiKey_Escape,false); settle();
    assert(Menu::s_popup==0);
    click(x+800,y+222); assert(Menu::s_popup==2);
    auto* opts=options();
    ImGui::SetScrollY(opts,opts->ScrollMax.y); settle();
    // Choose the last actual option, checking its non-contiguous game ID.
    float optionY=opts->Pos.y+73*50.f-opts->Scroll.y+22;
    click(opts->Pos.x+100,optionY);
    assert(cfg.buttonSkinIdx==73 && cfg.buttonSkinId==kButtonSkinTable[73]);
    settle(); assert(cfg.buttonSkinIdx==73);
    click(x+800,y+301); assert(Menu::s_popup==3);
    opts=options(); ImGui::SetScrollY(opts,opts->ScrollMax.y); settle();
    click(opts->Pos.x+100,opts->Pos.y+61*50.f-opts->Scroll.y+22);
    assert(cfg.killNotifyIdx==61 && cfg.killNotifySkinId==kKillNotifyTable[61]);
    tab(3); click(x+840,y+121); assert(cfg.unlockFps120);
    click(x+840,y+210); assert(ConfigStore::enabled() && !ConfigStore::hasError());
    snapshot("08-settings");
    tab(4); snapshot("09-status");
    ImGui::SetScrollY(content(),content()->ScrollMax.y); settle(); snapshot("09b-status-details");
    click(x+891,y+28); assert(!Menu::isVisible() && !Menu::s_hidden);
    assert(Global::menuRect.x1==0); snapshot("10-hidden-launcher");
    auto* mainWindow=ImGui::FindWindowByName("THE FIRST FROST AOV");
    assert(mainWindow && !mainWindow->Active);
    assert(Global::wmRect.x1-Global::wmRect.x0>=84);
    drag(Menu::s_button.x,Menu::s_button.y,50,80);
    assert(!Menu::isVisible());
    snapshot("10b-launcher-dragged");
    click(Menu::s_button.x,Menu::s_button.y); assert(Menu::isVisible());
    click(x+925,y+28); assert(!Menu::isVisible());
    click(Menu::s_button.x,Menu::s_button.y); assert(Menu::isVisible());
    Global::screenWidth=720; Global::screenHeight=1280; settle(); snapshot("11-portrait");
    Global::screenWidth=800; Global::screenHeight=480; settle(); snapshot("12-small");
    assert(Global::menuRect.x0>=0 && Global::menuRect.y0>=0);
    assert(Global::menuRect.x1<=800 && Global::menuRect.y1<=480);
    Global::screenWidth=1920; Global::screenHeight=1080; Global::uiScale=1.5f;
    io.FontGlobalScale=1.5f; Menu::applyStyle(); ImGui::GetStyle().ScaleAllSizes(1.5f);
    settle(); snapshot("13-high-dpi");
    assert(Global::menuRect.x1<=1920 && Global::menuRect.y1<=1080);
    puts("PASS: tabs, controls, save-config toggle, expanded status, full yellow hide, larger draggable launcher, red hide/reopen, modals, rotation.");
    ImGui::DestroyContext(); wglMakeCurrent(nullptr,nullptr); wglDeleteContext(gl); ReleaseDC(window,dc); DestroyWindow(window);
}
