#include "i18n.hpp"
#include "components.hpp"
#include <sstream>
namespace mg {
const std::vector<Component>& components(){
    static const std::vector<Component> c={{"ytdlp",L"yt-dlp",L"yt-dlp.exe"},{"ffmpeg",L"FFmpeg",L"ffmpeg.exe"},
        {"node",L"Node.js",L"node.exe"}};return c;
}
std::wstring browserId(int i){const wchar_t* ids[]={L"",L"chrome",L"edge",L"firefox",L"brave",L"opera",L"vivaldi",L"chromium",L"whale"};return ids[i>=0&&i<9?i:0];}
fs::path ComponentManager::executable(const std::string& id)const{for(auto& c:components())if(c.id==id)return directory(id)/c.exe;throw std::runtime_error(tr("Unknown component"));}
bool ComponentManager::installed(const std::string& id)const{return fs::is_regular_file(executable(id))&&(id!="ffmpeg"||fs::is_regular_file(directory(id)/L"ffprobe.exe"));}
std::wstring ComponentManager::version(const std::string& id)const{
    if(!supported(id))return tr(L"no x86 build");
    if(!installed(id))return tr(L"not installed");
    auto j=readJson(directory(id)/L"version.json");
    return wide(j.value("version",std::string(tr("installed"))));
}
struct Package{std::wstring url;std::string digest,version;bool zip=false;};
static std::string checksum(const std::string& text,const std::string& name){
    std::istringstream in(text);std::string line;
    while(std::getline(in,line)){std::istringstream row(line);std::string hash,file;row>>hash>>file;
        if(!file.empty()&&file[0]=='*')file.erase(0,1);
        if(file==name&&hash.size()==64)return hash;
    }return {};
}
static Package github(const std::string& repo,const std::function<bool(const std::string&)>& select,std::atomic_bool& cancel,const std::string& tag=""){
    auto url=L"https://api.github.com/repos/"+wide(repo)+(tag.empty()?L"/releases/latest":L"/releases/tags/"+wide(tag));
    auto r=json::parse(httpText(url,cancel));
    for(const auto& a:r.at("assets")){
        std::string name=a.at("name");
        if(!select(name))continue;
        Package p;p.url=wide(a.at("browser_download_url").get<std::string>());p.version=r.at("tag_name");
        if(a.contains("digest")&&a["digest"].is_string()){auto h=a["digest"].get<std::string>();if(h.rfind("sha256:",0)==0)p.digest=h.substr(7);}
        if(p.digest.empty()){
            for(const auto& sums:r["assets"])if(sums["name"]=="SHA2-256SUMS"||sums["name"]=="checksums.sha256")
                p.digest=checksum(httpText(wide(sums["browser_download_url"]),cancel),name);
        }
        if(p.digest.size()!=64)throw std::runtime_error(tr("The source did not publish SHA256 for ")+name);
        p.zip=name.size()>4&&name.substr(name.size()-4)==".zip";return p;
    }throw std::runtime_error(tr("Release ")+repo+tr(" has no compatible build"));
}
void ComponentManager::install(const std::string& id,bool force,std::atomic_bool& cancel,const Log& log,const Progress& progress){
    if(!supported(id)){log(wide(id)+tr(L": no build for 32-bit Windows"));return;}
    if(installed(id)&&!force)return;
    progress(tr(L"Checking version of ")+wide(id)+L"…",-1);log(tr(L"Checking component ")+wide(id));
    Package p;
    if(id=="node"){
        auto sums=httpText(L"https://nodejs.org/dist/latest-v22.x/SHASUMS256.txt",cancel);
        std::istringstream in(sums);std::string hash,name,suffix=x64_?"-win-x64.zip":"-win-x86.zip";
        while(in>>hash>>name)if(name.size()>suffix.size()&&name.substr(name.size()-suffix.size())==suffix){
            auto end=name.find("-win-");p.version=name.substr(5,end-5);p.digest=hash;
            p.url=L"https://nodejs.org/dist/"+wide(p.version)+L"/"+wide(name);p.zip=true;break;
        }
        if(p.digest.size()!=64)throw std::runtime_error(tr("No compatible Node.js 22 build found"));
    }else if(id=="ytdlp")p=github("yt-dlp/yt-dlp",[&](auto n){return n==(x64_?"yt-dlp.exe":"yt-dlp_x86.exe");},cancel);
    else if(id=="ffmpeg")p=github(x64_?"GyanD/codexffmpeg":"defisym/FFmpeg-Builds-Win32",[&](auto n){
        return x64_?n.find("-essentials_build.zip")!=std::string::npos:n=="ffmpeg-master-latest-win32-gpl.zip";},cancel);
    else throw std::runtime_error(tr("Unknown component"));
    auto previous=readJson(directory(id)/L"version.json");
    if(installed(id)&&previous.value("sha256",std::string())==p.digest){log(wide(id)+tr(L": already up to date, version ")+wide(p.version));return;}
    fs::create_directories(root_);TempDir work(root_);auto archive=work.path/L"download";auto stage=work.path/L"ready";fs::create_directories(stage);
    log(tr(L"Source: ")+p.url);
    httpDownload(p.url,archive,cancel,[&](auto text,int pc){progress(wide(id)+L" — "+text,pc);});
    checkCancel(cancel);progress(tr(L"Verifying SHA256: ")+wide(id),-1);
    if(sha256(archive)!=p.digest)throw std::runtime_error(tr("SHA256 mismatch: ")+id+tr(". Previous version preserved."));
    auto exe=executable(id).filename();
    if(p.zip){
        progress(tr(L"Extracting ")+wide(id)+L"…",-1);
        std::vector<std::wstring> names{exe.wstring()};if(id=="ffmpeg")names.push_back(L"ffprobe.exe");
        extractZip(archive,stage,names,cancel,log);
    }else fs::copy_file(archive,stage/exe);
    progress(tr(L"Testing launch of ")+wide(id)+L"…",-1);
    auto args=std::vector<std::wstring>{(stage/exe).wstring(),id=="ffmpeg"?L"-version":L"--version"};
    if(runProcess(args,stage,cancel,log,40)!=0)throw std::runtime_error(id+tr(": component failed to start. Previous version preserved."));
    if(id=="ffmpeg"&&runProcess({(stage/L"ffprobe.exe").wstring(),L"-version"},stage,cancel,log,40)!=0)throw std::runtime_error(tr("ffprobe failed to start"));
    checkCancel(cancel);
    writeJson(stage/L"version.json",{{"version",p.version},{"sha256",p.digest},{"source",utf8(p.url)}});
    installDirectory(stage,directory(id));log(wide(id)+tr(L": installed version ")+wide(p.version));
}
void ComponentManager::prepare(std::atomic_bool& cancel,const Log& log,const Progress& progress){
    std::vector<std::string> failures;
    for(const auto& c:components()){
        try{install(c.id,false,cancel,log,progress);}catch(const Cancelled&){throw;}catch(const std::exception& e){log(tr(L"ERROR: ")+wide(e.what()));failures.push_back(c.id);}
    }
    if(!failures.empty()){
        std::string msg=tr("Not installed: ");for(auto& s:failures)msg+=s+" ";
        throw std::runtime_error(msg+tr(". Retry installation using the update buttons."));
    }
}
}

