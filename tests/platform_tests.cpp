#include "platform.hpp"
#include "proxy.hpp"
#include "appearance.hpp"
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
using namespace mg;
#define CHECK(x) do{if(!(x)){std::cerr<<"Failed: " #x " line "<<__LINE__<<"\n";return 1;}}while(0)
int main(int argc,char** argv){
    if(argc>1&&std::string(argv[1])=="--child"){
        std::cout<<u8"Строка UTF-8\n"<<std::flush;std::cerr<<"stderr\n"<<std::flush;return 7;
    }
    if(argc>1&&std::string(argv[1])=="--wait"){std::cout<<"ready\n"<<std::flush;Sleep(30000);return 0;}
    std::atomic_bool cancel{false};TempDir temp(fs::temp_directory_path());auto p=temp.path/L"Тест ' путь";fs::create_directories(p);
    auto exe=programDir()/L"platform_tests.exe";
    std::vector<std::wstring> lines;auto log=[&](const auto& s){lines.push_back(s);};
    CHECK(runProcess({exe.wstring(),L"--child"},p,cancel,log,10)==7);
    CHECK(std::find(lines.begin(),lines.end(),L"Строка UTF-8")!=lines.end());
    CHECK(std::find(lines.begin(),lines.end(),L"stderr")!=lines.end());
    std::thread stopper([&]{std::this_thread::sleep_for(std::chrono::milliseconds(250));cancel=true;});
    bool stopped=false;auto started=std::chrono::steady_clock::now();
    try{runProcess({exe.wstring(),L"--wait"},p,cancel,log,10);}catch(const Cancelled&){stopped=true;}
    stopper.join();CHECK(stopped);CHECK(std::chrono::steady_clock::now()-started<std::chrono::seconds(5));cancel=false;
    auto j=p/L"settings.json";writeJson(j,{{"name",u8"Тест"}});CHECK(readJson(j)["name"]==u8"Тест");
    {std::ofstream o(p/L"abc");o<<"abc";}
    CHECK(sha256(p/L"abc")=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    fs::path removed;{TempDir nested(p);removed=nested.path;std::ofstream(nested.path/L"part")<<"unfinished";}CHECK(!fs::exists(removed));
    auto stage=p/L"stage",target=p/L"target";fs::create_directories(stage);fs::create_directories(target);
    std::ofstream(stage/L"new")<<"new";std::ofstream(target/L"old")<<"old";
    installDirectory(stage,target);CHECK(fs::exists(target/L"new"));CHECK(!fs::exists(target/L"old"));
    ProxySettings proxy;CHECK(!proxy.enabled);CHECK(proxyUrl(proxy).empty());
    proxy.enabled=true;proxy.type=1;proxy.host=L"::1";proxy.port=1080;proxy.user=L"user";proxy.password=L"p@ss: /%";
    CHECK(proxyUrl(proxy)==L"socks5h://user:p%40ss%3A%20%2F%25@[::1]:1080");
    auto redacted=redactProxy(L"proxy='"+proxyUrl(proxy)+L"' password="+proxy.password,proxy);
    CHECK(redacted.find(L"p@ss")==redacted.npos);CHECK(redacted.find(L"p%40ss")==redacted.npos);
    auto encrypted=protectSecret(proxy.password);CHECK(encrypted!=utf8(proxy.password));CHECK(unprotectSecret(encrypted)==proxy.password);
    saveProxy(p/L"proxy.json",proxy);CHECK(loadProxy(p/L"proxy.json").password==proxy.password);
    bool invalid=false;proxy.port=0;try{validateProxy(proxy);}catch(...){invalid=true;}CHECK(invalid);
    auto legacy=loadAppearance(p,2);CHECK(legacy.theme==2);CHECK(legacy.style==0);CHECK(legacy.language==1);
    for(int style=0;style<4;++style)for(int theme=1;theme<=2;++theme){
        Appearance a{style,theme,1};saveAppearance(p,a);auto b=loadAppearance(p);CHECK(b.style==style&&b.theme==theme&&b.language==1);
        auto colors=paletteFor(a);CHECK(colors.dark==(theme==2));CHECK(colors.success!=colors.error);CHECK(colors.fg!=colors.bg);
    }
    uiLanguage=1;CHECK(std::wstring(tr(L"Settings"))==L"Settings");CHECK(std::string(tr("Could not write settings"))=="Could not write settings");
    CHECK(friendlyError(L"connection timed out").find(L"timed out")!=std::wstring::npos);
    uiLanguage=0;CHECK(std::wstring(tr(L"Settings"))==L"Настройки");
    std::cout<<"Platform tests passed\n";
}

