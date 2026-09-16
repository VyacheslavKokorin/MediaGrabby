#include "i18n.hpp"
#include "download.hpp"
#include "proxy.hpp"
#include <fstream>
#include <sstream>
namespace mg {
static std::vector<std::wstring> baseArgs(const Options& o,ComponentManager& cm){
    std::string runtime="node";
    if(!cm.supported(runtime)||!cm.installed(runtime))throw std::runtime_error(tr("Node.js is not installed. Click Update Node.js."));
    std::vector<std::wstring>a{cm.executable("ytdlp").wstring(),L"--ignore-config",L"--no-plugin-dirs",L"--no-cache-dir",
        L"--encoding",L"utf-8",L"--newline",L"--no-colors",L"--no-js-runtimes",L"--js-runtimes",
        wide(runtime)+L":"+cm.executable(runtime).wstring(),L"--ffmpeg-location",cm.directory("ffmpeg").wstring(),
        L"--socket-timeout",L"30",L"--retries",L"3",L"--fragment-retries",L"3",L"--verbose"};
    a.insert(a.end(),{L"--proxy",proxyUrl(getProxy())});
    if(getProxy().enabled)a.insert(a.end(),{L"--downloader-args",L"ffmpeg_i:-protocol_whitelist file,pipe,crypto,data"});
    if(o.browser)a.insert(a.end(),{L"--cookies-from-browser",browserId(o.browser)});
    return a;
}
static std::string stringValue(const json& j,const char* key,const std::string& fallback=""){
    return j.contains(key)&&j[key].is_string()?j[key].get<std::string>():fallback;
}
static fs::path ownedPath(const fs::path& root,const std::wstring& path){
    auto candidate=fs::weakly_canonical(fs::path(path));auto base=fs::weakly_canonical(root);
    auto rel=candidate.lexically_relative(base);
    if(rel.empty()||rel.is_absolute()||*rel.begin()==L"..")throw std::runtime_error(tr("Downloader returned a file outside the working folder"));
    if(!fs::is_regular_file(candidate))throw std::runtime_error(tr("Downloaded file not found"));
    return candidate;
}
DownloadResult download(const Options& o,ComponentManager& cm,std::atomic_bool& cancel,const Log& log,const Progress& progress,const std::function<void(const fs::path&)>& onCompleted){
    if(!validUrl(o.url))throw std::runtime_error(tr("Enter a complete http:// or https:// URL"));
    if(!cm.installed("ytdlp")||!cm.installed("ffmpeg"))throw std::runtime_error(tr("Install yt-dlp and FFmpeg using the update buttons first"));
    ensureWritable(o.folder);auto common=baseArgs(o,cm);
    std::wstring lastIssue;
    auto originalLog=log;
    Log safeLog=[&](const std::wstring& line){if(line.find(L"ERROR:")!=line.npos)lastIssue=friendlyError(line); originalLog(redactProxy(line,getProxy()));};
    progress(tr(L"Getting video or playlist information…"),-1);
    auto infoArgs=common;infoArgs.insert(infoArgs.end(),{L"--dump-single-json",L"--flat-playlist",L"--skip-download",L"--ignore-errors"});
    infoArgs.push_back(youtubeVideo(o.url)?L"--no-playlist":L"--yes-playlist");
    infoArgs.insert(infoArgs.end(),{L"--",o.url});json info;
    int infoCode=runProcess(infoArgs,o.folder,cancel,[&](const auto& line){
        safeLog(line);
        if(!line.empty()&&line[0]==L'{'){auto j=json::parse(utf8(line),nullptr,false);if(!j.is_discarded()&&j.is_object())info=std::move(j);}
    },180);
    if(info.is_null()||(infoCode!=0&&!info.contains("entries")))throw std::runtime_error(utf8(lastIssue.empty()?tr(L"Could not get information. See the terminal for details."):lastIssue));
    std::vector<json> entries;
    bool playlist=info.contains("entries")&&info["entries"].is_array();
    if(playlist)for(const auto& entry:info["entries"])entries.push_back(entry);
    else entries.push_back(info);
    if(entries.empty())throw std::runtime_error(tr("Playlist is empty or unavailable"));
    fs::path folder=o.folder;
    if(playlist){folder/=safeName(wide(stringValue(info,"title","Playlist")));ensureWritable(folder);}
    auto historyPath=folder/L".mediagrabby-history.json";auto history=readJson(historyPath);
    if(!history.is_object())history=json::object();
    DownloadResult result;size_t index=0;
    for(const auto& entry:entries){
        checkCancel(cancel);++index;
        if(!entry.is_object()){++result.failed;log(tr(L"Playlist item unavailable"));continue;}
        auto id=stringValue(entry,"id");
        auto title=wide(stringValue(entry,"title",id.empty()?"Video":id));
        auto prefix=L"["+std::to_wstring(index)+L"/"+std::to_wstring(entries.size())+L"] ";
        std::string address=playlist?stringValue(entry,"webpage_url",stringValue(entry,"url")):utf8(o.url);
        std::string extractor=stringValue(entry,"extractor_key",stringValue(entry,"ie_key",stringValue(info,"extractor_key")));
        if(address.rfind("http://",0)!=0&&address.rfind("https://",0)!=0){
            if(extractor=="Youtube"&&!id.empty())address="https://www.youtube.com/watch?v="+id;
            else{++result.failed;log(prefix+tr(L"Could not determine URL: ")+title);continue;}
        }
        // Separate media types and explicit encodings; a successful file, not an attempted download, is archived.
        auto key=(id.empty()?address:extractor+":"+id)+(o.audioOnly?":mp3":":mp4:q"+std::to_string(o.quality)+":v"+std::to_string(o.videoCodec)+":a"+std::to_string(o.audioCodec));
        if(history.contains(key)&&history[key].is_string()){
            fs::path saved=wide(history[key].get<std::string>());
            if(saved==saved.filename()&&fs::is_regular_file(folder/saved)){
                ++result.skipped;result.lastFile=folder/saved;onCompleted(result.lastFile);log(prefix+tr(L"Already downloaded: ")+title);continue;
            }
        }
        progress(prefix+title,-1);
        try{
            TempDir work(folder/L".MediaGrabby-work");
            auto args=common;
            args.insert(args.end(),{L"--no-playlist",L"--no-simulate",L"--windows-filenames",L"--no-overwrites",L"--no-write-playlist-metafiles",
                L"--paths",work.path.wstring(),L"--paths",L"temp:"+work.path.wstring(),L"-o",L"%(title).120B [%(id)s].%(ext)s",
                L"-f",formatSelector(o.quality,o.audioOnly),L"--progress",L"--progress-delta",L"0.3",
                L"--progress-template",L"download:__MG_PROGRESS__%(progress._percent_str)s|%(progress._speed_str)s|%(progress._eta_str)s",
                L"--print",L"after_move:__MG_FILE__%(filepath)j"});
            if(o.audioOnly)args.insert(args.end(),{L"--extract-audio",L"--audio-format",L"mp3",L"--audio-quality",L"0"});
            else if(o.needsConversion())args.insert(args.end(),{L"--merge-output-format",L"mkv"});
            else args.insert(args.end(),{L"--merge-output-format",L"mp4/mkv",L"--remux-video",L"mp4"});
            if(o.metadata){
                args.push_back(L"--embed-metadata");
                if(o.needsConversion())args.insert(args.end(),{L"--write-thumbnail",L"--convert-thumbnails",L"jpg"});
                else args.push_back(L"--embed-thumbnail");
            }
            args.insert(args.end(),{L"--",wide(address)});
            fs::path source;
            int code=runProcess(args,work.path,cancel,[&](const auto& line){
                safeLog(line);
                if(line.rfind(L"__MG_FILE__",0)==0){
                    auto j=json::parse(utf8(line.substr(11)),nullptr,false);
                    if(j.is_string())source=ownedPath(work.path,wide(j.get<std::string>()));
                }else if(line.rfind(L"__MG_PROGRESS__",0)==0){
                    auto data=line.substr(15);auto a=data.find(L'|');auto b=data.find(L'|',a==data.npos?0:a+1);
                    int pc=-1;try{pc=(int)std::stod(data);}catch(...){}
                    auto text=prefix+title+L" — "+(a==data.npos?data:data.substr(0,a));
                    if(a!=data.npos&&b!=data.npos)text+=L"  •  "+data.substr(a+1,b-a-1)+tr(L"  •  remaining ")+data.substr(b+1);
                    progress(text,std::clamp(pc,-1,100));
                }else if(!line.empty()&&line[0]==L'['&&line.rfind(L"[download]",0)!=0&&line.rfind(L"[debug]",0)!=0){
                    progress(prefix+tr(L"Processing: ")+title,-1);
                }
            });
            if(code!=0||source.empty())throw std::runtime_error(utf8(lastIssue.empty()?tr(L"Downloader failed. See the terminal."):lastIssue));
            checkCancel(cancel);
            if(o.needsConversion()){
                progress(prefix+tr(L"Converting: ")+title,-1);
                auto converted=work.path/L"converted.mp4";
                std::vector<std::wstring> ff{cm.executable("ffmpeg").wstring(),L"-hide_banner",L"-nostdin",L"-y",L"-i",source.wstring()};
                fs::path cover;
                if(o.metadata)for(const auto& p:fs::directory_iterator(work.path))if(p.path().extension()==L".jpg"){cover=p.path();break;}
                if(!cover.empty())ff.insert(ff.end(),{L"-i",cover.wstring()});
                auto codecs=conversionArgs(o);ff.insert(ff.end(),codecs.begin(),codecs.end());
                if(!cover.empty())ff.insert(ff.end(),{L"-map",L"1:v:0",L"-c:v:1",L"mjpeg",L"-disposition:v:1",L"attached_pic"});
                ff.insert(ff.end(),{L"-movflags",L"+faststart",converted.wstring()});
                if(runProcess(ff,work.path,cancel,log)!=0)throw std::runtime_error(tr("Conversion failed. The final file was not saved."));
                if(!fs::is_regular_file(converted)||fs::file_size(converted)==0)throw std::runtime_error(tr("FFmpeg did not create a video"));
                auto finalName=source.filename();finalName.replace_extension(L".mp4");
                // Keep user-selected profiles distinct so an original file never blocks explicit conversion.
                std::wstring suffix=o.videoCodec==1?L" [H264]":o.videoCodec==2?L" [H265]":L"";
                if(o.audioCodec)suffix+=L" [AAC]";
                auto named=work.path/(finalName.stem().wstring()+suffix+L".mp4");
                if(named==source){fs::remove(source);} // source is an owned staging file, never a user's existing download
                fs::rename(converted,named);source=named;
            }
            auto required=o.audioOnly?L".mp3":L".mp4";
            if(source.extension()!=required)throw std::runtime_error(tr("Could not save the requested format. Automatic conversion is disabled."));
            // Verify the resulting file can be parsed before marking it completed.
            if(runProcess({(cm.directory("ffmpeg")/L"ffprobe.exe").wstring(),L"-v",L"error",L"-show_entries",L"format=duration,size",L"-of",L"json",source.wstring()},work.path,cancel,log,45)!=0)
                throw std::runtime_error(tr("Final file verification failed"));
            checkCancel(cancel);
            auto targetName=source.filename();
            if(!o.audioOnly&&o.quality>0){
                const wchar_t* qualities[]={L"",L"1080p",L"720p",L"480p",L"360p"};
                targetName=targetName.stem().wstring()+L" ["+qualities[std::clamp(o.quality,0,4)]+L"]"+targetName.extension().wstring();
            }
            auto target=folder/targetName;
            if(MoveFileExW(source.c_str(),target.c_str(),MOVEFILE_WRITE_THROUGH)){++result.completed;}
            else if(GetLastError()==ERROR_ALREADY_EXISTS||GetLastError()==ERROR_FILE_EXISTS){++result.skipped;log(tr(L"File already exists — skipped"));}
            else throw std::runtime_error(utf8(winError(tr(L"Saving file"))));
            result.lastFile=target;onCompleted(target);
            history[key]=utf8(target.filename().wstring());
            try{writeJson(historyPath,history);}catch(const std::exception& e){log(tr(L"File saved; could not update history: ")+wide(e.what()));}
            log(prefix+tr(L"Done: ")+target.wstring());
        }catch(const Cancelled&){log(prefix+tr(L"Stopped. Incomplete files removed."));throw;}
        catch(const std::exception& e){++result.failed;result.lastError=friendlyError(wide(e.what()));log(prefix+tr(L"ERROR: ")+result.lastError);}
    }
    return result;
}
}

