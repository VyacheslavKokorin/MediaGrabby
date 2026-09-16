MediaGrabby — Video and music from a link

QUICK START
1. Extract the ZIP into a writable folder, for example D:\MediaGrabby.
2. Run MediaGrabby.exe.
3. Wait for yt-dlp, FFmpeg and Node.js to download.
4. Paste a URL and click Download.

Supported: Windows 10 x86/x64 and Windows 11 x64. Windows 7 is not supported.
Use the x86 EXE for 32-bit Windows. No administrator privileges or Python
installation are needed. Do not run the EXE directly from inside the ZIP.
Built-in Windows PowerShell extracts component archives.

SETTINGS
Click Settings at the top right to configure appearance, language and proxy.
Styles: Fluent, Graphite, Warm minimalism, Technical.
Color modes: Follow Windows, Light, Dark.
Languages: English (default) and Russian. Existing saved choices are preserved.
Save applies changes without restarting or clearing download options.
Cancel discards the preview. Appearance preferences are in appearance.json.

TERMINAL
The terminal is fully hidden when collapsed, even if new output arrives.
Scroll to the very bottom to follow new lines automatically.
Scroll up to pause automatic scrolling; return to the bottom to resume it.
Long lines wrap to the window width. The wheel never changes dropdown values.
Copy copies the displayed log. Clear clears the display and resumes following.
Full logs remain in the logs folder. Third-party output is not translated.

PROXY
Disabled by default. HTTP and SOCKS5 are supported.
Enter a server address, port and optional username/password in Settings.
Proxy applies to ALL downloads, including component installation and updates.
If an enabled proxy fails, the app does not connect directly instead.
Test connection checks GitHub and YouTube: green for success, red for failure.
During downloads, click Stop and wait before changing settings.
Passwords are protected with Windows DPAPI in proxy.json and redacted in logs.
After moving to another computer, enter the proxy password again.

DOWNLOAD OPTIONS
Video: MP4 with original codecs by default; optional H.264, H.265 and AAC.
An incompatible MP4 source produces an error without hidden conversion.
Audio only: MP3. Metadata and thumbnails are embedded when available.
A video URL downloads the video; a playlist URL downloads the entire playlist.
Stop deletes the incomplete item and keeps completed files.
Completed files are skipped, and existing files are not overwritten.
Retry uses the previous attempt's options and current proxy settings.
Download preferences: settings.json. Components: components. Full logs: logs.
History: .mediagrabby-history.json in the destination folder.
Only Node.js is used. Old Deno/Bun/QuickJS component folders can be removed.

UPDATING MEDIAGRABBY
Close the app and replace MediaGrabby.exe and readme.txt from the new ZIP.
Keep the existing components, logs and settings files.

Project: https://github.com/VyacheslavKokorin/MediaGrabby

THIRD-PARTY NOTICE
This application includes nlohmann/json 3.12.0.

MIT License
Copyright (c) 2013-2025 Niels Lohmann

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.


This application also includes libcurl 8.22.0.

COPYRIGHT AND PERMISSION NOTICE

Copyright (c) 1996 - 2026, Daniel Stenberg, <daniel@haxx.se>, and many
contributors, see the THANKS file.

All rights reserved.

Permission to use, copy, modify, and distribute this software for any purpose
with or without fee is hereby granted, provided that the above copyright
notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF THIRD PARTY RIGHTS. IN
NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE
OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of a copyright holder shall not
be used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization of the copyright holder.
