#pragma once
#include "components.hpp"
namespace mg {
struct DownloadResult{int completed=0,skipped=0,failed=0;fs::path lastFile;std::wstring lastError;};
DownloadResult download(const Options& options,ComponentManager& components,std::atomic_bool& cancel,const Log& log,const Progress& progress,const std::function<void(const fs::path&)>& completed);
}

