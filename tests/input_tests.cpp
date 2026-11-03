// Exercise the real edit controls and command handlers without network downloads.
#define wWinMain MediaGrabbyTestEntry
#include "../src/main.cpp"
#undef wWinMain
#include <iostream>
#include <stdexcept>

namespace {
LRESULT CALLBACK testWindow(HWND h,UINT m,WPARAM w,LPARAM l){
    auto* app=reinterpret_cast<App*>(GetWindowLongPtrW(h,GWLP_USERDATA));
    if(m==WM_COMMAND&&app){app->command(LOWORD(w),HIWORD(w));return 0;}
    return DefWindowProcW(h,m,w,l);
}
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void clipboard(HWND owner,const wchar_t* text){
    require(OpenClipboard(owner)!=FALSE,"Open clipboard");
    EmptyClipboard();
    SIZE_T size=(wcslen(text)+1)*sizeof(wchar_t);
    HGLOBAL data=GlobalAlloc(GMEM_MOVEABLE,size);
    auto* memory=GlobalLock(data);
    memcpy(memory,text,size);GlobalUnlock(data);
    auto result=SetClipboardData(CF_UNICODETEXT,data);
    CloseClipboard();
    require(result!=nullptr,"Set clipboard");
}
}
int main(){
    try{
        INITCOMMONCONTROLSEX controls{sizeof(controls),ICC_STANDARD_CLASSES};
        InitCommonControlsEx(&controls);
        App app;
        WNDCLASSW wc{};wc.lpfnWndProc=testWindow;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"MediaGrabbyInputTest";
        RegisterClassW(&wc);
        app.window=CreateWindowW(wc.lpszClassName,L"",WS_OVERLAPPED,0,0,640,480,nullptr,nullptr,wc.hInstance,nullptr);
        require(app.window!=nullptr,"Create window");
        SetWindowLongPtrW(app.window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(&app));
        app.add(L"EDIT",L"",URL,ES_AUTOHSCROLL);
        app.add(L"EDIT",L"",FOLDER,ES_AUTOHSCROLL);
        app.button(START,L"Download");app.button(PASTE,L"Paste");app.button(STOP,L"Stop");
        const wchar_t* first=L"https://example.com/first";
        const wchar_t* second=L"https://example.com/second";
        clipboard(app.window,first);app.command(PASTE,BN_CLICKED);
        require(app.text(URL)==first,"First paste");
        app.canRetry=true;
        Event failure{1,L"Failed",-2};app.event(failure);
        require(app.retryShown,"Failure offers retry");
        clipboard(app.window,second);app.command(PASTE,BN_CLICKED);
        require(app.text(URL)==second,"Paste replaces old URL");
        require(!app.retryShown&&!app.canRetry,"New URL clears old retry");
        require(app.text(START)==tr(L"Download"),"Download caption restored");
        // Ctrl+A followed by the native paste command must replace all text.
        app.canRetry=true;app.event(failure);
        SendMessageW(app.get(URL),WM_CHAR,1,0);
        DWORD begin=0,end=0;SendMessageW(app.get(URL),EM_GETSEL,(WPARAM)&begin,(LPARAM)&end);
        require(begin==0&&end==wcslen(second),"Ctrl+A selects whole URL");
        clipboard(app.window,first);SendMessageW(app.get(URL),WM_PASTE,0,0);
        require(app.text(URL)==first&&!app.retryShown,"Keyboard paste resets retry");
        app.busy=true;app.refresh();
        require(!IsWindowEnabled(app.get(URL)),"Input disabled while busy");
        Event finished{2,L""};app.event(finished);
        require(IsWindowEnabled(app.get(URL))&&IsWindowEnabled(app.get(PASTE)),"Input restored after operation");
        clipboard(app.window,second);app.command(PASTE,BN_CLICKED);
        require(app.collect().url==second,"Next download collects new URL");
        DestroyWindow(app.window);
        std::cout<<"URL replacement and retry tests passed\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
}
