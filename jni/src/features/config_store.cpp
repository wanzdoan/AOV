#include "include/config_store.hpp"
#include "include/aov_config.hpp"
#include <array>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <locale>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <direct.h>
#include <io.h>
#else
#include <unistd.h>
#endif

namespace ConfigStore {
namespace {
struct Field { const char* name; double min, max; bool integer; };
constexpr Field fields[] = {
    {"map",0,1,true}, {"cooldown",0,1,true}, {"camera_enabled",0,1,true},
    {"camera",0,30,false}, {"fps120",0,1,true}, {"aim",0,1,true},
    {"c1",0,1,true}, {"c2",0,1,true}, {"c3",0,1,true},
    {"target",0,999,true}, {"range",0,25,false}, {"lead",0,5,false},
    {"elsu",0,1,true}, {"execute",0,1,true}, {"punish",0,1,true},
    {"boss",0,1,true}, {"buff",0,1,true}, {"skins",0,1,true},
    {"buttons",0,73,true}, {"notify",0,61,true}
};
using Values=std::array<double,sizeof(fields)/sizeof(fields[0])>;
std::string filePath, lastDocument;
Values lastValues{};
bool saving=false, error=false;
const char* message="Chưa khởi tạo bộ nhớ";
std::time_t savedAt=0;
std::time_t retryAt=0;

Values capture() {
    return {{double(cfg.hackMapV2),double(cfg.timeHoiChieu),double(cfg.rangeEnable),
        double(std::round(cfg.rangeValue/.036186f)),double(cfg.unlockFps120),double(AimSkill),
        double(AimSkill1),double(AimSkill2),double(AimSkill3),double(aimType),
        double(PviChieu),double(KCachDon),double(EspElsu),double(Ksbp),double(Kstt),
        double(g_TaThanRong),double(g_BuaXanhDo),double(cfg.unlockAllSkins),
        double(cfg.buttonSkinIdx),double(cfg.killNotifyIdx)}};
}
bool valid(const Values& v) {
    for(size_t i=0;i<v.size();++i) {
        if(!std::isfinite(v[i]) || v[i]<fields[i].min || v[i]>fields[i].max ||
            (fields[i].integer && std::floor(v[i])!=v[i])) return false;
    }
    return v[9]==AIM_OFF || (v[9]>=AIM_LOW_HP_PCT && v[9]<=AIM_NEAREST_CROSS);
}
void apply(const Values& v) {
    cfg.hackMapV2=v[0]; cfg.timeHoiChieu=v[1]; cfg.rangeEnable=v[2];
    cfg.rangeValue=float(v[3])*.036186f; cfg.unlockFps120=v[4];
    cfg.aimEnable=AimSkill=v[5]; cfg.aimC1=AimSkill1=v[6];
    cfg.aimC2=AimSkill2=v[7]; cfg.aimUlti=AimSkill3=v[8];
    cfg.aimType=aimType=int(v[9]);
    cfg.range1=cfg.range2=cfg.range3=Rangeskill1=Rangeskill2=Rangeskill3=PviChieu=float(v[10]);
    cfg.kCachDon=KCachDon=float(v[11]); cfg.drawAimRay=EspElsu=v[12];
    Ksbp=v[13]; Kstt=v[14]; g_TaThanRong=v[15]; g_BuaXanhDo=v[16];
    cfg.unlockAllSkins=v[17]; cfg.buttonSkinIdx=int(v[18]); cfg.killNotifyIdx=int(v[19]);
    cfg.buttonSkinId=kButtonSkinTable[cfg.buttonSkinIdx];
    cfg.killNotifySkinId=kKillNotifyTable[cfg.killNotifyIdx];
}
std::string serialize(bool enabled,const Values& v) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out<<"FIRST_FROST 1\nsave_enabled "<<int(enabled)<<'\n'<<std::setprecision(9);
    for(size_t i=0;i<v.size();++i) out<<fields[i].name<<' '<<v[i]<<'\n';
    return out.str();
}
bool parse(const std::string& document,bool& enabled,Values& values) {
    std::istringstream in(document);
    in.imbue(std::locale::classic());
    std::string magic,key;
    int version,on;
    if(!(in>>magic>>version) || magic!="FIRST_FROST" || version!=1 ||
       !(in>>key>>on) || key!="save_enabled" || (on!=0 && on!=1)) return false;
    std::array<bool,sizeof(fields)/sizeof(fields[0])> seen{};
    size_t count=0;
    while(in>>key) {
        size_t i=0;
        for(;i<values.size();++i) if(key==fields[i].name) break;
        if(i==values.size() || seen[i] || !(in>>values[i])) return false;
        seen[i]=true; ++count;
    }
    enabled=on!=0;
    return count==values.size() && valid(values);
}
bool fail(const char* reason) {
    error=true; message=reason; retryAt=std::time(nullptr)+5;
    return false;
}
bool writeDocument(const std::string& document) {
    if(filePath.empty()) return fail("Không có thư mục riêng của game");
    const std::string tmp=filePath+".tmp";
#ifdef _WIN32
    int fd=_open(tmp.c_str(),_O_WRONLY|_O_CREAT|_O_TRUNC|_O_BINARY,_S_IREAD|_S_IWRITE);
    FILE* file=fd>=0?_fdopen(fd,"wb"):nullptr;
#else
    int fd=open(tmp.c_str(),O_WRONLY|O_CREAT|O_TRUNC|O_CLOEXEC|O_NOFOLLOW,0600);
    FILE* file=fd>=0?fdopen(fd,"wb"):nullptr;
#endif
    if(!file) {
        if(fd>=0) {
#ifdef _WIN32
            _close(fd);
#else
            close(fd);
#endif
        }
        return fail("Không mở được tệp cấu hình để ghi");
    }
    bool ok=fwrite(document.data(),1,document.size(),file)==document.size() && fflush(file)==0;
#ifdef _WIN32
    if(ok) ok=_commit(fd)==0;
#else
    if(ok) ok=fsync(fd)==0;
#endif
    if(fclose(file)!=0) ok=false;
    if(ok) {
#ifdef _WIN32
        ok=MoveFileExA(tmp.c_str(),filePath.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
#else
        ok=rename(tmp.c_str(),filePath.c_str())==0;
#endif
    }
    if(!ok) { std::remove(tmp.c_str()); return fail("Lưu thất bại · bản cũ được giữ lại"); }
    lastDocument=document; savedAt=std::time(nullptr);
    retryAt=0; error=false;
    return true;
}
}

void initialize(const char* appDataDirectory) {
    filePath.clear(); lastDocument.clear(); saving=false; error=false; savedAt=0; retryAt=0;
    message="Chưa bật lưu cấu hình";
    if(!appDataDirectory || !*appDataDirectory) { fail("Không có thư mục riêng của game"); return; }
    std::string directory=std::string(appDataDirectory)+"/files";
#ifdef _WIN32
    int result=_mkdir(directory.c_str());
#else
    int result=mkdir(directory.c_str(),0700);
#endif
    if(result!=0 && errno!=EEXIST) { fail("Không truy cập được bộ nhớ cấu hình"); return; }
    filePath=directory+"/first_frost.cfg";
    FILE* file=fopen(filePath.c_str(),"rb");
    if(!file) {
        if(errno!=ENOENT) fail("Không đọc được cấu hình đã lưu");
        return;
    }
    char data[8193];
    size_t size=fread(data,1,sizeof(data),file);
    bool readError=ferror(file)!=0;
    fclose(file);
    bool enabled=false; Values values{};
    if(readError || size==sizeof(data) || !parse(std::string(data,size),enabled,values)) {
        fail("Bản lưu lỗi hoặc khác phiên bản · chưa áp dụng"); return;
    }
    if(!enabled) { message="Đã tắt khôi phục cấu hình"; return; }
    apply(values); saving=true;
    lastValues=capture();
    lastDocument=serialize(true,capture());
    struct stat info{};
    if(stat(filePath.c_str(),&info)==0) savedAt=info.st_mtime;
    message="Đã khôi phục cấu hình";
}
bool enabled() { return saving; }
bool setEnabled(bool enabled) {
    Values values=capture();
    if(!valid(values)) return fail("Giá trị cấu hình không hợp lệ");
    if(!writeDocument(serialize(enabled,values))) return false;
    saving=enabled;
    lastValues=values;
    message=enabled?"Đã lưu · tự cập nhật khi thay đổi":"Đã tắt khôi phục cấu hình";
    return true;
}
void saveIfChanged() {
    if(!saving || std::time(nullptr)<retryAt) return;
    Values values=capture();
    if(values==lastValues) return;
    if(!valid(values)) { fail("Giá trị cấu hình không hợp lệ"); return; }
    auto document=serialize(true,values);
    if(document==lastDocument) return;
    if(writeDocument(document)) {
        lastValues=values;
        message="Đã lưu · tự cập nhật khi thay đổi";
    }
}
const char* status() { return message; }
bool hasError() { return error; }
std::time_t lastSaved() { return savedAt; }
}
