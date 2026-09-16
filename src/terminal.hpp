#pragma once
#include <windows.h>
#include <commctrl.h>
#include <richedit.h>
#include <string>
namespace mg {
// Tracks the viewport after user input, independently of append/resize operations.
class TerminalLog {
    HWND window_=nullptr;
    bool following_=true,updating_=false;
    static LRESULT CALLBACK proc(HWND,UINT,WPARAM,LPARAM,UINT_PTR,DWORD_PTR);
public:
    void attach(HWND window);
    void append(const std::wstring& text);
    void clear();
    bool following()const{return following_;}
    bool atBottom()const;
};
}
