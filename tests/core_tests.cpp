#include "core.hpp"
#include <iostream>
#define CHECK(x) do{if(!(x)){std::cerr<<"Failed: " #x " line "<<__LINE__<<"\n";return 1;}}while(0)
int main(){
    CHECK(mg::quote(L"")==L"\"\"");
    CHECK(mg::quote(L"D:\\Мои видео\\")==L"\"D:\\Мои видео\\\\\"");
    CHECK(mg::quote(L"a\"b")==L"\"a\\\"b\"");
    CHECK(mg::commandLine({L"app.exe",L"a&b",L"\"x\""})==L"\"app.exe\" \"a&b\" \"\\\"x\\\"\"");
    CHECK(!mg::validUrl(L"--exec=calc.exe"));
    CHECK(!mg::validUrl(L"https://site.org/a\n--exec calc"));
    CHECK(mg::validUrl(L"https://site.org/a?x=1&y=2"));
    CHECK(mg::youtubeVideo(L"https://www.youtube.com/watch?v=abc&list=xyz"));
    CHECK(!mg::youtubeVideo(L"https://www.youtube.com/playlist?list=xyz"));
    CHECK(!mg::youtubeVideo(L"https://youtube.com.evil.org/watch?v=abc"));
    CHECK(mg::formatSelector(2,false)==L"bestvideo*[height<=?720]+bestaudio/best[height<=?720]");
    CHECK(mg::formatSelector(0,true)==L"bestaudio/best");
    mg::Options o;CHECK(!o.needsConversion());o.videoCodec=2;CHECK(o.needsConversion());
    auto a=mg::conversionArgs(o);CHECK(std::find(a.begin(),a.end(),L"libx265")!=a.end());CHECK(std::find(a.begin(),a.end(),L"aac")==a.end());
    o.audioOnly=true;CHECK(!o.needsConversion());
    CHECK(mg::safeName(L"../CON: foo.").find(L'/')==std::wstring::npos);
    std::cout<<"Core tests passed\n";
}

