$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class Native {
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr h, int id);
  [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, string l);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindow(string c, string t);
  [DllImport("user32.dll")] public static extern int GetWindowLong(IntPtr h, int i);
  [DllImport("user32.dll", CharSet=CharSet.Unicode, EntryPoint="SendMessageW")] public static extern IntPtr ReadText(IntPtr h, uint m, IntPtr w, System.Text.StringBuilder l);
  public static string Text(IntPtr h) { var b=new System.Text.StringBuilder(4096); ReadText(h,0xD,(IntPtr)4096,b); return b.ToString(); }
  public struct RECT { public int L,T,R,B; }
}
'@
New-Item -ItemType Directory -Force screenshots | Out-Null
$exe = (Resolve-Path build\Release\MediaGrabby.exe).Path
$root = Split-Path $exe
$settings = Join-Path $root settings.json
$appearance = Join-Path $root appearance.json
$proxy = Join-Path $root proxy.json
function Send($h, $m, $w=0, $l=0) { [Native]::SendMessage($h,$m,[IntPtr]$w,[IntPtr]$l) | Out-Null }
function Click($h,$id) { [Native]::PostMessage([Native]::GetDlgItem($h,$id),0xF5,[IntPtr]::Zero,[IntPtr]::Zero) | Out-Null; Start-Sleep -Milliseconds 250 }
function Set-Choice($h,$id,$index) { Send ([Native]::GetDlgItem($h,$id)) 0x14E $index; Send $h 0x111 (65536+$id) ([Native]::GetDlgItem($h,$id)).ToInt64() }
function Shot($h,$name,$inline=$false) {
    [Native]::SetForegroundWindow($h) | Out-Null
    Start-Sleep -Milliseconds 200
    $r=New-Object Native+RECT
    [Native]::GetWindowRect($h,[ref]$r) | Out-Null
    $bmp=New-Object Drawing.Bitmap ($r.R-$r.L),($r.B-$r.T)
    $g=[Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($r.L,$r.T,0,0,$bmp.Size)
    $path=Join-Path (Resolve-Path screenshots) ($name+'.png')
    $bmp.Save($path);$g.Dispose();$bmp.Dispose()
    if($inline){Write-Output ('MG_SCREENSHOT_'+$name+':'+[Convert]::ToBase64String([IO.File]::ReadAllBytes($path)))}
}
# A fresh install must open in English without writing a language preference first.
Remove-Item $settings,$appearance,$proxy -ErrorAction SilentlyContinue
$fresh=Start-Process $exe -ArgumentList '--preview' -PassThru
try {
    Start-Sleep -Seconds 2
    $fresh.Refresh()
    if($fresh.HasExited -or $fresh.MainWindowHandle -eq 0){throw 'Fresh install failed to open'}
    if([Native]::Text([Native]::GetDlgItem($fresh.MainWindowHandle,113)) -ne 'Download'){throw 'Fresh install did not default to English'}
    if([Native]::IsWindowVisible([Native]::GetDlgItem($fresh.MainWindowHandle,123))){throw 'Collapsed terminal visible at startup'}
} finally {if(!$fresh.HasExited){$fresh.CloseMainWindow() | Out-Null;$fresh.WaitForExit(5000) | Out-Null;if(!$fresh.HasExited){$fresh.Kill()}}}
foreach($style in 0..3){foreach($theme in 1..2){
    $language=$style%2
    @{theme=1;quality=2} | ConvertTo-Json | Set-Content -Encoding utf8 $settings
    @{style=$style;theme=$theme;language=$language} | ConvertTo-Json | Set-Content -Encoding utf8 $appearance
    Remove-Item $proxy -ErrorAction SilentlyContinue
    $p=Start-Process $exe -ArgumentList '--preview' -PassThru
    try {
        Start-Sleep -Seconds 2
        $p.Refresh();$main=$p.MainWindowHandle
        if($p.HasExited -or $main -eq 0){throw 'GUI failed to open'}
        $expected=if($language -eq 0){'Скачать'}else{'Download'}
        if([Native]::Text([Native]::GetDlgItem($main,113)) -ne $expected){throw 'Startup language not applied'}
        foreach($id in @(110,112)){if([Native]::GetDlgItem($main,$id) -ne [IntPtr]::Zero){throw 'Old runtime/theme selector remains'}}
        [Native]::SendMessage([Native]::GetDlgItem($main,101),0xC,[IntPtr]::Zero,'https://example.com/keep-this-link') | Out-Null
        Shot $main "STYLE_${style}_${theme}" ($theme -eq ($style%2+1))
        $combo=[Native]::GetDlgItem($main,105)
        $before=[Native]::SendMessage($combo,0x147,[IntPtr]::Zero,[IntPtr]::Zero)
        Send $combo 0x20A (-120*65536)
        if([Native]::SendMessage($combo,0x147,[IntPtr]::Zero,[IntPtr]::Zero) -ne $before){throw 'Wheel changed quality'}
        Click $main 127
        $title=if($language -eq 0){'Настройки'}else{'Settings'}
        $dialog=[Native]::FindWindow('#32770',$title)
        if($dialog -eq [IntPtr]::Zero){throw 'Settings dialog did not open'}
        if([Native]::SendMessage([Native]::GetDlgItem($dialog,301),0xF0,[IntPtr]::Zero,[IntPtr]::Zero) -ne [IntPtr]::Zero){throw 'Proxy enabled by default'}
        Shot $dialog "SETTINGS_${style}_${theme}" ($style -eq 1 -and $theme -eq 2)
        # Verify immediate dialog preview, then Cancel must preserve the saved appearance.
        Set-Choice $dialog 401 (($style+1)%4)
        Set-Choice $dialog 403 (1-$language)
        $previewTitle=if($language -eq 0){'Settings'}else{'Настройки'}
        if([Native]::Text($dialog) -ne $previewTitle){throw 'Dialog language preview failed'}
        Click $dialog 2
        $saved=Get-Content $appearance -Raw | ConvertFrom-Json
        if($saved.style -ne $style -or $saved.language -ne $language){throw 'Cancel saved draft settings'}
        Click $main 127
        $dialog=[Native]::FindWindow('#32770',$title)
        # Invalid enabled proxy must keep dialog open and display a readable error.
        Click $dialog 301
        Click $dialog 307
        if([string]::IsNullOrWhiteSpace([Native]::Text([Native]::GetDlgItem($dialog,308)))){throw 'Missing proxy validation error'}
        Click $dialog 301
        Set-Choice $dialog 403 (1-$language)
        Click $dialog 1
        $expected=if($language -eq 0){'Download'}else{'Скачать'}
        if([Native]::Text([Native]::GetDlgItem($main,113)) -ne $expected){throw 'Language change needs restart'}
        if([Native]::Text([Native]::GetDlgItem($main,101)) -ne 'https://example.com/keep-this-link'){throw 'Language change lost URL'}
        if([Native]::SendMessage($combo,0x147,[IntPtr]::Zero,[IntPtr]::Zero) -ne $before){throw 'Language change lost quality'}
        $saved=Get-Content $appearance -Raw | ConvertFrom-Json
        if($saved.language -ne (1-$language)){throw 'Language not persisted'}
        Click $main 120
        $terminal=[Native]::GetDlgItem($main,123)
        if(![Native]::IsWindowVisible($terminal)){throw 'Terminal did not open'}
        if(([Native]::GetWindowLong($terminal,-16) -band 0x100000) -ne 0){throw 'Horizontal scrollbar remains'}
        [Native]::SendMessage($terminal,0xC,[IntPtr]::Zero,('https://example.com/'+('0123456789'*500))) | Out-Null
        if([Native]::SendMessage($terminal,0xBA,[IntPtr]::Zero,[IntPtr]::Zero).ToInt32() -lt 2){throw 'Terminal does not wrap'}
        Click $main 120
        if([Native]::IsWindowVisible($terminal)){throw 'Terminal remains visible after collapsing'}
        Write-Output "GUI verified style=$style theme=$theme, language switch/save/cancel, proxy validation, wheel, terminal wrap"
    } finally { if(!$p.HasExited){$p.CloseMainWindow() | Out-Null;$p.WaitForExit(5000) | Out-Null;if(!$p.HasExited){$p.Kill()}} }
}}
Remove-Item $settings,$appearance,$proxy -ErrorAction SilentlyContinue
