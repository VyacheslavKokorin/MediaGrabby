#include "download.hpp"
#include "proxy.hpp"
#include <iostream>
using namespace mg;
#define CHECK(x) do{if(!(x)){std::cerr<<"Failed: " #x " line "<<__LINE__<<"\n";return 1;}}while(0)
int main(int argc,char** argv){
    if(argc<4)return 2;
    std::atomic_bool cancel{false};auto log=[](const auto& s){std::cout<<utf8(s)<<"\n"<<std::flush;};
    auto progress=[&](const auto& s,int){log(s);};
    try{
        auto fixtures=fs::absolute(fs::path(argv[1]));fs::create_directories(fixtures);
        bool x64=std::string(argv[3])=="x64";
        ComponentManager cm(programDir()/L"integration-tools",x64);
        bool proxied=argc>4;ProxySettings proxy;proxy.enabled=proxied;proxy.type=x64?0:1;proxy.host=L"127.0.0.1";proxy.port=x64?18882:18883;proxy.user=L"test-user";proxy.password=L"test @/:% password";
        setProxy(proxy);
        cm.prepare(cancel,log,progress);
        CHECK(cm.installed("ytdlp"));CHECK(cm.installed("ffmpeg"));CHECK(cm.installed("node"));
        CHECK(components().size()==3);
        auto video=fixtures/L"sample.mp4";
        CHECK(runProcess({cm.executable("ffmpeg").wstring(),L"-hide_banner",L"-nostdin",L"-y",L"-f",L"lavfi",L"-i",L"testsrc=size=128x72:rate=5",
            L"-f",L"lavfi",L"-i",L"sine=frequency=440:sample_rate=44100",L"-t",L"2",L"-c:v",L"libx264",L"-pix_fmt",L"yuv420p",L"-c:a",L"aac",video.wstring()},fixtures,cancel,log,60)==0);
        TempDir out(fixtures);Options o;o.url=(proxied?L"http://mediagrabby.test:":L"http://127.0.0.1:")+std::wstring()+wide(argv[2])+L"/sample.mp4";o.folder=(out.path/L"Видео ' & папка").wstring();
        auto onFile=[](const auto&){};
        auto r=download(o,cm,cancel,log,progress,onFile);CHECK(r.completed==1);CHECK(fs::exists(r.lastFile));CHECK(r.lastFile.extension()==L".mp4");
        auto saved=r.lastFile;auto hash=sha256(saved);
        r=download(o,cm,cancel,log,progress,onFile);CHECK(r.skipped==1);CHECK(r.completed==0);CHECK(sha256(saved)==hash);
        o.audioOnly=true;r=download(o,cm,cancel,log,progress,onFile);CHECK(r.completed==1);CHECK(r.lastFile.extension()==L".mp3");
        o.audioOnly=false;o.videoCodec=1;o.audioCodec=1;r=download(o,cm,cancel,log,progress,onFile);CHECK(r.completed==1);
        o.videoCodec=2;r=download(o,cm,cancel,log,progress,onFile);CHECK(r.completed==1);
        CHECK(sha256(saved)==hash);
        if(proxied){
            for(int type:{0,1}){
                proxy.type=type;proxy.port=type?18883:18882;setProxy(proxy);
                CHECK(httpText(L"https://nodejs.org/dist/latest-v22.x/SHASUMS256.txt",cancel).size()>100);
                o.audioOnly=false;o.videoCodec=0;o.audioCodec=0;o.folder=(out.path/(type?L"socks":L"http")).wstring();
                r=download(o,cm,cancel,log,progress,onFile);CHECK(r.completed==1);
                auto wrong=proxy;wrong.password=L"wrong";bool rejected=false;
                try{httpTextUsing(L"https://nodejs.org/",cancel,wrong);}catch(const std::exception&){rejected=true;}CHECK(rejected);
            }
        }
        setProxy({});
        std::cout<<"Media integration tests passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
}

