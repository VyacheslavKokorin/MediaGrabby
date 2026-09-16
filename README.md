# MediaGrabby

**Video and music from a link.** A portable Windows desktop app powered by yt-dlp, with English and Russian interfaces.

[Download ZIP](../../releases/latest) · [Builds and tests](../../actions)

## Quick start

1. Download the ZIP for your Windows architecture: x64 or x86.
2. Extract it into a writable folder, such as `D:\MediaGrabby`.
3. Run `MediaGrabby.exe`.
4. Wait for yt-dlp, FFmpeg and Node.js to download. Progress appears in the app.
5. Paste a video or playlist URL, choose your options and click **Download**.

The ZIP contains only `MediaGrabby.exe` and `readme.txt`. No administrator privileges, Python installation or .NET SDK are required. Built-in Windows PowerShell extracts component archives. Run the app from the extracted folder, not from inside the ZIP.

English is the default on a fresh installation or when no language preference has been saved. Existing saved language choices are preserved. To change the language, open **Settings → Language**.

## Features

- Videos and playlists from websites supported by yt-dlp.
- The Windows **Videos** folder as the default destination, including relocated user folders.
- Best quality, 1080p, 720p, 480p and 360p presets. The selected resolution is a maximum height; smaller sources are allowed and never upscaled.
- MP4 video with original codecs by default; optional H.264, H.265 and AAC conversion.
- Audio-only MP3 downloads at VBR quality 0.
- Optional cookies from Chrome, Edge, Firefox, Brave, Opera, Vivaldi, Chromium or Whale.
- Automatic installation and updates for yt-dlp, FFmpeg/ffprobe and Node.js.
- Optional embedded metadata and thumbnails.
- Download progress, speed, remaining time, playlist position and processing status.
- Stop, retry, open destination folder and open the last completed file.
- Four visual styles, each with light and dark modes.
- HTTP and SOCKS5 proxy support for all downloads, including components.
- A collapsible terminal with wrapped lines, copy, clear and automatic scrolling.

## Settings and appearance

The **Settings** button at the top right opens a single window for appearance, language and proxy configuration.

| Setting | Choices |
|---|---|
| Interface style | Fluent, Graphite, Warm minimalism, Technical |
| Color mode | Follow Windows, Light, Dark |
| Language | English, Russian |
| Proxy | Disabled, HTTP, SOCKS5 |

Appearance and language can be previewed in the settings window. **Save** applies changes without restarting or clearing the URL and download options. **Cancel** discards the preview. Preferences are stored in `appearance.json`; download options are stored in `settings.json`.

## Terminal and scrolling

The collapsed terminal is fully hidden, even while new output arrives. Expand it to view the buffered output.

Automatic scrolling follows the actual viewport:

- Scroll all the way to the bottom to follow new lines automatically.
- Scroll up, even slightly, to stop following and read earlier output.
- Return to the bottom to resume following.

Incoming lines preserve the viewport and selection while you read older output. Resizing or collapsing the terminal does not change your follow preference. The on-screen buffer is bounded; its oldest lines are eventually removed, while full output remains in `logs` next to the executable.

Long lines wrap to the terminal width. The mouse wheel scrolls the terminal when the pointer is over it; elsewhere it scrolls the main form if needed. The wheel never changes dropdown selections. **Copy** copies the displayed log; **Clear** clears the display and resumes following, without deleting the full log file. Third-party tool output remains in its original language.

## Downloads and files

A direct YouTube video URL, including a watch URL containing a `list` parameter, downloads that video only. A playlist URL downloads the entire playlist into its own folder. On other websites, yt-dlp determines the URL type.

Each item is processed in an isolated `.MediaGrabby-work` folder. Only completed files are moved to the destination. **Stop** also terminates child FFmpeg processes and removes the incomplete item. Completed playlist items remain intact. Closing the app first stops active processes and performs cleanup.

The destination's `.mediagrabby-history.json` records completed downloads. Existing completed files are skipped; deleted files can be downloaded again. History distinguishes MP3/MP4, quality and explicitly selected codecs. Existing files are never overwritten. After a Windows crash, temporary files may remain in `.MediaGrabby-work`; you can remove that folder while MediaGrabby is closed.

**MP4 is a container.** Original video and audio streams are merged or remuxed without changing codecs by default. If the source codec cannot be stored in MP4, the app reports an error. Select H.264/H.265 and, if needed, AAC explicitly to convert it. Selecting H.264/H.265 always performs real conversion, even if the input is already MP4. Thumbnails may need conversion to JPEG. MP3 output requires audio conversion unless the source is already MP3.

Video conversion uses the CPU, preset `medium`, CRF 18 for H.264 and CRF 20 for H.265. AAC uses 192 kbit/s. Selecting **Original** for audio copies the existing stream.

After a failed download, **Retry** uses the previous attempt's URL and download options with the current proxy settings.

## Compatibility

| Operating system | App | Components |
|---|---|---|
| Windows 10 x64 | x64 or x86 EXE | yt-dlp, FFmpeg, Node.js |
| Windows 10 x86 | x86 EXE | yt-dlp, FFmpeg, Node.js |
| Windows 11 x64 | x64 or x86 EXE | yt-dlp, FFmpeg, Node.js |

Windows 7 is not supported. Node.js 22 is installed automatically for both architectures. There is no runtime selector. Old Deno/Bun/QuickJS folders from previous releases are unused and can be removed manually from `components`.

## Proxy

Proxy is disabled and unconfigured by default. In **Settings**, enable it and choose HTTP or SOCKS5. Enter a server address without a URL scheme, a port and, optionally, a username and password. IPv6 is supported. SOCKS5 resolves destination hostnames through the proxy.

The proxy applies to video information, media, thumbnails, release lists and installation/updates of yt-dlp, FFmpeg and Node.js. If an enabled proxy fails, the app does not fall back to a direct connection. Rare streams requiring FFmpeg's network downloader instead of yt-dlp's built-in downloader fail with proxy enabled to prevent FFmpeg from bypassing it.

**Test connection** checks access to GitHub and YouTube. Success is shown in green and failure in red, with a text explanation. Changing proxy fields clears the old result. A successful test does not guarantee access to every video.

Settings are disabled during active operations. Click **Stop**, wait for cleanup and then open Settings to change the proxy. This also applies during initial component installation.

The password is protected using Windows DPAPI in `proxy.json` and is usable by the same Windows account on the same computer. After moving the app to another computer, enter the password again. A decryption failure blocks automatic downloads until new proxy settings are saved. Proxy credentials are redacted from the terminal and logs.

## Component sources and updates

- [yt-dlp releases](https://github.com/yt-dlp/yt-dlp/releases)
- [FFmpeg x64 releases](https://github.com/GyanD/codexffmpeg/releases)
- [FFmpeg x86 releases](https://github.com/defisym/FFmpeg-Builds-Win32/releases)
- [Node.js 22](https://nodejs.org/dist/latest-v22.x/)

Components download over HTTPS with Windows certificate verification. SHA256 is checked against the source's published digest. Before replacement, the component is extracted into a staging folder and tested with a version command. A failed update preserves the previous component. Updates are disabled during video downloads. System-installed software, PATH and browsers are not modified.

To update MediaGrabby itself, close the app and replace `MediaGrabby.exe` and `readme.txt` from the new ZIP. Keep `components`, `logs` and your settings files.

## Troubleshooting

- Expand **Terminal** for details. Full logs are in `logs` next to the EXE.
- Update yt-dlp and, if needed, FFmpeg and Node.js.
- For cookie errors, close the browser, including background processes. Browser protection can prevent yt-dlp from reading Chromium cookies; availability depends on the browser and yt-dlp version. Try a signed-in Firefox profile or no cookies for public videos.
- If a component cannot download, check access to its source and retry its update.
- If a stream is incompatible with MP4, explicitly select conversion.
- DRM, website restrictions and authentication availability depend on yt-dlp and the website.
- Logs can contain URLs and local paths. Review them before sharing.

## Building

Requirements: Visual Studio 2022 with **Desktop development with C++**, and CMake 3.20 or newer.

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

Use `-A Win32` instead of `-A x64` for a 32-bit build.

C/C++ runtimes are linked statically. CMake downloads nlohmann/json 3.12.0 (MIT) and static libcurl 8.22.0 (curl license, TLS through Windows Schannel). The portable package needs no additional DLLs. Third-party notices are included in `readme.txt` and `resources/CURL-LICENSE.txt`.

English source strings are the translation keys. Russian translations live in `src/i18n.hpp`; existing numeric language preferences remain compatible (`0` Russian, `1` English). Unicode test fixtures intentionally include non-English text.

## Tests and releases

GitHub Actions builds both architectures and checks command generation, processes, cancellation, password protection, settings persistence and the real RichEdit terminal. Terminal tests cover hidden output, pause/resume at the bottom, selection preservation and resizing.

Integration tests install real components and download a generated test video through authenticated HTTP and SOCKS5 proxies. They cover MP4, MP3, H.264/AAC, H.265/AAC, completed-file skipping, remote DNS and no direct proxy bypass. GUI tests cover all eight style/color combinations, language changes, Save/Cancel behavior, dropdown wheel protection and terminal wrapping.

Portable ZIPs are published only after the checks pass. Automated tests do not cover a user's live YouTube session or their particular Windows installation.

## Security

Please report security vulnerabilities privately rather than opening a public issue. See [SECURITY.md](SECURITY.md) for details. Do not attach real browser cookies, proxy credentials, private URLs or unreviewed logs to public reports.

## License

MediaGrabby is released under the [MIT License](LICENSE). Third-party components and libraries remain subject to their own licenses; the corresponding notices are included with the project and portable package.
