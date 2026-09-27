#include "include/config_store.hpp"
#include "include/aov_config.hpp"
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
namespace fs=std::filesystem;
static std::string read(const fs::path& p) {
    std::ifstream in(p,std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in),{});
}
static void write(const fs::path& p,const std::string& value) {
    std::ofstream out(p,std::ios::binary|std::ios::trunc); out<<value; assert(out.good());
}
static void populate() {
    cfg.hackMapV2=true; cfg.timeHoiChieu=true; cfg.rangeEnable=true;
    cfg.rangeValue=30*.036186f; cfg.unlockFps120=true;
    AimSkill=true; AimSkill1=true; AimSkill2=false; AimSkill3=true;
    aimType=AIM_NEAREST_CROSS; PviChieu=23; KCachDon=2.7f;
    EspElsu=true; Ksbp=true; Kstt=true; g_TaThanRong=true; g_BuaXanhDo=false;
    cfg.unlockAllSkins=true; cfg.buttonSkinIdx=73; cfg.killNotifyIdx=61;
}
static void verify() {
    assert(ConfigStore::enabled() && !ConfigStore::hasError());
    assert(cfg.hackMapV2 && cfg.timeHoiChieu && cfg.rangeEnable && cfg.unlockFps120);
    assert(std::fabs(cfg.rangeValue-30*.036186f)<.00001f);
    assert(AimSkill && cfg.aimEnable && AimSkill1 && cfg.aimC1);
    assert(!AimSkill2 && !cfg.aimC2 && AimSkill3 && cfg.aimUlti);
    assert(aimType==AIM_NEAREST_CROSS && cfg.aimType==aimType);
    assert(PviChieu==23 && Rangeskill1==23 && Rangeskill2==23 && Rangeskill3==23);
    assert(cfg.range1==23 && cfg.range2==23 && cfg.range3==23);
    assert(std::fabs(KCachDon-2.7f)<.0001f && cfg.kCachDon==KCachDon);
    assert(EspElsu && cfg.drawAimRay && Ksbp && Kstt && g_TaThanRong && !g_BuaXanhDo);
    assert(cfg.unlockAllSkins && cfg.buttonSkinIdx==73 && cfg.killNotifyIdx==61);
    assert(cfg.buttonSkinId==kButtonSkinTable[73] && cfg.killNotifySkinId==kKillNotifyTable[61]);
}
int main(int argc,char** argv) {
    assert(argc==3); std::string mode=argv[1]; fs::path root=argv[2];
    fs::create_directories(root);
    auto file=root/"files"/"first_frost.cfg";
    ConfigStore::initialize(root.string().c_str());
    if(mode=="write") {
        assert(!ConfigStore::enabled() && !ConfigStore::hasError());
        populate(); assert(ConfigStore::setEnabled(true));
        assert(fs::exists(file) && ConfigStore::lastSaved()>0);
        // Changes after enabling are captured without revisiting Settings.
        KCachDon=3.5f; ConfigStore::saveIfChanged();
        assert(read(file).find("lead 3.5")!=std::string::npos);
        KCachDon=2.7f; ConfigStore::saveIfChanged();
        const auto old=fs::file_time_type::clock::now()-std::chrono::hours(1);
        fs::last_write_time(file,old);
        const auto actual=fs::last_write_time(file);
        ConfigStore::saveIfChanged();
        assert(fs::last_write_time(file)==actual); // No per-frame idle write.
    } else if(mode=="read") {
        verify(); // Fresh process, all globals started at their default values.
    } else if(mode=="disable") {
        verify(); assert(ConfigStore::setEnabled(false));
        assert(!ConfigStore::enabled() && cfg.hackMapV2); // Current session unchanged.
    } else if(mode=="disabled-read") {
        assert(!ConfigStore::enabled() && !ConfigStore::hasError());
        assert(!cfg.hackMapV2 && !AimSkill && cfg.buttonSkinIdx==0);
    } else if(mode=="failures") {
        populate(); assert(ConfigStore::setEnabled(true));
        const std::string good=read(file);
        fs::create_directory(file.string()+".tmp"); // Force a real atomic-write failure.
        assert(!ConfigStore::setEnabled(false));
        assert(ConfigStore::enabled() && ConfigStore::hasError());
        assert(read(file)==good);
        fs::remove(file.string()+".tmp");
        ConfigStore::initialize(root.string().c_str()); verify();
        const std::string bad[]={
            "FIRST_FROST 1\nsave_enabled 1\nmap 1\n",
            "FIRST_FROST 99\n"+good.substr(good.find('\n')+1),
            good+"map 0\n", good+"unknown 3\n",
            good.substr(0,good.find("buttons"))+"buttons 999\nnotify 61\n",
            good.substr(0,good.find("lead"))+"lead nan\n",
            good.substr(0,good.find("range "))+"range -1\n",
            std::string(9000,'x')
        };
        for(const auto& document:bad) {
            write(file,document);
            cfg.hackMapV2=false; AimSkill=false;
            ConfigStore::initialize(root.string().c_str());
            assert(!ConfigStore::enabled() && ConfigStore::hasError());
            assert(!cfg.hackMapV2 && !AimSkill); // Reject whole file, no partial application.
            assert(read(file)==document); // Do not silently replace a corrupt file.
        }
        write(file,good); ConfigStore::initialize(root.string().c_str()); verify();
        ConfigStore::initialize(""); assert(ConfigStore::hasError());
        assert(!ConfigStore::setEnabled(true));
    } else assert(false);
    std::cout<<"PASS config persistence: "<<mode<<"\n";
}
