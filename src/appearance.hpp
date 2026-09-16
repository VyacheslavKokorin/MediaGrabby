#pragma once
#include "platform.hpp"
#include "i18n.hpp"
#include <commctrl.h>
#include <dwmapi.h>
namespace mg {
struct Appearance {int style=0,theme=0,language=1;};
struct Palette {
    COLORREF bg,card,field,fg,muted,border,accent,onAccent,success,error;
    int radius=6;bool dark=false;
};
inline Appearance loadAppearance(const fs::path& root,int legacyTheme=0){
    Appearance a;a.theme=legacyTheme;
    try{auto j=readJson(root/L"appearance.json");a.style=std::clamp(j.value("style",0),0,3);a.theme=std::clamp(j.value("theme",legacyTheme),0,2);a.language=std::clamp(j.value("language",1),0,1);}catch(...){}
    return a;
}
inline void saveAppearance(const fs::path& root,const Appearance& a){writeJson(root/L"appearance.json",{{"style",a.style},{"theme",a.theme},{"language",a.language}});}
inline Palette paletteFor(const Appearance& a){
    DWORD light=1,n=sizeof(light);RegGetValueW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",L"AppsUseLightTheme",RRF_RT_REG_DWORD,nullptr,&light,&n);
    bool dark=a.theme==2||(a.theme==0&&!light);
    Palette p=dark?Palette{RGB(20,25,33),RGB(27,33,43),RGB(34,41,53),RGB(237,241,248),RGB(164,177,195),RGB(65,77,96),RGB(112,160,255),RGB(12,24,43),RGB(96,218,153),RGB(255,134,140),7,true}:
        Palette{RGB(242,245,251),RGB(255,255,255),RGB(255,255,255),RGB(26,37,57),RGB(91,106,128),RGB(198,209,225),RGB(53,106,230),RGB(255,255,255),RGB(23,117,70),RGB(184,34,49),7,false};
    if(a.style==1){
        p.radius=9;
        if(dark){p.bg=RGB(22,23,28);p.card=RGB(31,33,40);p.field=RGB(39,42,51);p.border=RGB(71,74,89);p.accent=RGB(157,143,255);p.onAccent=RGB(24,16,51);}
        else{p.bg=RGB(241,240,248);p.card=RGB(251,250,255);p.field=RGB(255,255,255);p.border=RGB(207,201,224);p.accent=RGB(103,79,202);}
    }else if(a.style==2){
        p.radius=3;
        if(dark){p.bg=RGB(24,29,26);p.card=p.bg;p.field=RGB(34,43,37);p.fg=RGB(237,237,225);p.muted=RGB(167,184,170);p.border=RGB(76,96,81);p.accent=RGB(116,197,155);p.onAccent=RGB(18,40,29);}
        else{p.bg=RGB(248,246,239);p.card=p.bg;p.field=RGB(255,254,249);p.fg=RGB(30,45,36);p.muted=RGB(99,112,101);p.border=RGB(190,198,184);p.accent=RGB(40,115,90);}
    }else if(a.style==3){
        p.radius=2;
        if(dark){p.bg=RGB(13,25,38);p.card=RGB(17,33,49);p.field=RGB(22,41,59);p.border=RGB(54,88,112);p.accent=RGB(54,197,232);p.onAccent=RGB(8,29,42);p.muted=RGB(152,180,201);}
        else{p.bg=RGB(234,242,247);p.card=RGB(244,249,252);p.field=RGB(255,255,255);p.border=RGB(164,192,209);p.accent=RGB(0,105,135);p.fg=RGB(18,44,61);}
    }
    return p;
}
void rounded(HDC dc,RECT r,COLORREF fill,COLORREF border,int radius);
void skinControl(HWND control,Palette* palette);
void drawButton(const DRAWITEMSTRUCT& d,const Palette& p,bool primary=false);
bool showSettingsDialog(HWND owner,const fs::path& root,Appearance& appearance);
}
