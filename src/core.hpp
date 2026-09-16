#pragma once
#include <algorithm>
#include <string>
#include <vector>
namespace mg {
inline std::wstring quote(const std::wstring& value) {
    std::wstring out=L"\""; size_t slashes=0;
    for(wchar_t c:value) {
        if(c==L'\\'){++slashes;continue;}
        out.append(c==L'"'?slashes*2+1:slashes,L'\\');
        slashes=0;out+=c;
    }
    out.append(slashes*2,L'\\');return out+L'"';
}
inline std::wstring commandLine(const std::vector<std::wstring>& args) {
    std::wstring r;
    for(const auto& a:args){if(!r.empty())r+=L' ';r+=quote(a);}return r;
}
inline std::wstring trim(std::wstring v) {
    auto a=v.find_first_not_of(L" \r\n\t");if(a==v.npos)return {};
    return v.substr(a,v.find_last_not_of(L" \r\n\t")-a+1);
}
inline bool validUrl(const std::wstring& v) {
    return (v.rfind(L"https://",0)==0||v.rfind(L"http://",0)==0)
        &&v.find_first_of(L" \r\n\t")==v.npos&&v.find(L'.')!=v.npos;
}
inline bool youtubeVideo(const std::wstring& url) {
    auto start=url.find(L"://");if(start==url.npos)return false;
    auto end=url.find(L'/',start+3);auto host=url.substr(start+3,end-start-3);
    std::transform(host.begin(),host.end(),host.begin(),[](wchar_t c){return static_cast<wchar_t>(c>=L'A'&&c<=L'Z'?c+32:c);});
    return host==L"youtu.be"||((host==L"youtube.com"||host==L"www.youtube.com"||host==L"m.youtube.com"||host==L"music.youtube.com")
        &&(url.find(L"?v=")!=url.npos||url.find(L"&v=")!=url.npos||url.find(L"/shorts/")!=url.npos||url.find(L"/live/")!=url.npos));
}
inline std::wstring formatSelector(int quality,bool audio) {
    if(audio)return L"bestaudio/best";
    const int heights[]={0,1080,720,480,360};
    if(quality<0||quality>4)quality=0;
    auto f=quality?L"[height<=?"+std::to_wstring(heights[quality])+L"]":L"";
    return L"bestvideo*"+f+L"+bestaudio/best"+f;
}
inline std::wstring safeName(std::wstring s) {
    for(auto& c:s)if(c<32||std::wstring(L"<>:\"/\\|?*").find(c)!=std::wstring::npos)c=L'_';
    while(!s.empty()&&(s.back()==L'.'||s.back()==L' '))s.pop_back();
    if(s.empty())s=L"Playlist";if(s.size()>90)s.resize(90);
    return L"Playlist - "+s;
}
struct Options {
    std::wstring url,folder;
    int quality=0,browser=0,videoCodec=0,audioCodec=0,theme=0;
    bool audioOnly=false,metadata=true;
    bool needsConversion()const{return !audioOnly&&(videoCodec!=0||audioCodec!=0);}
};
inline std::vector<std::wstring> conversionArgs(const Options& o) {
    std::vector<std::wstring>a{L"-map",L"0:v:0",L"-map",L"0:a?",L"-map_metadata",L"0",L"-c:v:0"};
    a.push_back(o.videoCodec==1?L"libx264":o.videoCodec==2?L"libx265":L"copy");
    if(o.videoCodec) {
        a.insert(a.end(),{L"-preset",L"medium",L"-crf",o.videoCodec==1?L"18":L"20",L"-pix_fmt",L"yuv420p"});
        if(o.videoCodec==2)a.insert(a.end(),{L"-tag:v:0",L"hvc1"});
    }
    a.insert(a.end(),{L"-c:a",o.audioCodec?L"aac":L"copy"});
    if(o.audioCodec)a.insert(a.end(),{L"-b:a",L"192k"});
    return a;
}
}

