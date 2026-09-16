#pragma once
#include "platform.hpp"
namespace mg {
struct Component {
    std::string id;
    std::wstring title,exe;
};
const std::vector<Component>& components();
class ComponentManager {
    fs::path root_;
    bool x64_;
public:
    explicit ComponentManager(const fs::path& app,bool use64=system64()):root_(app/L"components"/(use64?L"x64":L"x86")),x64_(use64){}
    fs::path directory(const std::string& id)const{return root_/id;}
    fs::path executable(const std::string& id)const;
    bool supported(const std::string& id)const{return id=="ytdlp"||id=="ffmpeg"||id=="node";}
    bool installed(const std::string& id)const;
    std::wstring version(const std::string& id)const;
    void install(const std::string& id,bool force,std::atomic_bool& cancel,const Log& log,const Progress& progress);
    void prepare(std::atomic_bool& cancel,const Log& log,const Progress& progress);
};

std::wstring browserId(int index);
}

