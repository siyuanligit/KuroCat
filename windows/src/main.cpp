#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#ifndef WINVER
#define WINVER 0x0601
#endif
#define NOMINMAX
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <shellapi.h>
#include <winhttp.h>
// MSVC's GDI+ headers call unqualified min()/max() inside inline helpers; NOMINMAX
// removes the macros, so the std overloads must be visible before <gdiplus.h>.
#include <algorithm>
using std::min;
using std::max;
#include <objidl.h>
#include <gdiplus.h>
#include <wtsapi32.h>
#include <string>
#include <vector>
#include <cmath>
#include <cwchar>
#include "KeyMap.hpp"
#include "InputEngine.hpp"
#include "CatPhysics.hpp"
#include "CatRenderer.hpp"
#include "SkinLibrary.hpp"

#ifndef WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY
#define WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY 4
#endif
#ifndef WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_1
#define WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_1 0x00000200
#endif
#ifndef WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2
#define WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 0x00000800
#endif

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "wtsapi32.lib")

namespace {

constexpr const wchar_t* APP_VERSION = L"1.4.3";
constexpr const wchar_t* VERSION_PROP = L"KuroCatVersion"; // set on the tray owner window so newer builds can take over older ones
constexpr const wchar_t* TRAY_OWNER_CLASS = L"KuroCatTrayOwnerClass";
constexpr const wchar_t* OVERLAY_CLASS = L"KuroCatOverlayWindowClass";
constexpr double FRAME_INTERVAL = 1.0 / 60.0;
constexpr const wchar_t* UPDATE_PAGE_URL = L"https://kurocat-download.app.workbuddy.host/?sharecode=JVAABIVZ7L";
constexpr const wchar_t* UPDATE_WIN_EXE_PATH = L"/downloads/KuroCat-Windows-x64.exe?sharecode=JVAABIVZ7L";
constexpr UINT WM_TRAYICON = WM_APP + 1;
constexpr UINT WM_WAKE_EXISTING = WM_APP + 2;
constexpr UINT WM_UPDATE_CHECK_DONE = WM_APP + 3;
constexpr UINT WM_UPDATE_DOWNLOAD_DONE = WM_APP + 4;
constexpr UINT WM_REQUEST_QUIT = WM_APP + 5; // posted by a newer / elevated instance that is taking over
constexpr UINT_PTR TIMER_RENDER = 1;
constexpr int HOTKEY_CLICKTHROUGH_ID = 101;
UINT g_msgTaskbarCreated = 0;

enum MenuCmd : UINT {
    CMD_TOGGLE_VISIBLE = 1001,
    CMD_TOGGLE_CLICKTHROUGH,
    CMD_TOGGLE_TOPMOST,
    CMD_TOGGLE_AUTOHIDE_FS,
    CMD_TOGGLE_AUTOSTART,
    CMD_TOGGLE_BADGES,
    CMD_TOGGLE_MOUSE_BADGES,
    CMD_TOGGLE_KEY_LEGENDS,
    CMD_TOGGLE_FOOT,
    CMD_TOGGLE_TAIL,
    CMD_TOGGLE_MARQUEE,
    CMD_TOGGLE_PET,
    CMD_OBS_OFF,
    CMD_OBS_TRANSPARENT,
    CMD_OBS_GREENSCREEN,
    CMD_TOGGLE_OBS_HIDE,
    CMD_TOGGLE_EDIT,
    CMD_LANG_ZH,
    CMD_LANG_EN,
    CMD_COAT_BLACK,
    CMD_COAT_SIAMESE,
    CMD_COAT_SHORTHAIR,
    CMD_COAT_ORANGE_WHITE,
    CMD_COAT_DAZED,
    CMD_KB_MILK,
    CMD_KB_HACKER,
    CMD_KB_RETRO,
    CMD_EYE_LIME,
    CMD_EYE_BLUE,
    CMD_EYE_AMBER,
    CMD_EYE_CYAN,
    CMD_EYE_SOFT_AMBER,
    CMD_EYE_REFERENCE_HAZEL,
    CMD_SCALE_65,
    CMD_SCALE_85,
    CMD_SCALE_100,
    CMD_SCALE_125,
    CMD_SCALE_150,
    CMD_DEMO_BURST,
    CMD_RESET_POS,
    CMD_CHECK_UPDATE,
    CMD_OPEN_UPDATE,
    CMD_RELAUNCH_ADMIN,
    CMD_TOGGLE_RUN_AS_ADMIN,
    CMD_QUIT,
    CMD_COAT_TABBY,
    CMD_COAT_BRITISH,
    CMD_KB_GRAPHITE,
    CMD_EYE_OLIVE_GOLD,
    CMD_TABLET_AUTO,
    CMD_TABLET_ON,
    CMD_TABLET_OFF,
    CMD_IMPORT_SKIN,
    CMD_OPEN_WORKSHOP,
    CMD_OPEN_SKINS_DIR,
    CMD_IMPORT_KB,
    CMD_OPEN_KB_PLAZA,
    CMD_OPEN_KB_DIR,
    CMD_OPEN_SETTINGS,
    CMD_SKIN_SELECT_BASE = 4000,  // + index into AppState::skins.skins
    CMD_SKIN_DELETE_BASE = 5000,
    CMD_KB_SELECT_BASE = 6000,    // + index into AppState::kbLibrary.skins
    CMD_KB_DELETE_BASE = 7000,
    CMD_SKIN_RANGE = 900
};
constexpr const wchar_t* DRESSING_ROOM_CLASS = L"KuroCatDressingRoomClass";
constexpr const wchar_t* SETTINGS_WINDOW_CLASS = L"KuroCatSettingsWindowClass";
constexpr int IDC_DRESSING_CHOOSE = 11;

struct AppState {
    HWND hwnd = nullptr;
    HWND menuOwnerHwnd = nullptr; // Non-NOACTIVATE helper window so TrackPopupMenu from tray NEVER fails!
    HICON trayIcon = nullptr;
    NOTIFYICONDATAW nid = {};

    CatSettings settings;
    InputEngine input;
    CatPhysics physics;
    SkinLibrary skins;
    KeyboardSkinLibrary kbLibrary;
    HWND dressingRoom = nullptr;    // 「换装间」 import window (nullptr when closed)
    std::wstring dressingStatusText;  // status line of 换装间; kind 0 neutral, 1 success, 2 error
    int dressingStatusKind = 0;
    float dressingScale = 1.0f;       // DPI scale of the 换装间 layout (design units are 96-dpi pixels)
    HWND settingsWnd = nullptr;     // unified native Settings window (nullptr when closed)
    HFONT settingsFont = nullptr;
    HFONT settingsBoldFont = nullptr;
    HBRUSH settingsBgBrush = nullptr;
    bool settingsDark = false;
    bool settingsUpdating = false;

    bool isVisible = true;
    bool hiddenByFullscreen = false;
    bool isDragging = false;
    int activeResizeCorner = -1; // -1 = none, 0=TL, 1=TR, 2=BL, 3=BR
    POINT resizeAnchorScreenPt = {};
    bool isRotating = false;
    bool rotateMoved = false;
    float rotateStartAngle = 0.0f;
    float rotateStartDeg = 0.0f;
    bool rButtonDown = false;
    bool pendingMenuAfterAlt = false;
    POINT pendingMenuPt = {};
    POINT dragStartCursor = {};
    POINT dragStartWinOrigin = {};
    bool hasLastHover = false;
    Pt lastHoverCanvas = {};

    // Reusable 32-bit premultiplied DIB backing buffer
    HDC memDC = nullptr;
    HBITMAP dibBmp = nullptr;
    HBITMAP oldBmp = nullptr;
    void* dibBits = nullptr;
    int bufW = 0;
    int bufH = 0;

    double lastTickTime = 0.0;
    bool demoActive = false;
    double demoStartTime = 0.0;
    int demoStepIdx = 0;

    std::wstring availableUpdateVersion;
    std::wstring availableUpdateUrl;
    bool isUpdating = false;

    // Robustness against games, Win+L, sleep and display changes
    double nextFrameTime = 0.0;
    double lastRenderTime = 0.0;
    double nextHousekeepingTime = 0.0;
    double pendingTopmostTime = 0.0;
    int ulwFailures = 0;
    bool isElevated = false;
    bool foregroundBlocksKeys = false;   // foreground app runs as admin -> Windows (UIPI) hides its keys from us
    double lastBlockedForegroundTime = 0.0;
    double blockedForegroundSince = 0.0;
    bool warnedElevatedForeground = false;
    HWINEVENTHOOK foregroundHook = nullptr;
    bool sessionNotifyRegistered = false;

    void destroyBuffer() {
        if (memDC && oldBmp) SelectObject(memDC, oldBmp);
        if (dibBmp) DeleteObject(dibBmp);
        if (memDC) DeleteDC(memDC);
        memDC = nullptr; dibBmp = nullptr; oldBmp = nullptr; dibBits = nullptr;
        bufW = 0; bufH = 0;
    }

    void ensureBuffer(int w, int h) {
        if (w == bufW && h == bufH && memDC && dibBits) return;
        destroyBuffer();

        HDC screenDC = GetDC(nullptr);
        memDC = CreateCompatibleDC(screenDC);
        ReleaseDC(nullptr, screenDC);

        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = w;
        bmi.bmiHeader.biHeight = -h; // top-down 32bpp DIB
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        dibBmp = CreateDIBSection(memDC, &bmi, DIB_RGB_COLORS, &dibBits, nullptr, 0);
        if (dibBmp) {
            oldBmp = (HBITMAP)SelectObject(memDC, dibBmp);
            bufW = w;
            bufH = h;
        }
    }
};

AppState* g_app = nullptr;

HICON createCatTrayIcon();

// MARK: - Windows Auto-Start (HKCU\Software\Microsoft\Windows\CurrentVersion\Run)

std::wstring getCurrentExeQuotedPath() {
    wchar_t buf[MAX_PATH * 2] = {};
    DWORD len = GetModuleFileNameW(nullptr, buf, (DWORD)(sizeof(buf) / sizeof(buf[0]) - 1));
    if (len == 0) return L"";
    return L"\"" + std::wstring(buf, len) + L"\"";
}

bool isAutoStartEnabled() {
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        return false;
    }
    DWORD type = 0, size = 0;
    bool exists = (RegQueryValueExW(hKey, L"KuroCat", nullptr, &type, nullptr, &size) == ERROR_SUCCESS && size > 0);
    RegCloseKey(hKey);
    return exists;
}

bool setAutoStartEnabled(bool enable) {
    HKEY hKey = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr) != ERROR_SUCCESS) {
        return false;
    }
    bool ok = false;
    if (enable) {
        std::wstring cmd = getCurrentExeQuotedPath();
        if (!cmd.empty()) {
            ok = (RegSetValueExW(hKey, L"KuroCat", 0, REG_SZ, (const BYTE*)cmd.c_str(), (DWORD)((cmd.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS);
        }
    } else {
        LSTATUS st = RegDeleteValueW(hKey, L"KuroCat");
        ok = (st == ERROR_SUCCESS || st == ERROR_FILE_NOT_FOUND);
    }
    RegCloseKey(hKey);
    return ok;
}

void showTrayBalloon(AppState& app, const wchar_t* title, const wchar_t* body) {
    NOTIFYICONDATAW b = app.nid;
    b.uFlags = NIF_INFO;
    b.dwInfoFlags = NIIF_INFO | NIIF_NOSOUND;
    wcsncpy_s(b.szInfoTitle, title, _TRUNCATE);
    wcsncpy_s(b.szInfo, body, _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &b);
}

// MARK: - Elevation (UIPI) helpers
//
// Many games (and launchers such as WeGame / anti-cheat clients) run as administrator. Windows'
// User Interface Privilege Isolation then refuses to deliver that window's keystrokes to normal
// programs — Raw Input, GetAsyncKeyState and low-level hooks all go silent — while mouse movement
// is still visible. That is exactly the "cat only sees the mouse in games" report. The only fix
// is for KuroCat to run at the same level, so we detect it and offer an admin relaunch.

bool isCurrentProcessElevated() {
    HANDLE tok = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok)) return false;
    TOKEN_ELEVATION te = {};
    DWORD sz = 0;
    bool elevated = GetTokenInformation(tok, TokenElevation, &te, sizeof(te), &sz) && te.TokenIsElevated != 0;
    CloseHandle(tok);
    return elevated;
}

bool isProcessElevatedOrProtected(DWORD pid) {
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return GetLastError() == ERROR_ACCESS_DENIED; // protected / anti-cheat processes
    bool elevated = false;
    HANDLE tok = nullptr;
    if (OpenProcessToken(h, TOKEN_QUERY, &tok)) {
        TOKEN_ELEVATION te = {};
        DWORD sz = 0;
        if (GetTokenInformation(tok, TokenElevation, &te, sizeof(te), &sz)) elevated = te.TokenIsElevated != 0;
        CloseHandle(tok);
    } else {
        elevated = (GetLastError() == ERROR_ACCESS_DENIED);
    }
    CloseHandle(h);
    return elevated;
}

bool commandLineHasFlag(const wchar_t* flag) {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return false;
    bool found = false;
    for (int i = 1; i < argc; ++i) {
        if (_wcsicmp(argv[i], flag) == 0) { found = true; break; }
    }
    LocalFree(argv);
    return found;
}

/// Starts a new elevated copy of this exe (UAC prompt). Returns false if the user declined.
bool relaunchSelfElevated(const wchar_t* params) {
    wchar_t exe[MAX_PATH * 2] = {};
    if (GetModuleFileNameW(nullptr, exe, (DWORD)(sizeof(exe) / sizeof(exe[0]) - 1)) == 0) return false;
    SHELLEXECUTEINFOW sei = {};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOASYNC;
    sei.lpVerb = L"runas";
    sei.lpFile = exe;
    sei.lpParameters = params;
    sei.nShow = SW_SHOWNORMAL;
    return ShellExecuteExW(&sei) != FALSE;
}

/// True when the foreground window covers its whole monitor (WPS PPT slideshow, fullscreen video, games).
bool isForegroundFullscreen(HWND selfHwnd) {
    HWND fg = GetForegroundWindow();
    if (!fg || fg == selfHwnd || fg == GetDesktopWindow() || fg == GetShellWindow()) return false;
    wchar_t cls[64] = {};
    GetClassNameW(fg, cls, 63);
    if (wcscmp(cls, L"WorkerW") == 0 || wcscmp(cls, L"Progman") == 0 ||
        wcscmp(cls, L"Shell_TrayWnd") == 0 || wcscmp(cls, L"Shell_SecondaryTrayWnd") == 0) return false;
    RECT wr = {};
    if (!GetWindowRect(fg, &wr)) return false;
    MONITORINFO mi = {};
    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfoW(MonitorFromWindow(fg, MONITOR_DEFAULTTONEAREST), &mi)) return false;
    return wr.left <= mi.rcMonitor.left + 2 && wr.top <= mi.rcMonitor.top + 2 &&
           wr.right >= mi.rcMonitor.right - 2 && wr.bottom >= mi.rcMonitor.bottom - 2;
}

bool hasVisibleWindowAbove(HWND hwnd) {
    if (!hwnd) return false;
    HWND prev = GetWindow(hwnd, GW_HWNDPREV);
    int steps = 0;
    while (prev && steps++ < 16) {
        if (IsWindowVisible(prev)) {
            RECT r = {};
            if (GetWindowRect(prev, &r) && (r.right - r.left) > 4 && (r.bottom - r.top) > 4) {
                return true;
            }
        }
        prev = GetWindow(prev, GW_HWNDPREV);
    }
    return false;
}

// MARK: - Auto Update Checker (GitHub Releases via WinHTTP)

struct UpdateCheckResult {
    bool ok = false;
    bool manual = false;
    bool hasUpdate = false;
    std::wstring latestTag;
    std::wstring releaseUrl;
};

std::string extractJsonStringField(const std::string& json, const char* fieldName) {
    std::string key = std::string("\"") + fieldName + "\"";
    size_t pos = json.find(key);
    if (pos == std::string::npos) return "";
    pos = json.find(':', pos + key.size());
    if (pos == std::string::npos) return "";
    pos = json.find('"', pos + 1);
    if (pos == std::string::npos) return "";
    size_t end = json.find('"', pos + 1);
    if (end == std::string::npos) return "";
    return json.substr(pos + 1, end - pos - 1);
}

bool isSemVerNewer(const std::wstring& lhs, const std::wstring& rhs) {
    auto parse = [](const std::wstring& v) {
        std::vector<int> parts;
        size_t i = 0;
        while (i < v.size() && (v[i] == L'v' || v[i] == L'V' || v[i] == L' ')) i++;
        int cur = 0; bool inNum = false;
        for (; i < v.size(); ++i) {
            if (v[i] >= L'0' && v[i] <= L'9') {
                cur = cur * 10 + (v[i] - L'0');
                inNum = true;
            } else if (v[i] == L'.') {
                parts.push_back(inNum ? cur : 0);
                cur = 0; inNum = false;
            } else {
                break;
            }
        }
        if (inNum) parts.push_back(cur);
        return parts;
    };
    auto a = parse(lhs);
    auto b = parse(rhs);
    size_t n = std::max(a.size(), b.size());
    for (size_t i = 0; i < n; ++i) {
        int va = i < a.size() ? a[i] : 0;
        int vb = i < b.size() ? b[i] : 0;
        if (va != vb) return va > vb;
    }
    return false;
}

/// "1.3.3" -> 10303. Stored as a window property so a newly launched (newer) build can tell
/// which version is already running. Builds up to 1.3.2 never set it, which reads back as 0.
int versionToNumber(const std::wstring& v) {
    int parts[3] = { 0, 0, 0 };
    int idx = 0;
    for (wchar_t c : v) {
        if (c >= L'0' && c <= L'9') parts[idx] = parts[idx] * 10 + (c - L'0');
        else if (c == L'.') { if (++idx >= 3) break; }
    }
    return parts[0] * 10000 + parts[1] * 100 + parts[2];
}

std::wstring currentExePath() {
    wchar_t buf[MAX_PATH * 2] = {};
    DWORD len = GetModuleFileNameW(nullptr, buf, (DWORD)(sizeof(buf) / sizeof(buf[0]) - 1));
    return std::wstring(buf, len);
}

std::string extractVersionFromHtml(const std::string& html) {
    auto parseVerAt = [&](size_t pos) -> std::string {
        if (pos >= html.size() || (html[pos] != 'v' && html[pos] != 'V')) return "";
        size_t i = pos + 1;
        int dots = 0;
        std::string ver = "v";
        while (i < html.size()) {
            char c = html[i];
            if (c >= '0' && c <= '9') {
                ver.push_back(c);
            } else if (c == '.') {
                dots++;
                ver.push_back(c);
            } else {
                break;
            }
            i++;
        }
        return (dots >= 2 && ver.size() > 5) ? ver : "";
    };

    const char* marker = "\xE5\xB7\xB2\xE5\x8F\x91\xE5\xB8\x83"; // "已发布" in UTF-8
    size_t mPos = html.find(marker);
    if (mPos != std::string::npos) {
        size_t searchStart = (mPos > 32) ? (mPos - 32) : 0;
        for (size_t i = mPos; i > searchStart; --i) {
            if (html[i - 1] == 'v' || html[i - 1] == 'V') {
                std::string v = parseVerAt(i - 1);
                if (!v.empty()) return v;
            }
        }
    }

    const char* verPrefix = "\xE7\x89\x88\xE6\x9C\xAC "; // "版本 " in UTF-8
    size_t vPos = html.find(verPrefix);
    if (vPos != std::string::npos) {
        std::string v = parseVerAt(vPos + 7);
        if (!v.empty()) return v;
    }
    return "";
}

std::string fetchWorkBuddyPath(HINTERNET hConnect, LPCWSTR path) {
    std::string body;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (hRequest) {
        if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
            WinHttpReceiveResponse(hRequest, nullptr)) {
            DWORD statusCode = 0, statusSize = sizeof(statusCode);
            WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);
            if (statusCode >= 200 && statusCode < 300) {
                DWORD avail = 0;
                while (WinHttpQueryDataAvailable(hRequest, &avail) && avail > 0) {
                    std::vector<char> buf(avail);
                    DWORD readBytes = 0;
                    if (WinHttpReadData(hRequest, buf.data(), avail, &readBytes) && readBytes > 0) {
                        body.append(buf.data(), readBytes);
                    } else {
                        break;
                    }
                }
            }
        }
        WinHttpCloseHandle(hRequest);
    }
    return body;
}

HINTERNET openWinHttpSession(const std::wstring& ua) {
    // Windows 8.1+ supports AUTOMATIC_PROXY; Windows 7 requires DEFAULT_PROXY.
    HINTERNET hSession = WinHttpOpen(ua.c_str(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        hSession = WinHttpOpen(ua.c_str(), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    }
    if (hSession) {
        // Enable TLS 1.2 explicitly for Windows 7 SP1 (KB3140245) compatibility
        DWORD protos = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_1 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1;
        WinHttpSetOption(hSession, WINHTTP_OPTION_SECURE_PROTOCOLS, &protos, sizeof(protos));
    }
    return hSession;
}

void enableBestDpiAwareness() {
    // 1. Windows 10 1703+: Per-Monitor V2 DPI Awareness (dynamically loaded so Win7/8 won't fail to launch)
    if (HMODULE hUser32 = GetModuleHandleW(L"user32.dll")) {
        using SetDpiCtxFn = BOOL(WINAPI*)(HANDLE);
        auto fn = reinterpret_cast<SetDpiCtxFn>(GetProcAddress(hUser32, "SetProcessDpiAwarenessContext"));
        if (fn && fn(reinterpret_cast<HANDLE>(static_cast<INT_PTR>(-4)))) {
            return;
        }
    }
    // 2. Windows 8.1+: Per-Monitor DPI Awareness via shcore.dll
    if (HMODULE hShcore = LoadLibraryW(L"shcore.dll")) {
        using SetDpiAwareFn = HRESULT(WINAPI*)(int);
        auto fn = reinterpret_cast<SetDpiAwareFn>(GetProcAddress(hShcore, "SetProcessDpiAwareness"));
        bool ok = (fn && SUCCEEDED(fn(2)));
        FreeLibrary(hShcore);
        if (ok) return;
    }
    // 3. Windows Vista / 7: System DPI Awareness
    if (HMODULE hUser32 = GetModuleHandleW(L"user32.dll")) {
        using SetSysDpiFn = BOOL(WINAPI*)();
        auto fn = reinterpret_cast<SetSysDpiFn>(GetProcAddress(hUser32, "SetProcessDPIAware"));
        if (fn) fn();
    }
}

struct UpdateCheckThreadParams {
    HWND notifyHwnd;
    bool manual;
    int delayMs;
};

DWORD WINAPI updateCheckThreadProc(LPVOID param) {
    auto* p = static_cast<UpdateCheckThreadParams*>(param);
    HWND notifyHwnd = p->notifyHwnd;
    bool manual = p->manual;
    int delayMs = p->delayMs;
    delete p;

    if (delayMs > 0) Sleep((DWORD)delayMs);
    auto* res = new UpdateCheckResult();
    res->manual = manual;

    std::wstring ua = std::wstring(L"KuroCat/") + APP_VERSION;
    HINTERNET hSession = openWinHttpSession(ua);
    if (hSession) {
        WinHttpSetTimeouts(hSession, 8000, 8000, 8000, 8000);
        HINTERNET hConnect = WinHttpConnect(hSession, L"kurocat-download.app.workbuddy.host", INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (hConnect) {
            std::string body = fetchWorkBuddyPath(hConnect, L"/version.json?sharecode=JVAABIVZ7L");
            std::string tag = extractJsonStringField(body, "tag_name");
            std::string url = extractJsonStringField(body, "html_url");

            if (tag.empty()) {
                std::string html = fetchWorkBuddyPath(hConnect, L"/?sharecode=JVAABIVZ7L");
                tag = extractVersionFromHtml(html);
            }

            if (!tag.empty()) {
                res->ok = true;
                res->latestTag = utf8ToWide(tag);
                res->releaseUrl = url.empty() ? UPDATE_PAGE_URL : utf8ToWide(url);
                res->hasUpdate = isSemVerNewer(res->latestTag, APP_VERSION);
            }
            WinHttpCloseHandle(hConnect);
        }
        WinHttpCloseHandle(hSession);
    }

    if (!PostMessageW(notifyHwnd, WM_UPDATE_CHECK_DONE, 0, reinterpret_cast<LPARAM>(res))) {
        delete res;
    }
    return 0;
}

void startAsyncUpdateCheck(HWND notifyHwnd, bool manual, int delayMs = 0) {
    auto* params = new UpdateCheckThreadParams{ notifyHwnd, manual, delayMs };
    HANDLE hThread = CreateThread(nullptr, 0, updateCheckThreadProc, params, 0, nullptr);
    if (hThread) {
        CloseHandle(hThread);
    } else {
        delete params;
    }
}

struct UpdateDownloadResult {
    bool ok = false;
    std::wstring newExePath;
    std::wstring targetExePath;
};

bool launchProcessArgs(const std::wstring& exePath, const std::wstring& args) {
    std::wstring cmdLine = L"\"" + exePath + L"\"";
    if (!args.empty()) {
        cmdLine += L" ";
        cmdLine += args;
    }
    std::vector<wchar_t> cmdBuf(cmdLine.begin(), cmdLine.end());
    cmdBuf.push_back(L'\0');
    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};
    BOOL ok = CreateProcessW(
        exePath.c_str(),
        cmdBuf.data(),
        nullptr, nullptr, FALSE,
        0, nullptr, nullptr,
        &si, &pi
    );
    if (ok) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return true;
    }
    return false;
}

DWORD WINAPI updateDownloadThreadProc(LPVOID param) {
    HWND notifyHwnd = reinterpret_cast<HWND>(param);
    auto* res = new UpdateDownloadResult();
    std::string exeBytes;

    std::wstring ua = std::wstring(L"KuroCat/") + APP_VERSION;
    HINTERNET hSession = openWinHttpSession(ua);
    if (hSession) {
        WinHttpSetTimeouts(hSession, 10000, 10000, 30000, 60000);
        HINTERNET hConnect = WinHttpConnect(hSession, L"kurocat-download.app.workbuddy.host", INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (hConnect) {
            exeBytes = fetchWorkBuddyPath(hConnect, UPDATE_WIN_EXE_PATH);
            WinHttpCloseHandle(hConnect);
        }
        WinHttpCloseHandle(hSession);
    }

    if (exeBytes.size() > 50000 && exeBytes[0] == 'M' && exeBytes[1] == 'Z') {
        wchar_t curExe[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, curExe, MAX_PATH);
        wchar_t tempDir[MAX_PATH] = {};
        GetTempPathW(MAX_PATH, tempDir);

        std::wstring curExeStr(curExe);
        std::wstring newExePath = (curExeStr.size() > 4 && _wcsicmp(curExeStr.c_str() + curExeStr.size() - 4, L".exe") == 0)
            ? curExeStr.substr(0, curExeStr.size() - 4) + L".update.exe"
            : curExeStr + L".update.exe";
        HANDLE hNew = CreateFileW(newExePath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hNew == INVALID_HANDLE_VALUE) {
            newExePath = std::wstring(tempDir) + L"KuroCat_update.exe";
            hNew = CreateFileW(newExePath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        }
        if (hNew != INVALID_HANDLE_VALUE) {
            DWORD written = 0;
            BOOL okWrite = WriteFile(hNew, exeBytes.data(), (DWORD)exeBytes.size(), &written, nullptr);
            CloseHandle(hNew);
            if (okWrite && written == (DWORD)exeBytes.size()) {
                res->ok = true;
                res->newExePath = newExePath;
                res->targetExePath = curExeStr;
            }
        }
    }

    if (!PostMessageW(notifyHwnd, WM_UPDATE_DOWNLOAD_DONE, 0, reinterpret_cast<LPARAM>(res))) {
        delete res;
    }
    return 0;
}

void startInAppUpdate(AppState& app) {
    if (app.isUpdating || !app.menuOwnerHwnd) return;
    app.isUpdating = true;
    HANDLE hThread = CreateThread(nullptr, 0, updateDownloadThreadProc, app.menuOwnerHwnd, 0, nullptr);
    if (hThread) {
        CloseHandle(hThread);
    } else {
        app.isUpdating = false;
    }
}

void registerTrayIcon(AppState& app) {
    if (!app.trayIcon) app.trayIcon = createCatTrayIcon();
    app.nid = {};
    app.nid.cbSize = sizeof(app.nid);
    app.nid.hWnd = app.menuOwnerHwnd ? app.menuOwnerHwnd : app.hwnd;
    app.nid.uID = 1;
    app.nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    app.nid.uCallbackMessage = WM_TRAYICON;
    app.nid.hIcon = app.trayIcon;
    wcscpy_s(app.nid.szTip, L"KuroCat \u9ED1\u732B\u952E\u76D8\u684C\u5BA0 (\u53F3\u952E/\u5DE6\u952E\u6253\u5F00\u83DC\u5355)");
    Shell_NotifyIconW(NIM_ADD, &app.nid);
}

HICON createCatTrayIcon() {
    using namespace Gdiplus;
    constexpr int sz = 32;
    Bitmap bmp(sz, sz, PixelFormat32bppPARGB);
    Graphics g(&bmp);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.Clear(Color(0, 0, 0, 0));

    SolidBrush black(Color(255, 22, 22, 26));
    PointF lEar[3] = { {4, 15}, {6, 3}, {14, 11} };
    PointF rEar[3] = { {28, 15}, {26, 3}, {18, 11} };
    g.FillPolygon(&black, lEar, 3);
    g.FillPolygon(&black, rEar, 3);
    g.FillEllipse(&black, 4.0f, 8.0f, 24.0f, 21.0f);

    SolidBrush lime(Color(255, 201, 242, 64));
    g.FillEllipse(&lime, 8.5f, 14.5f, 6.0f, 5.2f);
    g.FillEllipse(&lime, 17.5f, 14.5f, 6.0f, 5.2f);
    g.FillEllipse(&black, 10.7f, 15.2f, 1.8f, 3.8f);
    g.FillEllipse(&black, 19.7f, 15.2f, 1.8f, 3.8f);

    HICON hIcon = nullptr;
    bmp.GetHICON(&hIcon);
    return hIcon;
}

void computeDefaultOrigin(int w, int h, int& outX, int& outY) {
    RECT work = {};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    outX = work.right - w - 24;
    outY = work.bottom - h - 12;
}

void applyWindowScaleAndPosition(AppState& app, bool anchorBottomCenter, bool persist = true) {
    int newW = (int)std::round(CatPhysics::CanvasWidth * app.settings.scale);
    int newH = (int)std::round(CatPhysics::CanvasHeight * app.settings.scale);

    int x = app.settings.posX;
    int y = app.settings.posY;
    if (anchorBottomCenter && app.hwnd) {
        RECT r = {};
        GetWindowRect(app.hwnd, &r);
        int oldMidX = (r.left + r.right) / 2;
        int oldBotY = r.bottom;
        x = oldMidX - newW / 2;
        y = oldBotY - newH;
    } else if (x == -99999 || y == -99999) {
        computeDefaultOrigin(newW, newH, x, y);
    }

    // Clamp onto virtual desktop so saved off-monitor coords never get lost
    int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if (x + newW < vx + 40 || x > vx + vw - 40 || y + newH < vy + 40 || y > vy + vh - 40) {
        computeDefaultOrigin(newW, newH, x, y);
    }

    HWND insertAfter = app.settings.alwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST;
    SetWindowPos(app.hwnd, insertAfter, x, y, newW, newH, SWP_NOACTIVATE);
    if (persist) {
        app.settings.posX = x;
        app.settings.posY = y;
        app.settings.saveToRegistry();
    }
}

/// Puts the cat back on top of other topmost windows (borderless/fullscreen games that make
/// themselves topmost after launching used to bury the cat, which looked like it "vanished").
void assertTopmost(AppState& app) {
    if (!app.hwnd || !app.isVisible || app.hiddenByFullscreen || !app.settings.alwaysOnTop) return;
    if (app.settings.obsCaptureMode > 0 && app.settings.obsHideDesktop) return;
    if (app.dressingRoom && IsWindowVisible(app.dressingRoom)) return;
    SetWindowPos(app.hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOSENDCHANGING);
}

/// Full self-repair after Win+L unlock, sleep/resume, display-mode changes (games switching
/// resolution) or a rendering stall: clears stuck keys, re-registers Raw Input, rebuilds the
/// layered-window buffer, restarts the frame timer and re-places the window on a visible monitor
/// WITHOUT overwriting the user's saved position.
void applyOBSMode(AppState& app);

void recoverAfterSystemEvent(AppState& app) {
    if (!app.hwnd) return;
    double now = nowSeconds();
    app.input.resetAllState();
    app.input.registerRawInput(app.hwnd);
    app.input.installKeyboardHook();
    if (app.settings.obsCaptureMode > 0 && app.settings.obsHideDesktop) {
        applyOBSMode(app);
    }
    app.input.lastMouseMoveTime = now; // wake the cat up so it visibly greets the user again
    app.destroyBuffer();
    app.ulwFailures = 0;
    app.isDragging = false;
    app.lastTickTime = now;
    app.nextFrameTime = 0.0;
    app.lastRenderTime = now;
    KillTimer(app.hwnd, TIMER_RENDER);
    SetTimer(app.hwnd, TIMER_RENDER, 16, nullptr);
    if (app.isVisible && !app.hiddenByFullscreen) ShowWindow(app.hwnd, SW_SHOWNOACTIVATE);
    applyWindowScaleAndPosition(app, false, false);
    app.pendingTopmostTime = now + 1.5; // some games re-assert topmost shortly after a mode switch
}

/// Twice per second: watchdog, fullscreen auto-hide, topmost re-assert, and UIPI detection.
void housekeeping(AppState& app) {
    double now = nowSeconds();
    if (now < app.nextHousekeepingTime) return;
    app.nextHousekeepingTime = now + 0.5;
    if (!app.hwnd) return;

    bool fullscreenFg = isForegroundFullscreen(app.hwnd);
    bool obsBg = (app.settings.obsCaptureMode > 0 && app.settings.obsHideDesktop);

    // Fullscreen auto-hide (PPT slideshow, fullscreen video, games)
    if (app.isVisible && app.settings.autoHideFullscreen && !obsBg) {
        if (fullscreenFg) {
            if (!app.hiddenByFullscreen) {
                app.hiddenByFullscreen = true;
                ShowWindow(app.hwnd, SW_HIDE);
            }
        } else if (app.hiddenByFullscreen) {
            app.hiddenByFullscreen = false;
            ShowWindow(app.hwnd, SW_SHOWNOACTIVATE);
            app.lastTickTime = now;
            app.lastRenderTime = now;
            assertTopmost(app);
        }
    } else if (app.hiddenByFullscreen) {
        app.hiddenByFullscreen = false;
        if (app.isVisible) {
            ShowWindow(app.hwnd, SW_SHOWNOACTIVATE);
            app.lastTickTime = now;
            app.lastRenderTime = now;
            assertTopmost(app);
        }
    }

    const bool effectivelyVisible = app.isVisible && !app.hiddenByFullscreen;

    // Rendering stalled for >2s while visible (e.g. the layered window lost its surface): self-heal.
    if (effectivelyVisible && app.lastRenderTime > 0.0 && now - app.lastRenderTime > 2.0) {
        recoverAfterSystemEvent(app);
        return;
    }
    if (effectivelyVisible && !IsWindowVisible(app.hwnd)) ShowWindow(app.hwnd, SW_SHOWNOACTIVATE);

    if (effectivelyVisible && !obsBg && app.settings.alwaysOnTop) {
        if (app.pendingTopmostTime > 0.0 && now >= app.pendingTopmostTime) {
            app.pendingTopmostTime = 0.0;
            assertTopmost(app);
        } else if (fullscreenFg || hasVisibleWindowAbove(app.hwnd)) {
            assertTopmost(app);
        }
    }

    // Some fullscreen/DirectInput games reset or starve Raw Input when taking focus;
    // periodically re-register RIDEV_INPUTSINK while a fullscreen app is active and no raw keys arrive.
    if (fullscreenFg && app.input.secondsSinceRawKeyboard(now) > 3.0) {
        app.input.registerRawInput(app.hwnd);
        if (!app.input.kbHook_) app.input.installKeyboardHook();
    }

    // Detect admin-level apps (WPS Office/PPT, games, admin terminals) whose keystrokes Windows withholds (UIPI).
    app.foregroundBlocksKeys = false;
    if (!app.isElevated) {
        HWND fg = GetForegroundWindow();
        DWORD pid = 0;
        if (fg) GetWindowThreadProcessId(fg, &pid);
        if (pid && pid != GetCurrentProcessId() && isProcessElevatedOrProtected(pid)) {
            app.foregroundBlocksKeys = true;
            app.lastBlockedForegroundTime = now;
            if (app.blockedForegroundSince <= 0.0) app.blockedForegroundSince = now;
            if (!app.warnedElevatedForeground && (fullscreenFg || (now - app.blockedForegroundSince >= 1.0))) {
                app.warnedElevatedForeground = true;
                showTrayBalloon(app,
                    L"KuroCat 听不到当前窗口的键盘",
                    L"当前软件（如 WPS / 游戏）以管理员权限运行，Windows 会拦截它的键盘输入。右键小猫或托盘图标 →「以管理员身份重新启动」即可恢复。");
            }
        } else {
            app.blockedForegroundSince = 0.0;
        }
    }
}

void quitApp(AppState& app) {
    app.input.uninstallKeyboardHook();
    // Destroying the overlay runs the normal WM_DESTROY cleanup (tray icon, hotkey...) and quits.
    if (app.hwnd && IsWindow(app.hwnd)) DestroyWindow(app.hwnd);
    else PostQuitMessage(0);
}

void setClickThrough(HWND hwnd, bool transparent) {
    LONG_PTR ex = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    bool isTrans = (ex & WS_EX_TRANSPARENT) != 0;
    if (isTrans != transparent) {
        if (transparent) ex |= WS_EX_TRANSPARENT;
        else ex &= ~WS_EX_TRANSPARENT;
        SetWindowLongPtrW(hwnd, GWL_EXSTYLE, ex);
    }
}

void applyOBSMode(AppState& app) {
    if (!app.hwnd) return;
    LONG_PTR ex = GetWindowLongPtrW(app.hwnd, GWL_EXSTYLE);
    bool wantToolWindow = (app.settings.obsCaptureMode == 0);
    bool isToolWindow = (ex & WS_EX_TOOLWINDOW) != 0;
    if (wantToolWindow != isToolWindow) {
        if (wantToolWindow) ex |= WS_EX_TOOLWINDOW;
        else ex &= ~WS_EX_TOOLWINDOW;
        bool wasVis = app.isVisible && !app.hiddenByFullscreen;
        if (wasVis) ShowWindow(app.hwnd, SW_HIDE);
        SetWindowLongPtrW(app.hwnd, GWL_EXSTYLE, ex);
        SetWindowPos(app.hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        if (wasVis) ShowWindow(app.hwnd, SW_SHOWNOACTIVATE);
    }
    bool hideDesk = (app.settings.obsCaptureMode > 0 && app.settings.obsHideDesktop);
    if (hideDesk) {
        // Move off-monitor & remove topmost so desktop stays clean while OBS WGC captures live frames!
        int offX = GetSystemMetrics(SM_XVIRTUALSCREEN) + GetSystemMetrics(SM_CXVIRTUALSCREEN) + 120;
        int offY = GetSystemMetrics(SM_YVIRTUALSCREEN) + 40;
        SetWindowPos(app.hwnd, HWND_BOTTOM, offX, offY, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
    } else {
        applyWindowScaleAndPosition(app, false);
    }
}

// MARK: - Skins (built-in coats + imported .kurocat files)

const SkinSpec& activeSkin(const AppState& app) {
    if (!app.settings.customSkin.empty())
        if (const SkinSpec* s = app.skins.find(app.settings.customSkin)) return *s;
    return builtInSkin(app.settings.coatTheme);
}

const KeyboardSkinSpec& builtInKeyboardSkin(KeyboardTheme theme) {
    static const KeyboardSkinSpec themes[4] = {
        KeyboardSkinSpec::builtInTheme(0),
        KeyboardSkinSpec::builtInTheme(1),
        KeyboardSkinSpec::builtInTheme(2),
        KeyboardSkinSpec::builtInTheme(3)
    };
    int i = std::clamp((int)theme, 0, 3);
    return themes[i];
}

const KeyboardSkinSpec& activeKeyboardSkin(const AppState& app) {
    if (!app.settings.customKeyboard.empty())
        if (const KeyboardSkinSpec* k = app.kbLibrary.find(app.settings.customKeyboard)) return *k;
    return builtInKeyboardSkin(app.settings.keyboardTheme);
}

void allowDropsWhenElevated(HWND hwnd) {
    // An elevated KuroCat would otherwise never see files dragged from the (non-elevated) Explorer.
    ChangeWindowMessageFilterEx(hwnd, WM_DROPFILES, MSGFLT_ALLOW, nullptr);
    ChangeWindowMessageFilterEx(hwnd, WM_COPYDATA, MSGFLT_ALLOW, nullptr);
    ChangeWindowMessageFilterEx(hwnd, 0x0049 /* WM_COPYGLOBALDATA */, MSGFLT_ALLOW, nullptr);
}

void setDressingStatus(AppState& app, const std::wstring& text, int kind) {
    app.dressingStatusText = text;
    app.dressingStatusKind = kind;
    if (app.dressingRoom) InvalidateRect(app.dressingRoom, nullptr, FALSE);
}

void refreshSettingsWindow(AppState& app);

void refreshDressingRoom(AppState& app) {
    if (app.dressingRoom) InvalidateRect(app.dressingRoom, nullptr, FALSE);
    refreshSettingsWindow(app);
}

// Imports one file; on success switches to it. Errors are shown in Chinese.
bool importSkinFile(AppState& app, const std::wstring& path) {
    std::string id, error;
    std::wstring base = path.substr(path.find_last_of(L"\\/") == std::wstring::npos ? 0 : path.find_last_of(L"\\/") + 1);
    if (!app.skins.importFile(path, id, error)) {
        // With 换装间 open the red status line explains it in place; a dialog is only for drops onto the cat.
        if (app.dressingRoom) {
            setDressingStatus(app, L"没能导入「" + base + L"」：" + utf8ToWide(error), 2);
        } else {
            std::wstring msg = L"「" + base + L"」：" + utf8ToWide(error);
            MessageBoxW(app.menuOwnerHwnd, msg.c_str(), L"无法导入这个皮肤", MB_OK | MB_ICONWARNING | MB_TOPMOST | MB_SETFOREGROUND);
        }
        return false;
    }
    app.settings.customSkin = id;
    app.settings.saveToRegistry();
    const SkinSpec* s = app.skins.find(id);
    std::wstring msg = L"✓ 已换上「" + (s ? utf8ToWide(s->name) : std::wstring(L"新皮肤")) + L"」";
    if (s && !s->author.empty()) msg += L"（作者：" + utf8ToWide(s->author) + L"）";
    setDressingStatus(app, msg, 1);
    if (!app.dressingRoom) showTrayBalloon(app, L"KuroCat 换装间", msg.c_str());
    return true;
}

bool importKeyboardSkinFile(AppState& app, const std::wstring& path) {
    std::string id, error;
    size_t slash = path.find_last_of(L"\\/");
    std::wstring base = path.substr(slash == std::wstring::npos ? 0 : slash + 1);
    if (!app.kbLibrary.importFile(path, id, error)) {
        if (app.dressingRoom) {
            setDressingStatus(app, L"没能导入键盘皮肤「" + base + L"」：" + utf8ToWide(error), 2);
        } else {
            std::wstring msg = L"「" + base + L"」：" + utf8ToWide(error);
            MessageBoxW(app.menuOwnerHwnd, msg.c_str(),
                        app.settings.isEnglish ? L"Failed to Import Keyboard Skin" : L"无法导入这个键盘皮肤",
                        MB_OK | MB_ICONWARNING | MB_TOPMOST | MB_SETFOREGROUND);
        }
        return false;
    }
    app.settings.customKeyboard = id;
    app.settings.saveToRegistry();
    const KeyboardSkinSpec* kb = app.kbLibrary.find(id);
    std::wstring msg = L"✓ 已换上键盘皮肤「" + (kb ? utf8ToWide(kb->name) : std::wstring(L"新键盘")) + L"」";
    if (kb && !kb->author.empty()) msg += L"（作者：" + utf8ToWide(kb->author) + L"）";
    setDressingStatus(app, msg, 1);
    if (!app.dressingRoom) showTrayBalloon(app, L"KuroCat 键盘皮肤", msg.c_str());
    return true;
}

bool hasExtensionIgnoreCase(const std::wstring& path, const wchar_t* targetExt) {
    size_t slash = path.find_last_of(L"\\/");
    size_t dot = path.find_last_of(L'.');
    if (dot == std::wstring::npos || (slash != std::wstring::npos && dot < slash)) return false;
    return _wcsicmp(path.c_str() + dot + 1, targetExt) == 0;
}

void importDroppedFiles(AppState& app, HDROP drop) {
    UINT count = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
    std::vector<std::wstring> files;
    for (UINT i = 0; i < count && i < 16; ++i) {
        UINT len = DragQueryFileW(drop, i, nullptr, 0);
        std::wstring f(len + 1, L'\0');
        DragQueryFileW(drop, i, &f[0], len + 1);
        f.resize(len);
        files.push_back(f);
    }
    DragFinish(drop);
    for (const auto& f : files) {
        if (KeyboardSkinLibrary::looksLikeKeyboardFile(f)) {
            importKeyboardSkinFile(app, f);
        } else if (hasExtensionIgnoreCase(f, L"kurocat")) {
            importSkinFile(app, f);
        }
    }
}

void chooseSkinFile(AppState& app) {
    // comdlg32 is loaded on demand so the build keeps its existing link libraries.
    typedef BOOL (WINAPI *GetOpenFileNameFn)(LPOPENFILENAMEW);
    HMODULE dlg = LoadLibraryW(L"comdlg32.dll");
    auto getOpen = dlg ? (GetOpenFileNameFn)(void*)GetProcAddress(dlg, "GetOpenFileNameW") : nullptr;
    if (!getOpen) {
        if (dlg) FreeLibrary(dlg);
        MessageBoxW(app.dressingRoom, L"无法打开文件选择框，请直接把 .kurocat 文件拖进来。", L"KuroCat 换装间", MB_OK | MB_ICONWARNING);
        return;
    }
    wchar_t file[MAX_PATH * 4] = {0};
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = app.dressingRoom;
    ofn.lpstrFilter = L"KuroCat 皮肤 (*.kurocat)\0*.kurocat\0所有文件 (*.*)\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = (DWORD)(sizeof(file) / sizeof(file[0]));
    ofn.lpstrTitle = L"选择 .kurocat 皮肤文件";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_EXPLORER;
    BOOL ok = getOpen(&ofn);
    FreeLibrary(dlg);
    if (ok) {
        if (KeyboardSkinLibrary::looksLikeKeyboardFile(file)) importKeyboardSkinFile(app, file);
        else importSkinFile(app, file);
    }
}

void chooseKeyboardSkinFile(AppState& app) {
    typedef BOOL (WINAPI *GetOpenFileNameFn)(LPOPENFILENAMEW);
    HMODULE dlg = LoadLibraryW(L"comdlg32.dll");
    auto getOpen = dlg ? (GetOpenFileNameFn)(void*)GetProcAddress(dlg, "GetOpenFileNameW") : nullptr;
    if (!getOpen) {
        if (dlg) FreeLibrary(dlg);
        MessageBoxW(app.menuOwnerHwnd,
                    app.settings.isEnglish ? L"Unable to open file dialog. Please drag a .kurokb or .json file onto the cat."
                                           : L"无法打开文件选择框，请直接把 .kurokb / .json 文件拖到小猫身上。",
                    L"KuroCat", MB_OK | MB_ICONWARNING | MB_TOPMOST);
        return;
    }
    wchar_t file[MAX_PATH * 4] = {0};
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = app.menuOwnerHwnd;
    ofn.lpstrFilter = L"KuroCat \u952E\u76D8\u76AE\u80A4 (*.kurokb;*.json)\0*.kurokb;*.json\0\u6240\u6709\u6587\u4EF6 (*.*)\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = (DWORD)(sizeof(file) / sizeof(file[0]));
    ofn.lpstrTitle = app.settings.isEnglish ? L"Choose Keyboard Skin (.kurokb / .json)" : L"\u9009\u62E9\u952E\u76D8\u76AE\u80A4\u6587\u4EF6 (.kurokb / .json)";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_EXPLORER;
    BOOL ok = getOpen(&ofn);
    FreeLibrary(dlg);
    if (ok) importKeyboardSkinFile(app, file);
}

// MARK: 「换装间」 window — GDI+ painted: live preview of the worn skin, dashed drop zone, pill button.
// Layout in 96-dpi design units, scaled by AppState::dressingScale.
namespace dressing {
constexpr float W = 444, H = 472;
const Gdiplus::RectF previewRect(22, 22, 150, 134);
const Gdiplus::RectF dropRect(22, 172, 400, 150);
const Gdiplus::RectF buttonRect(147, 338, 150, 40);

Gdiplus::FontFamily* family() {
    static Gdiplus::FontFamily* fam = nullptr;
    if (!fam) {
        for (const wchar_t* name : {L"Microsoft YaHei UI", L"Microsoft YaHei", L"Segoe UI"}) {
            auto* f = new Gdiplus::FontFamily(name);
            if (f->GetLastStatus() == Gdiplus::Ok) { fam = f; break; }
            delete f;
        }
        if (!fam) fam = Gdiplus::FontFamily::GenericSansSerif()->Clone();
    }
    return fam;
}
void roundRect(Gdiplus::GraphicsPath& p, const Gdiplus::RectF& r, float rad) {
    float d = rad * 2;
    p.AddArc(r.X, r.Y, d, d, 180, 90);
    p.AddArc(r.X + r.Width - d, r.Y, d, d, 270, 90);
    p.AddArc(r.X + r.Width - d, r.Y + r.Height - d, d, d, 0, 90);
    p.AddArc(r.X, r.Y + r.Height - d, d, d, 90, 90);
    p.CloseFigure();
}
void text(Gdiplus::Graphics& g, const std::wstring& s, float size, int style, Gdiplus::Color c, const Gdiplus::RectF& box,
          Gdiplus::StringAlignment align = Gdiplus::StringAlignmentNear, bool ellipsis = false) {
    Gdiplus::Font font(family(), size, style, Gdiplus::UnitPixel);
    Gdiplus::SolidBrush brush(c);
    Gdiplus::StringFormat fmt;
    fmt.SetAlignment(align);
    if (ellipsis) { fmt.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter); fmt.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap); }
    g.DrawString(s.c_str(), (INT)s.size(), &font, box, &fmt, &brush);
}
void paw(Gdiplus::Graphics& g, float cx, float cy, Gdiplus::Color c) {
    Gdiplus::SolidBrush b(c);
    g.FillEllipse(&b, cx - 9.5f, cy + 6 - 7.8f, 19.0f, 15.6f);
    const float toes[4][2] = {{-10.5f, -5.5f}, {-3.8f, -11.5f}, {3.8f, -11.5f}, {10.5f, -5.5f}};
    for (const auto& t : toes) g.FillEllipse(&b, cx + t[0] - 3.7f, cy + t[1] - 4.4f, 7.4f, 8.8f);
}

void paint(AppState& app, HWND hwnd, HDC hdc) {
    using namespace Gdiplus;
    RECT rc; GetClientRect(hwnd, &rc);
    int cw = std::max(1, (int)rc.right), ch = std::max(1, (int)rc.bottom);
    Bitmap back(cw, ch, PixelFormat32bppPARGB);
    Graphics g(&back);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
    g.Clear(Color(255, 251, 247, 240));
    g.ScaleTransform(app.dressingScale, app.dressingScale);

    // Live preview of the skin being worn
    GraphicsPath card; roundRect(card, previewRect, 18);
    SolidBrush cardBrush(Color(255, 243, 233, 220));
    g.FillPath(&cardBrush, &card);
    {
        GraphicsState st = g.Save();
        g.SetClip(&card, CombineModeIntersect);
        g.TranslateTransform(previewRect.X, previewRect.Y);
        g.ScaleTransform(previewRect.Width / 290.0f, previewRect.Height / 260.0f);
        g.TranslateTransform(-40.0f, -36.0f);
        static CatPhysics pose;   // a calm sitting pose
        pose.time = 0.7f;
        CatSettings settings = app.settings;
        settings.showKeyLegends = false;
        settings.isEditMode = false;
        settings.flipHorizontal = false;
        settings.rotationDeg = 0.0f;
        CatRenderer::draw(g, pose, settings, activeSkin(app), activeKeyboardSkin(app));
        g.Restore(st);
        g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
    }

    const SkinSpec& skin = activeSkin(app);
    const Color ink(255, 59, 47, 42), brown(255, 90, 74, 64), muted(255, 168, 153, 141), orange(255, 232, 150, 78);
    text(g, L"换装间", 21, FontStyleBold, ink, RectF(190, 22, 240, 30));
    text(g, L"正在穿", 11.5f, FontStyleRegular, muted, RectF(191, 58, 230, 18));
    text(g, utf8ToWide(skin.name), 15, FontStyleBold, brown, RectF(190, 74, 236, 22), StringAlignmentNear, true);
    if (!skin.author.empty()) text(g, L"作者 " + utf8ToWide(skin.author), 11.5f, FontStyleRegular, muted, RectF(191, 97, 232, 18), StringAlignmentNear, true);
    text(g, L"导入皮肤工坊做好的 .kurocat 文件，\n小猫会立刻换上新毛色。", 11.5f, FontStyleRegular, muted, RectF(191, 120, 236, 40));

    // Drop zone
    GraphicsPath zone; roundRect(zone, dropRect, 16);
    SolidBrush zoneBrush(Color(255, 255, 253, 249));
    g.FillPath(&zoneBrush, &zone);
    Pen dash(Color(255, 227, 195, 160), 2.0f);
    REAL pattern[2] = {3.5f, 2.5f};
    dash.SetDashPattern(pattern, 2);
    g.DrawPath(&dash, &zone);
    float cx = dropRect.X + dropRect.Width / 2, cy = dropRect.Y + dropRect.Height / 2;
    paw(g, cx, cy - 26, Color(230, 232, 150, 78));
    text(g, L"把 .kurocat 皮肤拖到这里", 15, FontStyleBold, brown, RectF(dropRect.X, cy + 2, dropRect.Width, 24), StringAlignmentCenter);
    text(g, L"也可以直接拖到桌面上的小猫身上 · 点这里选择文件", 11.5f, FontStyleRegular, Color(255, 160, 143, 130),
         RectF(dropRect.X, cy + 28, dropRect.Width, 20), StringAlignmentCenter);

    // Status + footer
    Color statusColor = app.dressingStatusKind == 1 ? Color(255, 62, 142, 90)
                      : app.dressingStatusKind == 2 ? Color(255, 194, 73, 59) : Color(255, 138, 123, 112);
    text(g, app.dressingStatusText, 12.5f, app.dressingStatusKind ? FontStyleBold : FontStyleRegular, statusColor,
         RectF(22, 390, 400, 42), StringAlignmentCenter);
    text(g, L"皮肤独立保存在 %APPDATA%\\KuroCat\\skins（软件更新会自动保留）  ·  右键小猫 → 猫咪皮肤 可切换或备份", 10.0f, FontStyleRegular,
         Color(255, 179, 166, 155), RectF(18, 434, 408, 34), StringAlignmentCenter);
    (void)orange;

    Graphics screen(hdc);
    screen.DrawImage(&back, 0, 0, cw, ch);
}

void paintButton(AppState& app, const DRAWITEMSTRUCT* di) {
    using namespace Gdiplus;
    int w = di->rcItem.right - di->rcItem.left, h = di->rcItem.bottom - di->rcItem.top;
    Bitmap back(std::max(1, w), std::max(1, h), PixelFormat32bppPARGB);
    Graphics g(&back);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
    g.Clear(Color(255, 251, 247, 240));
    float k = app.dressingScale;
    bool pressed = (di->itemState & ODS_SELECTED) != 0;
    GraphicsPath pill; roundRect(pill, RectF(1, 1, w - 2.0f, h - 2.0f), (h - 2.0f) / 2);
    SolidBrush fill(pressed ? Color(255, 215, 132, 62) : Color(255, 232, 150, 78));
    g.FillPath(&fill, &pill);
    Font font(family(), 14.5f * k, FontStyleBold, UnitPixel);
    SolidBrush white(Color(255, 255, 255, 255));
    StringFormat fmt; fmt.SetAlignment(StringAlignmentCenter); fmt.SetLineAlignment(StringAlignmentCenter);
    g.DrawString(L"选择文件…", -1, &font, RectF(0, 0, (REAL)w, (REAL)h), &fmt, &white);
    if (di->itemState & ODS_FOCUS) {
        Pen ring(Color(120, 232, 150, 78), 2.0f);
        g.DrawPath(&ring, &pill);
    }
    Graphics screen(di->hDC);
    screen.DrawImage(&back, di->rcItem.left, di->rcItem.top, w, h);
}

bool inDrop(AppState& app, LPARAM lParam) {
    float x = (short)LOWORD(lParam) / app.dressingScale, y = (short)HIWORD(lParam) / app.dressingScale;
    return x >= dropRect.X && x <= dropRect.X + dropRect.Width && y >= dropRect.Y && y <= dropRect.Y + dropRect.Height;
}
} // namespace dressing

LRESULT CALLBACK dressingRoomProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    AppState* app = g_app;
    switch (msg) {
        case WM_DROPFILES:
            if (app) importDroppedFiles(*app, (HDROP)wParam);
            else DragFinish((HDROP)wParam);
            return 0;
        case WM_COMMAND:
            if (LOWORD(wParam) == IDC_DRESSING_CHOOSE && app) chooseSkinFile(*app);
            return 0;
        case WM_DRAWITEM:
            if (app && wParam == IDC_DRESSING_CHOOSE) { dressing::paintButton(*app, (const DRAWITEMSTRUCT*)lParam); return TRUE; }
            break;
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            if (app) dressing::paint(*app, hwnd, hdc);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_SETCURSOR:
            if (app && LOWORD(lParam) == HTCLIENT) {
                POINT pt; GetCursorPos(&pt); ScreenToClient(hwnd, &pt);
                if (dressing::inDrop(*app, MAKELPARAM(pt.x, pt.y))) { SetCursor(LoadCursorW(nullptr, IDC_HAND)); return TRUE; }
            }
            break;
        case WM_LBUTTONUP:
            if (app && dressing::inDrop(*app, lParam)) chooseSkinFile(*app);
            return 0;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            if (app && app->dressingRoom == hwnd) app->dressingRoom = nullptr;
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void showDressingRoom(AppState& app) {
    if (app.dressingRoom) { refreshDressingRoom(app); ShowWindow(app.dressingRoom, SW_SHOWNORMAL); SetForegroundWindow(app.dressingRoom); return; }
    HINSTANCE inst = GetModuleHandleW(nullptr);
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = dressingRoomProc;
        wc.hInstance = inst;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;   // fully painted in WM_PAINT
        wc.lpszClassName = DRESSING_ROOM_CLASS;
        registered = RegisterClassExW(&wc) != 0;
    }
    HDC screen = GetDC(nullptr);
    app.dressingScale = GetDeviceCaps(screen, LOGPIXELSY) / 96.0f;
    ReleaseDC(nullptr, screen);
    float k = app.dressingScale;
    auto px = [&](float v) { return (int)std::lround(v * k); };
    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, exStyle = WS_EX_TOPMOST | WS_EX_ACCEPTFILES;
    RECT rc = {0, 0, px(dressing::W), px(dressing::H)};
    AdjustWindowRectEx(&rc, style, FALSE, exStyle);
    int w = rc.right - rc.left, h = rc.bottom - rc.top;
    RECT work = {};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    HWND win = CreateWindowExW(exStyle, DRESSING_ROOM_CLASS, L"换装间", style,
        (work.left + work.right - w) / 2, (work.top + work.bottom - h) / 2, w, h, nullptr, nullptr, inst, nullptr);
    if (!win) return;
    app.dressingRoom = win;
    allowDropsWhenElevated(win);
    DragAcceptFiles(win, TRUE);
    const Gdiplus::RectF& b = dressing::buttonRect;
    CreateWindowExW(0, L"BUTTON", L"选择文件…", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                    px(b.X), px(b.Y), px(b.Width), px(b.Height), win, (HMENU)(INT_PTR)IDC_DRESSING_CHOOSE, inst, nullptr);
    setDressingStatus(app, L"支持拖入多个文件；坏掉的文件会告诉你哪里不对。", 0);
    ShowWindow(win, SW_SHOWNORMAL);
    SetForegroundWindow(win);
}

void confirmDeleteSkin(AppState& app, const std::string& id) {
    const SkinSpec* s = app.skins.find(id);
    if (!s) return;
    std::wstring q = L"删除皮肤「" + utf8ToWide(s->name) + L"」？\n\n会从皮肤文件夹里删除这个文件，删除后需要重新导入。";
    if (MessageBoxW(app.menuOwnerHwnd, q.c_str(), L"KuroCat", MB_YESNO | MB_ICONQUESTION | MB_TOPMOST | MB_SETFOREGROUND | MB_DEFBUTTON2) != IDYES) return;
    std::string removing = id;
    if (!app.skins.remove(removing)) {
        MessageBoxW(app.menuOwnerHwnd, (L"无法删除 " + SkinLibrary::dir() + L"\\" + utf8ToWide(removing) + L".kurocat").c_str(),
                    L"删除失败", MB_OK | MB_ICONWARNING | MB_TOPMOST);
        return;
    }
    if (app.settings.customSkin == removing) { app.settings.customSkin.clear(); app.settings.saveToRegistry(); }
    refreshDressingRoom(app);
}

void confirmDeleteKeyboardSkin(AppState& app, const std::string& id) {
    const KeyboardSkinSpec* kb = app.kbLibrary.find(id);
    if (!kb) return;
    std::wstring q = L"\u5220\u9664\u952E\u76D8\u76AE\u80A4\u300C" + utf8ToWide(kb->name) + L"\u300D\uFF1F\n\n\u4F1A\u4ECE\u952E\u76D8\u76AE\u80A4\u6587\u4EF6\u5939\u91CC\u5220\u9664\u8FD9\u4E2A\u6587\u4EF6\uFF0C\u5220\u9664\u540E\u9700\u8981\u91CD\u65B0\u5BFC\u5165\u3002";
    if (MessageBoxW(app.menuOwnerHwnd, q.c_str(), L"KuroCat", MB_YESNO | MB_ICONQUESTION | MB_TOPMOST | MB_SETFOREGROUND | MB_DEFBUTTON2) != IDYES) return;
    std::string removing = id;
    if (!app.kbLibrary.remove(removing)) {
        MessageBoxW(app.menuOwnerHwnd, (L"\u65E0\u6CD5\u5220\u9664 " + KeyboardSkinLibrary::directory() + L"\\" + utf8ToWide(removing) + L".kurokb").c_str(),
                    L"\u5220\u9664\u5931\u8D25", MB_OK | MB_ICONWARNING | MB_TOPMOST);
        return;
    }
    if (app.settings.customKeyboard == removing) { app.settings.customKeyboard.clear(); app.settings.saveToRegistry(); }
    refreshDressingRoom(app);
}

namespace settings_ui {
enum CtrlId : int {
    ID_COAT_COMBO = 2001,
    ID_BTN_DRESSING,
    ID_EYE_COMBO,
    ID_KB_COMBO,
    ID_BTN_IMPORT_KB,
    ID_SCALE_SCROLL,
    ID_SCALE_VAL,
    ID_ROT_SCROLL,
    ID_ROT_VAL,
    ID_BTN_ROT_ZERO,
    ID_CHK_FLIP,
    ID_CHK_OUTLINE,
    ID_BTN_RESET_POS,
    ID_CHK_ANGER,
    ID_CHK_PET,
    ID_CHK_BADGES,
    ID_CHK_LEGENDS,
    ID_CHK_MARQUEE,
    ID_CHK_EASTER,
    ID_TABLET_COMBO,
    ID_CHK_TOPMOST,
    ID_CHK_AUTOHIDE_FS,
    ID_CHK_CLICKTHROUGH,
    ID_CHK_AUTOSTART,
    ID_CHK_RUN_ADMIN,
    ID_BTN_RELAUNCH_ADMIN,
    ID_OBS_COMBO,
    ID_CHK_OBS_HIDE,
    ID_LANG_COMBO,
    ID_BTN_SKINS_DIR,
    ID_BTN_KB_DIR,
    ID_BTN_EDIT_MODE
};

bool isWindowsDarkMode() {
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
                      L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                      0, KEY_READ, &hKey) != ERROR_SUCCESS) return false;
    DWORD val = 1, sz = sizeof(val);
    bool dark = (RegQueryValueExW(hKey, L"AppsUseLightTheme", nullptr, nullptr, (LPBYTE)&val, &sz) == ERROR_SUCCESS && val == 0);
    RegCloseKey(hKey);
    return dark;
}

void applyDwmDarkTitleBar(HWND hwnd, bool dark) {
    HMODULE dwm = LoadLibraryW(L"dwmapi.dll");
    if (!dwm) return;
    using Fn = HRESULT (WINAPI*)(HWND, DWORD, LPCVOID, DWORD);
    auto fn = (Fn)GetProcAddress(dwm, "DwmSetWindowAttribute");
    if (fn) {
        BOOL v = dark ? TRUE : FALSE;
        // 20 = DWMWA_USE_IMMERSIVE_DARK_MODE (Win10 2004+ / Win11); 19 = older Win10 1903/1909
        if (FAILED(fn(hwnd, 20, &v, sizeof(v)))) fn(hwnd, 19, &v, sizeof(v));
    }
    FreeLibrary(dwm);
}

void rebuildCombosAndSync(AppState& app) {
    HWND hwnd = app.settingsWnd;
    if (!hwnd) return;
    app.settingsUpdating = true;
    bool en = app.settings.isEnglish;
    auto tr = [&](const wchar_t* zh, const wchar_t* eng) -> const wchar_t* { return en ? eng : zh; };

    // Coat / Skin combo (7 built-in + imported)
    HWND coatCb = GetDlgItem(hwnd, ID_COAT_COMBO);
    if (coatCb) {
        SendMessageW(coatCb, CB_RESETCONTENT, 0, 0);
        const wchar_t* builtins[7] = {
            tr(L"\u7ECF\u5178\u5C0F\u9ED1\u732B", L"Classic Black Cat"),
            tr(L"\u6316\u7164\u66B9\u7F57\u732B", L"Siamese Cat"),
            tr(L"\u7F8E\u77ED\u94F6\u864E\u6591", L"Silver Tabby Shorthair"),
            tr(L"\u6A58\u767D\u732B", L"Orange & White Cat"),
            tr(L"\u5446\u732B", L"Dazed Cat"),
            tr(L"\u72F8\u82B1\u732B", L"Tabby Cat"),
            tr(L"\u82F1\u77ED\u84DD\u732B\uFF08\u7070\u6BDB\u6A58\u9EC4\u77B3\uFF09", L"British Shorthair (Gray)")
        };
        for (int i = 0; i < 7; ++i) SendMessageW(coatCb, CB_ADDSTRING, 0, (LPARAM)builtins[i]);
        int sel = (int)app.settings.coatTheme;
        for (size_t i = 0; i < app.skins.skins.size(); ++i) {
            SendMessageW(coatCb, CB_ADDSTRING, 0, (LPARAM)utf8ToWide(app.skins.skins[i].name).c_str());
            if (!app.settings.customSkin.empty() && app.skins.skins[i].id == app.settings.customSkin) {
                sel = 7 + (int)i;
            }
        }
        SendMessageW(coatCb, CB_SETCURSEL, (WPARAM)sel, 0);
    }

    // Eye combo
    HWND eyeCb = GetDlgItem(hwnd, ID_EYE_COMBO);
    if (eyeCb) {
        SendMessageW(eyeCb, CB_RESETCONTENT, 0, 0);
        const wchar_t* eyes[7] = {
            tr(L"\u8367\u5149\u9EC4\u7EFF", L"Neon Chartreuse"),
            tr(L"\u66B9\u7F57\u5B9D\u77F3\u84DD", L"Siamese Ice Blue"),
            tr(L"\u7425\u73C0\u91D1\u77B3", L"Warm Amber"),
            tr(L"\u51B0\u6676\u6D45\u84DD", L"Ice Cyan"),
            tr(L"\u67D4\u548C\u7425\u73C0", L"Soft Amber"),
            tr(L"\u6DE1\u91D1\u9752\u7EFF", L"Golden Hazel"),
            tr(L"\u6A44\u6984\u91D1", L"Olive Gold")
        };
        for (int i = 0; i < 7; ++i) SendMessageW(eyeCb, CB_ADDSTRING, 0, (LPARAM)eyes[i]);
        SendMessageW(eyeCb, CB_SETCURSEL, (WPARAM)(int)app.settings.eyeTheme, 0);
    }

    // Keyboard combo (4 built-in + imported)
    app.kbLibrary.reload();
    HWND kbCb = GetDlgItem(hwnd, ID_KB_COMBO);
    if (kbCb) {
        SendMessageW(kbCb, CB_RESETCONTENT, 0, 0);
        const wchar_t* kbs[4] = {
            tr(L"\u7ECF\u5178\u5976\u767D\u6696\u9EC4", L"Classic Milk White"),
            tr(L"\u6697\u591C\u6781\u5BA2\u9ED1", L"Midnight Hacker Black"),
            tr(L"\u590D\u53E4\u5976\u6CB9\u7070", L"Retro Cream Gray"),
            tr(L"\u6DF1\u7070\u7D20\u8272", L"Graphite Gray")
        };
        for (int i = 0; i < 4; ++i) SendMessageW(kbCb, CB_ADDSTRING, 0, (LPARAM)kbs[i]);
        int sel = (int)app.settings.keyboardTheme;
        for (size_t i = 0; i < app.kbLibrary.skins.size(); ++i) {
            SendMessageW(kbCb, CB_ADDSTRING, 0, (LPARAM)utf8ToWide(app.kbLibrary.skins[i].name).c_str());
            if (!app.settings.customKeyboard.empty() && app.kbLibrary.skins[i].id == app.settings.customKeyboard) {
                sel = 4 + (int)i;
            }
        }
        SendMessageW(kbCb, CB_SETCURSEL, (WPARAM)sel, 0);
    }

    // Scale & Rotation scrollbars
    int scalePct = std::clamp((int)std::lround(app.settings.scale * 100.0f), 45, 250);
    SetScrollPos(GetDlgItem(hwnd, ID_SCALE_SCROLL), SB_CTL, scalePct, TRUE);
    wchar_t sBuf[32] = {};
    swprintf_s(sBuf, L"%d%%", scalePct);
    SetWindowTextW(GetDlgItem(hwnd, ID_SCALE_VAL), sBuf);

    int rotDeg = std::clamp((int)std::lround(app.settings.rotationDeg), -180, 180);
    SetScrollPos(GetDlgItem(hwnd, ID_ROT_SCROLL), SB_CTL, rotDeg, TRUE);
    wchar_t rBuf[32] = {};
    swprintf_s(rBuf, L"%d\u00B0", rotDeg);
    SetWindowTextW(GetDlgItem(hwnd, ID_ROT_VAL), rBuf);

    auto setChk = [&](int id, bool v) {
        SendMessageW(GetDlgItem(hwnd, id), BM_SETCHECK, v ? BST_CHECKED : BST_UNCHECKED, 0);
    };
    setChk(ID_CHK_FLIP, app.settings.flipHorizontal);
    setChk(ID_CHK_OUTLINE, app.settings.whiteOutline);
    setChk(ID_CHK_ANGER, app.settings.enableAnger);
    setChk(ID_CHK_PET, app.settings.petInteraction);
    setChk(ID_CHK_BADGES, app.settings.showKeyBadges);
    setChk(ID_CHK_LEGENDS, app.settings.showKeyLegends);
    setChk(ID_CHK_MARQUEE, app.settings.keyboardMarquee);
    setChk(ID_CHK_EASTER, app.settings.footEasterEgg || app.settings.tailEasterEgg);
    setChk(ID_CHK_TOPMOST, app.settings.alwaysOnTop);
    setChk(ID_CHK_AUTOHIDE_FS, app.settings.autoHideFullscreen);
    setChk(ID_CHK_CLICKTHROUGH, app.settings.fullClickThrough);
    setChk(ID_CHK_AUTOSTART, isAutoStartEnabled());
    setChk(ID_CHK_RUN_ADMIN, app.settings.runAsAdmin);
    setChk(ID_CHK_OBS_HIDE, app.settings.obsHideDesktop);
    EnableWindow(GetDlgItem(hwnd, ID_CHK_OBS_HIDE), app.settings.obsCaptureMode != 0);
    EnableWindow(GetDlgItem(hwnd, ID_BTN_RELAUNCH_ADMIN), !app.isElevated);

    SendMessageW(GetDlgItem(hwnd, ID_TABLET_COMBO), CB_SETCURSEL, (WPARAM)app.settings.tabletMode, 0);
    SendMessageW(GetDlgItem(hwnd, ID_OBS_COMBO), CB_SETCURSEL, (WPARAM)app.settings.obsCaptureMode, 0);
    SendMessageW(GetDlgItem(hwnd, ID_LANG_COMBO), CB_SETCURSEL, (WPARAM)(app.settings.isEnglish ? 1 : 0), 0);

    app.settingsUpdating = false;
}
} // namespace settings_ui

void showSettingsWindow(AppState& app);

void refreshSettingsWindow(AppState& app) {
    if (app.settingsWnd && IsWindow(app.settingsWnd)) {
        settings_ui::rebuildCombosAndSync(app);
    }
}

LRESULT CALLBACK settingsWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    AppState* app = g_app;
    switch (msg) {
        case WM_ERASEBKGND:
            if (app && app->settingsBgBrush) {
                RECT rc = {};
                GetClientRect(hwnd, &rc);
                FillRect((HDC)wParam, &rc, app->settingsBgBrush);
                return TRUE;
            }
            break;
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN:
            if (app && app->settingsBgBrush) {
                HDC hdc = (HDC)wParam;
                SetBkMode(hdc, TRANSPARENT);
                SetTextColor(hdc, app->settingsDark ? RGB(236, 238, 242) : RGB(28, 30, 34));
                return (LRESULT)app->settingsBgBrush;
            }
            break;
        case WM_HSCROLL: {
            if (!app || app->settingsUpdating) return 0;
            HWND sb = (HWND)lParam;
            int id = GetDlgCtrlID(sb);
            int cur = GetScrollPos(sb, SB_CTL);
            int lo = 0, hi = 100;
            GetScrollRange(sb, SB_CTL, &lo, &hi);
            int stepSmall = (id == settings_ui::ID_ROT_SCROLL) ? 1 : 2;
            int stepPage = (id == settings_ui::ID_ROT_SCROLL) ? 15 : 10;
            switch (LOWORD(wParam)) {
                case SB_LINELEFT: cur -= stepSmall; break;
                case SB_LINERIGHT: cur += stepSmall; break;
                case SB_PAGELEFT: cur -= stepPage; break;
                case SB_PAGERIGHT: cur += stepPage; break;
                case SB_THUMBTRACK:
                case SB_THUMBPOSITION: cur = (short)HIWORD(wParam); break;
                default: return 0;
            }
            cur = std::clamp(cur, lo, hi);
            SetScrollPos(sb, SB_CTL, cur, TRUE);
            if (id == settings_ui::ID_SCALE_SCROLL) {
                app->settings.scale = (float)cur / 100.0f;
                applyWindowScaleAndPosition(*app, true, true);
                wchar_t buf[32] = {};
                swprintf_s(buf, L"%d%%", cur);
                SetWindowTextW(GetDlgItem(hwnd, settings_ui::ID_SCALE_VAL), buf);
            } else if (id == settings_ui::ID_ROT_SCROLL) {
                if (std::abs(cur) <= 3) cur = 0;
                else if (std::abs(cur - 90) <= 3) cur = 90;
                else if (std::abs(cur + 90) <= 3) cur = -90;
                SetScrollPos(sb, SB_CTL, cur, TRUE);
                app->settings.rotationDeg = (float)cur;
                app->settings.saveToRegistry();
                wchar_t buf[32] = {};
                swprintf_s(buf, L"%d\u00B0", cur);
                SetWindowTextW(GetDlgItem(hwnd, settings_ui::ID_ROT_VAL), buf);
            }
            return 0;
        }
        case WM_COMMAND: {
            if (!app || app->settingsUpdating) return 0;
            int id = LOWORD(wParam);
            int code = HIWORD(wParam);
            using namespace settings_ui;
            if (code == CBN_SELCHANGE) {
                int sel = (int)SendMessageW((HWND)lParam, CB_GETCURSEL, 0, 0);
                if (sel < 0) return 0;
                if (id == ID_COAT_COMBO) {
                    if (sel < 7) {
                        app->settings.customSkin.clear();
                        app->settings.coatTheme = (CatCoatTheme)sel;
                        switch (app->settings.coatTheme) {
                            case CatCoatTheme::ClassicBlack: app->settings.eyeTheme = EyeColorTheme::NeonChartreuse; break;
                            case CatCoatTheme::Siamese:      app->settings.eyeTheme = EyeColorTheme::SiameseBlue; break;
                            case CatCoatTheme::Shorthair:    app->settings.eyeTheme = EyeColorTheme::ReferenceHazel; break;
                            case CatCoatTheme::OrangeWhite:  app->settings.eyeTheme = EyeColorTheme::WarmAmber; break;
                            case CatCoatTheme::Dazed:        app->settings.eyeTheme = EyeColorTheme::SoftAmber; break;
                            case CatCoatTheme::Tabby:        app->settings.eyeTheme = EyeColorTheme::OliveGold; break;
                            case CatCoatTheme::BritishShorthair: app->settings.eyeTheme = EyeColorTheme::WarmAmber; break;
                        }
                    } else if ((size_t)(sel - 7) < app->skins.skins.size()) {
                        app->settings.customSkin = app->skins.skins[sel - 7].id;
                    }
                    app->settings.saveToRegistry();
                    refreshDressingRoom(*app);
                } else if (id == ID_EYE_COMBO && sel <= (int)EyeColorTheme::OliveGold) {
                    app->settings.eyeTheme = (EyeColorTheme)sel;
                    app->settings.saveToRegistry();
                } else if (id == ID_KB_COMBO) {
                    if (sel < 4) {
                        app->settings.customKeyboard.clear();
                        app->settings.keyboardTheme = (KeyboardTheme)sel;
                    } else if ((size_t)(sel - 4) < app->kbLibrary.skins.size()) {
                        app->settings.customKeyboard = app->kbLibrary.skins[sel - 4].id;
                    }
                    app->settings.saveToRegistry();
                } else if (id == ID_TABLET_COMBO && sel <= 2) {
                    app->settings.tabletMode = sel;
                    app->settings.saveToRegistry();
                } else if (id == ID_OBS_COMBO && sel <= 2) {
                    app->settings.obsCaptureMode = sel;
                    if (sel == 0) app->settings.obsHideDesktop = false;
                    app->settings.saveToRegistry();
                    applyOBSMode(*app);
                    refreshSettingsWindow(*app);
                } else if (id == ID_LANG_COMBO) {
                    bool newEn = (sel == 1);
                    if (newEn != app->settings.isEnglish) {
                        app->settings.isEnglish = newEn;
                        app->settings.saveToRegistry();
                        DestroyWindow(hwnd);
                        showSettingsWindow(*app);
                    }
                }
                return 0;
            }
            if (code == BN_CLICKED) {
                auto isChk = [&](int cid) { return SendMessageW(GetDlgItem(hwnd, cid), BM_GETCHECK, 0, 0) == BST_CHECKED; };
                switch (id) {
                    case ID_BTN_DRESSING: showDressingRoom(*app); break;
                    case ID_BTN_IMPORT_KB: chooseKeyboardSkinFile(*app); break;
                    case ID_BTN_ROT_ZERO:
                        app->settings.rotationDeg = 0.0f;
                        app->settings.saveToRegistry();
                        refreshSettingsWindow(*app);
                        break;
                    case ID_BTN_RESET_POS:
                        app->settings.scale = 1.0f;
                        app->settings.rotationDeg = 0.0f;
                        app->settings.flipHorizontal = false;
                        app->settings.posX = -99999;
                        app->settings.posY = -99999;
                        applyWindowScaleAndPosition(*app, false, true);
                        refreshSettingsWindow(*app);
                        break;
                    case ID_BTN_EDIT_MODE:
                        app->settings.isEditMode = !app->settings.isEditMode;
                        break;
                    case ID_CHK_FLIP:
                        app->settings.flipHorizontal = isChk(id);
                        app->settings.saveToRegistry();
                        break;
                    case ID_CHK_OUTLINE:
                        app->settings.whiteOutline = isChk(id);
                        app->settings.saveToRegistry();
                        break;
                    case ID_CHK_ANGER:
                        app->settings.enableAnger = isChk(id);
                        app->settings.saveToRegistry();
                        break;
                    case ID_CHK_PET:
                        app->settings.petInteraction = isChk(id);
                        app->settings.saveToRegistry();
                        break;
                    case ID_CHK_BADGES:
                        app->settings.showKeyBadges = isChk(id);
                        app->settings.saveToRegistry();
                        break;
                    case ID_CHK_LEGENDS:
                        app->settings.showKeyLegends = isChk(id);
                        app->settings.saveToRegistry();
                        break;
                    case ID_CHK_MARQUEE:
                        app->settings.keyboardMarquee = isChk(id);
                        app->settings.saveToRegistry();
                        break;
                    case ID_CHK_EASTER: {
                        bool on = isChk(id);
                        app->settings.footEasterEgg = on;
                        app->settings.tailEasterEgg = on;
                        app->settings.saveToRegistry();
                        break;
                    }
                    case ID_CHK_TOPMOST:
                        app->settings.alwaysOnTop = isChk(id);
                        applyWindowScaleAndPosition(*app, false, true);
                        break;
                    case ID_CHK_AUTOHIDE_FS:
                        app->settings.autoHideFullscreen = isChk(id);
                        app->settings.saveToRegistry();
                        if (!app->settings.autoHideFullscreen && app->hiddenByFullscreen) {
                            app->hiddenByFullscreen = false;
                            if (app->isVisible) { ShowWindow(app->hwnd, SW_SHOWNOACTIVATE); assertTopmost(*app); }
                        }
                        app->nextHousekeepingTime = 0.0;
                        break;
                    case ID_CHK_CLICKTHROUGH:
                        app->settings.fullClickThrough = isChk(id);
                        app->settings.saveToRegistry();
                        if (app->settings.fullClickThrough) {
                            showTrayBalloon(*app,
                                L"KuroCat \u5DF2\u5F00\u542F\u5B8C\u5168\u9F20\u6807\u7A7F\u900F",
                                L"\u53D6\u6D88\u65B9\u5F0F\uFF1A\u6309 Ctrl+Alt+Shift+P\uFF0C\u6216\u6309\u4F4F Alt \u952E\u53F3\u952E\u5C0F\u732B\uFF0C\u6216\u70B9\u53F3\u4E0B\u89D2\u6258\u76D8\u56FE\u6807");
                        }
                        break;
                    case ID_CHK_AUTOSTART:
                        setAutoStartEnabled(isChk(id));
                        break;
                    case ID_CHK_RUN_ADMIN:
                        app->settings.runAsAdmin = isChk(id);
                        app->settings.saveToRegistry();
                        if (app->settings.runAsAdmin && !app->isElevated && relaunchSelfElevated(L"--takeover --no-elevate")) {
                            quitApp(*app);
                        }
                        break;
                    case ID_BTN_RELAUNCH_ADMIN:
                        if (!app->isElevated && relaunchSelfElevated(L"--takeover --no-elevate")) quitApp(*app);
                        break;
                    case ID_CHK_OBS_HIDE:
                        app->settings.obsHideDesktop = isChk(id);
                        app->settings.saveToRegistry();
                        applyOBSMode(*app);
                        break;
                    case ID_BTN_SKINS_DIR:
                        SkinLibrary::ensureDir();
                        ShellExecuteW(nullptr, L"open", SkinLibrary::dir().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                        break;
                    case ID_BTN_KB_DIR:
                        KeyboardSkinLibrary::ensureDir();
                        ShellExecuteW(nullptr, L"open", KeyboardSkinLibrary::directory().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                        break;
                }
            }
            return 0;
        }
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            if (app && app->settingsWnd == hwnd) {
                app->settingsWnd = nullptr;
                if (app->settingsFont) { DeleteObject(app->settingsFont); app->settingsFont = nullptr; }
                if (app->settingsBoldFont) { DeleteObject(app->settingsBoldFont); app->settingsBoldFont = nullptr; }
                if (app->settingsBgBrush) { DeleteObject(app->settingsBgBrush); app->settingsBgBrush = nullptr; }
            }
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void showSettingsWindow(AppState& app) {
    if (app.settingsWnd && IsWindow(app.settingsWnd)) {
        refreshSettingsWindow(app);
        ShowWindow(app.settingsWnd, SW_SHOWNORMAL);
        SetForegroundWindow(app.settingsWnd);
        return;
    }
    HINSTANCE inst = GetModuleHandleW(nullptr);
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = settingsWndProc;
        wc.hInstance = inst;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = SETTINGS_WINDOW_CLASS;
        registered = RegisterClassExW(&wc) != 0;
    }

    HDC screen = GetDC(nullptr);
    float k = GetDeviceCaps(screen, LOGPIXELSY) / 96.0f;
    ReleaseDC(nullptr, screen);
    auto px = [&](int v) { return (int)std::lround(v * k); };

    app.settingsDark = settings_ui::isWindowsDarkMode();
    app.settingsBgBrush = CreateSolidBrush(app.settingsDark ? RGB(32, 32, 36) : RGB(246, 246, 248));
    app.settingsFont = CreateFontW(-px(13), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    app.settingsBoldFont = CreateFontW(-px(13), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

    bool en = app.settings.isEnglish;
    auto tr = [&](const wchar_t* zh, const wchar_t* eng) -> const wchar_t* { return en ? eng : zh; };

    const int W = 540, H = 618;
    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
    RECT rc = { 0, 0, px(W), px(H) };
    AdjustWindowRectEx(&rc, style, FALSE, 0);
    int winW = rc.right - rc.left, winH = rc.bottom - rc.top;
    RECT work = {};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    HWND win = CreateWindowExW(0, SETTINGS_WINDOW_CLASS,
        tr(L"KuroCat \u8BBE\u7F6E", L"KuroCat Settings"), style,
        (work.left + work.right - winW) / 2, (work.top + work.bottom - winH) / 2,
        winW, winH, nullptr, nullptr, inst, nullptr);
    if (!win) return;
    app.settingsWnd = win;
    settings_ui::applyDwmDarkTitleBar(win, app.settingsDark);

    auto addCtrl = [&](const wchar_t* cls, const wchar_t* txt, DWORD st, int x, int y, int w, int h, int id, bool bold = false) -> HWND {
        HWND c = CreateWindowExW(0, cls, txt, WS_CHILD | WS_VISIBLE | st,
            px(x), px(y), px(w), px(h), win, (HMENU)(INT_PTR)id, inst, nullptr);
        if (c) SendMessageW(c, WM_SETFONT, (WPARAM)(bold ? app.settingsBoldFont : app.settingsFont), TRUE);
        return c;
    };

    using namespace settings_ui;
    // Section 1: Appearance & Orientation
    addCtrl(L"BUTTON", tr(L"\u5916\u89C2\u4E0E\u65B9\u5411", L"Appearance & Orientation"), BS_GROUPBOX, 14, 10, 512, 232, 0, true);

    addCtrl(L"STATIC", tr(L"\u732B\u54AA\u76AE\u80A4", L"Cat Skin"), SS_LEFT, 28, 38, 76, 20, 0);
    addCtrl(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 110, 34, 290, 220, ID_COAT_COMBO);
    addCtrl(L"BUTTON", tr(L"\u6362\u88C5\u95F4\u2026", L"Wardrobe..."), BS_PUSHBUTTON | WS_TABSTOP, 410, 34, 102, 24, ID_BTN_DRESSING);

    addCtrl(L"STATIC", tr(L"\u77B3\u8272\u5207\u6362", L"Eye Color"), SS_LEFT, 28, 70, 76, 20, 0);
    addCtrl(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 110, 66, 290, 220, ID_EYE_COMBO);
    addCtrl(L"BUTTON", tr(L"\u6846\u9009\u8C03\u6574", L"Edit Box"), BS_PUSHBUTTON | WS_TABSTOP, 410, 66, 102, 24, ID_BTN_EDIT_MODE);

    addCtrl(L"STATIC", tr(L"\u952E\u76D8\u914D\u8272", L"Keyboard"), SS_LEFT, 28, 102, 76, 20, 0);
    addCtrl(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 110, 98, 290, 220, ID_KB_COMBO);
    addCtrl(L"BUTTON", tr(L"\u5BFC\u5165\u952E\u76D8\u2026", L"Import..."), BS_PUSHBUTTON | WS_TABSTOP, 410, 98, 102, 24, ID_BTN_IMPORT_KB);

    addCtrl(L"STATIC", tr(L"\u7F29\u653E\u5927\u5C0F", L"Pet Scale"), SS_LEFT, 28, 136, 76, 20, 0);
    HWND scaleSb = addCtrl(L"SCROLLBAR", L"", SBS_HORZ | WS_TABSTOP, 110, 136, 290, 18, ID_SCALE_SCROLL);
    SetScrollRange(scaleSb, SB_CTL, 45, 250, FALSE);
    addCtrl(L"STATIC", L"100%", SS_RIGHT, 410, 136, 48, 20, ID_SCALE_VAL);

    addCtrl(L"STATIC", tr(L"\u671D\u5411\u89D2\u5EA6", L"Rotation"), SS_LEFT, 28, 168, 76, 20, 0);
    HWND rotSb = addCtrl(L"SCROLLBAR", L"", SBS_HORZ | WS_TABSTOP, 110, 168, 290, 18, ID_ROT_SCROLL);
    SetScrollRange(rotSb, SB_CTL, -180, 180, FALSE);
    addCtrl(L"STATIC", L"0\u00B0", SS_RIGHT, 408, 168, 42, 20, ID_ROT_VAL);
    addCtrl(L"BUTTON", tr(L"\u5F52\u96F6", L"0\u00B0"), BS_PUSHBUTTON | WS_TABSTOP, 458, 164, 54, 24, ID_BTN_ROT_ZERO);

    addCtrl(L"BUTTON", tr(L"\u6C34\u5E73\u7FFB\u8F6C", L"Flip Horizontal"), BS_AUTOCHECKBOX | WS_TABSTOP, 28, 202, 140, 22, ID_CHK_FLIP);
    addCtrl(L"BUTTON", tr(L"\u767D\u8272\u8D34\u7EB8\u63CF\u8FB9", L"White Sticker Outline"), BS_AUTOCHECKBOX | WS_TABSTOP, 180, 202, 180, 22, ID_CHK_OUTLINE);
    addCtrl(L"BUTTON", tr(L"\u91CD\u7F6E\u4F4D\u7F6E\u4E0E\u671D\u5411", L"Reset Pose"), BS_PUSHBUTTON | WS_TABSTOP, 384, 200, 128, 24, ID_BTN_RESET_POS);

    // Section 2: Interaction & Effects
    addCtrl(L"BUTTON", tr(L"\u4E92\u52A8\u4E0E\u7279\u6548", L"Interaction & Effects"), BS_GROUPBOX, 14, 252, 512, 148, 0, true);
    addCtrl(L"BUTTON", tr(L"\u9AD8\u901F\u6253\u5B57\u751F\u6C14\u8868\u60C5", L"Angry Expression on Fast Typing"), BS_AUTOCHECKBOX | WS_TABSTOP, 28, 278, 236, 22, ID_CHK_ANGER);
    addCtrl(L"BUTTON", tr(L"\u6478\u732B\u4E92\u52A8\u4E0E\u5495\u565C\u7279\u6548", L"Petting & Purr Sparkles"), BS_AUTOCHECKBOX | WS_TABSTOP, 276, 278, 236, 22, ID_CHK_PET);
    addCtrl(L"BUTTON", tr(L"\u663E\u793A\u6309\u952E\u6C14\u6CE1", L"Show Key & Click Badges"), BS_AUTOCHECKBOX | WS_TABSTOP, 28, 306, 236, 22, ID_CHK_BADGES);
    addCtrl(L"BUTTON", tr(L"\u663E\u793A\u952E\u76D8\u5B57\u7B26", L"Show Keycap Legends"), BS_AUTOCHECKBOX | WS_TABSTOP, 276, 306, 236, 22, ID_CHK_LEGENDS);
    addCtrl(L"BUTTON", tr(L"\u952E\u76D8\u8DD1\u9A6C\u706F\u6D41\u5149", L"Keyboard RGB Marquee"), BS_AUTOCHECKBOX | WS_TABSTOP, 28, 334, 236, 22, ID_CHK_MARQUEE);
    addCtrl(L"BUTTON", tr(L"\u8E29\u952E\u4E0E\u7529\u5C3E\u5F69\u86CB", L"Hind-Foot & Tail Easter Eggs"), BS_AUTOCHECKBOX | WS_TABSTOP, 276, 334, 236, 22, ID_CHK_EASTER);

    addCtrl(L"STATIC", tr(L"\u6570\u4F4D\u677F\u6A21\u5F0F", L"Tablet Mode"), SS_LEFT, 28, 366, 90, 20, 0);
    HWND tabCb = addCtrl(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP, 124, 362, 388, 120, ID_TABLET_COMBO);
    SendMessageW(tabCb, CB_ADDSTRING, 0, (LPARAM)tr(L"\u81EA\u52A8\u68C0\u6D4B\uFF08\u8FDE\u63A5\u6570\u4F4D\u677F\u65F6\u81EA\u52A8\u5207\u6362\uFF09", L"Auto-Detect (Switch When Connected)"));
    SendMessageW(tabCb, CB_ADDSTRING, 0, (LPARAM)tr(L"\u59CB\u7EC8\u663E\u793A\u6570\u4F4D\u677F\u4E0E\u89E6\u63A7\u7B14", L"Always Show Tablet & Stylus"));
    SendMessageW(tabCb, CB_ADDSTRING, 0, (LPARAM)tr(L"\u59CB\u7EC8\u663E\u793A\u9F20\u6807", L"Always Show Mouse"));

    // Section 3: Window & System
    addCtrl(L"BUTTON", tr(L"\u7A97\u53E3\u4E0E\u7CFB\u7EDF", L"Window & System"), BS_GROUPBOX, 14, 410, 512, 194, 0, true);
    addCtrl(L"BUTTON", tr(L"\u7A97\u53E3\u7F6E\u9876\u663E\u793A", L"Always On Top"), BS_AUTOCHECKBOX | WS_TABSTOP, 28, 436, 236, 22, ID_CHK_TOPMOST);
    addCtrl(L"BUTTON", tr(L"\u5168\u5C4F\u5E94\u7528\u65F6\u81EA\u52A8\u9690\u85CF", L"Auto-Hide in Fullscreen"), BS_AUTOCHECKBOX | WS_TABSTOP, 276, 436, 236, 22, ID_CHK_AUTOHIDE_FS);
    addCtrl(L"BUTTON", tr(L"\u5B8C\u5168\u9F20\u6807\u7A7F\u900F (Alt \u4E34\u65F6\u89E3\u9501)", L"Full Click-Through (Hold Alt)"), BS_AUTOCHECKBOX | WS_TABSTOP, 28, 464, 236, 22, ID_CHK_CLICKTHROUGH);
    addCtrl(L"BUTTON", tr(L"\u5F00\u673A\u81EA\u52A8\u542F\u52A8", L"Launch at Startup"), BS_AUTOCHECKBOX | WS_TABSTOP, 276, 464, 236, 22, ID_CHK_AUTOSTART);
    addCtrl(L"BUTTON", tr(L"\u59CB\u7EC8\u4EE5\u7BA1\u7406\u5458\u8EAB\u4EFD\u8FD0\u884C", L"Always Run as Administrator"), BS_AUTOCHECKBOX | WS_TABSTOP, 28, 492, 236, 22, ID_CHK_RUN_ADMIN);
    addCtrl(L"BUTTON", app.isElevated ? tr(L"\u2713 \u5DF2\u7BA1\u7406\u5458\u8FD0\u884C", L"\u2713 Running as Admin")
                                      : tr(L"\u7BA1\u7406\u5458\u8EAB\u4EFD\u91CD\u542F", L"Restart as Admin"),
            BS_PUSHBUTTON | WS_TABSTOP, 276, 490, 148, 24, ID_BTN_RELAUNCH_ADMIN);

    addCtrl(L"STATIC", tr(L"OBS \u76F4\u64AD", L"OBS Capture"), SS_LEFT, 28, 526, 78, 20, 0);
    HWND obsCb = addCtrl(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP, 110, 522, 210, 120, ID_OBS_COMBO);
    SendMessageW(obsCb, CB_ADDSTRING, 0, (LPARAM)tr(L"\u5173\u95ED\uFF08\u6B63\u5E38\u60AC\u6D6E\uFF09", L"Off (Normal Overlay)"));
    SendMessageW(obsCb, CB_ADDSTRING, 0, (LPARAM)tr(L"OBS \u900F\u660E\u7A97\u53E3\u6355\u83B7", L"OBS Transparent Window"));
    SendMessageW(obsCb, CB_ADDSTRING, 0, (LPARAM)tr(L"OBS \u7EFF\u5E55 (#00FF00)", L"OBS Green Screen"));
    addCtrl(L"BUTTON", tr(L"\u540E\u53F0\u6355\u83B7\uFF08\u9690\u85CF\u684C\u9762\uFF09", L"Hide on Desktop"), BS_AUTOCHECKBOX | WS_TABSTOP, 332, 524, 180, 22, ID_CHK_OBS_HIDE);

    addCtrl(L"STATIC", tr(L"\u754C\u9762\u8BED\u8A00", L"Language"), SS_LEFT, 28, 560, 78, 20, 0);
    HWND langCb = addCtrl(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP, 110, 556, 150, 90, ID_LANG_COMBO);
    SendMessageW(langCb, CB_ADDSTRING, 0, (LPARAM)L"\u7B80\u4F53\u4E2D\u6587");
    SendMessageW(langCb, CB_ADDSTRING, 0, (LPARAM)L"English");
    addCtrl(L"BUTTON", tr(L"\u76AE\u80A4\u76EE\u5F55", L"Skins Dir"), BS_PUSHBUTTON | WS_TABSTOP, 290, 556, 106, 24, ID_BTN_SKINS_DIR);
    addCtrl(L"BUTTON", tr(L"\u952E\u76D8\u76EE\u5F55", L"Keyboards Dir"), BS_PUSHBUTTON | WS_TABSTOP, 406, 556, 106, 24, ID_BTN_KB_DIR);

    settings_ui::rebuildCombosAndSync(app);
    ShowWindow(win, SW_SHOWNORMAL);
    SetForegroundWindow(win);
}

void showContextMenu(AppState& app, POINT screenPt) {
    HMENU menu = CreatePopupMenu();
    bool en = app.settings.isEnglish;
    auto tr = [&](const wchar_t* zh, const wchar_t* eng) -> const wchar_t* { return en ? eng : zh; };

    // Status & live stats header
    std::wstring statusStr = app.input.isRawInputRegistered
        ? tr(L"\u25CF \u786C\u4EF6\u7EA7\u5168\u72B6\u6001\u76D1\u542C\u4E2D (Raw Input)",
             L"\u25CF Hardware Input Active (Raw Input)")
        : tr(L"\u26A0 Raw Input \u672A\u542F\u52A8", L"\u26A0 Raw Input Inactive");
    AppendMenuW(menu, MF_STRING | MF_GRAYED, 0, statusStr.c_str());
    if (!app.isElevated && app.lastBlockedForegroundTime > 0.0 && nowSeconds() - app.lastBlockedForegroundTime < 600.0) {
        AppendMenuW(menu, MF_STRING, CMD_RELAUNCH_ADMIN,
            tr(L"\u26A0 \u68C0\u6D4B\u5230\u7BA1\u7406\u5458\u7A97\u53E3\u62E6\u622A\u952E\u76D8\uFF0C\u70B9\u6B64\u7BA1\u7406\u5458\u91CD\u542F",
               L"\u26A0 Admin app blocked keys — Restart as Administrator"));
    }

    std::wstring stats = en
        ? (L"Today " + std::to_wstring(app.input.totalKeystrokesToday)
            + L"  \u00B7  KPM " + std::to_wstring(app.input.currentKPM())
            + L"  \u00B7  Pets " + std::to_wstring(app.physics.totalPetSessions))
        : (L"\u4ECA\u65E5\u51FB\u952E " + std::to_wstring(app.input.totalKeystrokesToday)
            + L"  \u00B7  KPM " + std::to_wstring(app.input.currentKPM())
            + L"  \u00B7  \u6478\u732B " + std::to_wstring(app.physics.totalPetSessions) + L" \u6B21");
    AppendMenuW(menu, MF_STRING | MF_GRAYED, 0, stats.c_str());
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

    // Essential quick actions
    AppendMenuW(menu, MF_STRING, CMD_TOGGLE_VISIBLE, app.isVisible ? tr(L"\u9690\u85CF\u684C\u5BA0", L"Hide Pet") : tr(L"\u663E\u793A\u684C\u5BA0", L"Show Pet"));
    AppendMenuW(menu, MF_STRING | (app.settings.isEditMode ? MF_CHECKED : 0), CMD_TOGGLE_EDIT,
        app.settings.isEditMode
            ? tr(L"\u2713 \u5B8C\u6210\u8C03\u6574\u5927\u5C0F\u4E0E\u65B9\u5411", L"\u2713 Finish Resize & Direction")
            : tr(L"\u8C03\u6574\u5927\u5C0F\u4E0E\u65B9\u5411 (\u53CC\u51FB\u5C0F\u732B)", L"Resize & Direction (Double-Click Cat)"));

    // Quick Cat Skin switcher
    HMENU coatMenu = CreatePopupMenu();
    bool customActive = app.skins.find(app.settings.customSkin) != nullptr;
    auto coatCheck = [&](CatCoatTheme c) -> UINT { return !customActive && app.settings.coatTheme == c ? MF_CHECKED : 0; };
    AppendMenuW(coatMenu, MF_STRING | coatCheck(CatCoatTheme::ClassicBlack), CMD_COAT_BLACK, tr(L"\u7ECF\u5178\u5C0F\u9ED1\u732B (\u539F\u7248)", L"Classic Black Cat"));
    AppendMenuW(coatMenu, MF_STRING | coatCheck(CatCoatTheme::Siamese), CMD_COAT_SIAMESE, tr(L"\u6316\u7164\u66B9\u7F57\u732B", L"Siamese Cat"));
    AppendMenuW(coatMenu, MF_STRING | coatCheck(CatCoatTheme::Shorthair), CMD_COAT_SHORTHAIR, tr(L"\u7F8E\u77ED\uFF08\u94F6\u864E\u6591\u52A0\u767D\uFF09", L"Silver Tabby Shorthair"));
    AppendMenuW(coatMenu, MF_STRING | coatCheck(CatCoatTheme::OrangeWhite), CMD_COAT_ORANGE_WHITE, tr(L"\u6A58\u767D\u732B", L"Orange & White Cat"));
    AppendMenuW(coatMenu, MF_STRING | coatCheck(CatCoatTheme::Dazed), CMD_COAT_DAZED, tr(L"\u5446\u732B", L"Dazed Cat"));
    AppendMenuW(coatMenu, MF_STRING | coatCheck(CatCoatTheme::Tabby), CMD_COAT_TABBY, tr(L"\u72F8\u82B1\u732B", L"Tabby Cat"));
    AppendMenuW(coatMenu, MF_STRING | coatCheck(CatCoatTheme::BritishShorthair), CMD_COAT_BRITISH, tr(L"\u82F1\u77ED\u84DD\u732B\uFF08\u7070\u6BDB\u6A58\u9EC4\u77B3\uFF09", L"British Shorthair (Gray)"));
    size_t listed = std::min<size_t>(app.skins.skins.size(), CMD_SKIN_RANGE);
    if (listed) AppendMenuW(coatMenu, MF_SEPARATOR, 0, nullptr);
    for (size_t i = 0; i < listed; ++i) {
        const SkinSpec& s = app.skins.skins[i];
        AppendMenuW(coatMenu, MF_STRING | (app.settings.customSkin == s.id ? MF_CHECKED : 0), CMD_SKIN_SELECT_BASE + (UINT)i, utf8ToWide(s.name).c_str());
    }
    AppendMenuW(coatMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(coatMenu, MF_STRING, CMD_IMPORT_SKIN, tr(L"\u5BFC\u5165\u76AE\u80A4 / \u6362\u88C5\u95F4\u2026", L"Import Skin / Wardrobe..."));
    AppendMenuW(coatMenu, MF_STRING, CMD_OPEN_WORKSHOP, tr(L"\u6253\u5F00\u76AE\u80A4\u5DE5\u574A\u2026", L"Open Skin Workshop..."));
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)coatMenu, tr(L"\u732B\u54AA\u76AE\u80A4 \U0001F431", L"Cat Skin \U0001F431"));

    // Skin Workshop (Create custom skins) right on the top-level menu
    AppendMenuW(menu, MF_STRING, CMD_OPEN_WORKSHOP,
        tr(L"\U0001F3A8 \u76AE\u80A4\u5DE5\u574A\uFF08\u5236\u4F5C\u76AE\u80A4\uFF09\u2026",
           L"\U0001F3A8 Skin Workshop (Create Skins)..."));

    // Unified Settings window
    AppendMenuW(menu, MF_STRING, CMD_OPEN_SETTINGS, tr(L"\u2699\uFE0F \u8BBE\u7F6E\u2026", L"\u2699\uFE0F Settings..."));

    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    if (!app.availableUpdateVersion.empty()) {
        std::wstring upTxt = app.isUpdating
            ? (std::wstring(tr(L"\u23F3 \u6B63\u5728\u4E0B\u8F7D\u66F4\u65B0 ", L"\u23F3 Downloading update ")) + app.availableUpdateVersion + L"...")
            : (std::wstring(tr(L"\U0001F195 \u53D1\u73B0\u65B0\u7248\u672C ", L"\U0001F195 Update Available ")) + app.availableUpdateVersion);
        AppendMenuW(menu, MF_STRING | (app.isUpdating ? MF_GRAYED : 0), CMD_OPEN_UPDATE, upTxt.c_str());
    } else {
        std::wstring chkTxt = std::wstring(tr(L"\u68C0\u67E5\u66F4\u65B0 (\u5F53\u524D v", L"Check for Updates (v")) + APP_VERSION + L")";
        AppendMenuW(menu, MF_STRING, CMD_CHECK_UPDATE, chkTxt.c_str());
    }
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, CMD_QUIT, tr(L"\u9000\u51FA KuroCat", L"Quit KuroCat"));

    HWND menuOwner = app.menuOwnerHwnd ? app.menuOwnerHwnd : app.hwnd;
    SetForegroundWindow(menuOwner);
    UINT cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, screenPt.x, screenPt.y, 0, menuOwner, nullptr);
    PostMessageW(menuOwner, WM_NULL, 0, 0);
    DestroyMenu(menu);

    switch (cmd) {
        case CMD_TOGGLE_VISIBLE:
            app.isVisible = !app.isVisible;
            app.hiddenByFullscreen = false;
            ShowWindow(app.hwnd, app.isVisible ? SW_SHOWNOACTIVATE : SW_HIDE);
            break;
        case CMD_TOGGLE_CLICKTHROUGH:
            app.settings.fullClickThrough = !app.settings.fullClickThrough;
            app.settings.saveToRegistry();
            if (app.settings.fullClickThrough) {
                showTrayBalloon(app,
                    L"KuroCat \u5DF2\u5F00\u542F\u5B8C\u5168\u9F20\u6807\u7A7F\u900F",
                    L"\u53D6\u6D88\u65B9\u5F0F\uFF1A\u6309 Ctrl+Alt+Shift+P\uFF0C\u6216\u6309\u4F4F Alt \u952E\u53F3\u952E\u5C0F\u732B\uFF0C\u6216\u70B9\u53F3\u4E0B\u89D2\u6258\u76D8\u5C0F\u732B\u56FE\u6807\uFF0C\u6216\u518D\u6B21\u53CC\u51FB KuroCat.exe");
            }
            break;
        case CMD_TOGGLE_TOPMOST:
            app.settings.alwaysOnTop = !app.settings.alwaysOnTop;
            applyWindowScaleAndPosition(app, false);
            break;
        case CMD_TOGGLE_AUTOHIDE_FS:
            app.settings.autoHideFullscreen = !app.settings.autoHideFullscreen;
            app.settings.saveToRegistry();
            if (!app.settings.autoHideFullscreen && app.hiddenByFullscreen) {
                app.hiddenByFullscreen = false;
                if (app.isVisible) {
                    ShowWindow(app.hwnd, SW_SHOWNOACTIVATE);
                    assertTopmost(app);
                }
            }
            app.nextHousekeepingTime = 0.0;
            break;
        case CMD_TOGGLE_AUTOSTART:
            setAutoStartEnabled(!isAutoStartEnabled());
            break;
        case CMD_TOGGLE_BADGES:
            app.settings.showKeyBadges = !app.settings.showKeyBadges;
            app.settings.saveToRegistry();
            break;
        case CMD_TOGGLE_MOUSE_BADGES:
            app.settings.showMouseBadges = !app.settings.showMouseBadges;
            app.settings.saveToRegistry();
            break;
        case CMD_TOGGLE_KEY_LEGENDS:
            app.settings.showKeyLegends = !app.settings.showKeyLegends;
            app.settings.saveToRegistry();
            break;
        case CMD_TOGGLE_FOOT:
            app.settings.footEasterEgg = !app.settings.footEasterEgg;
            app.settings.saveToRegistry();
            break;
        case CMD_TOGGLE_TAIL:
            app.settings.tailEasterEgg = !app.settings.tailEasterEgg;
            app.settings.saveToRegistry();
            break;
        case CMD_TOGGLE_MARQUEE:
            app.settings.keyboardMarquee = !app.settings.keyboardMarquee;
            app.settings.saveToRegistry();
            break;
        case CMD_TOGGLE_PET:
            app.settings.petInteraction = !app.settings.petInteraction;
            app.settings.saveToRegistry();
            break;
        case CMD_OBS_OFF:
            app.settings.obsCaptureMode = 0;
            app.settings.obsHideDesktop = false;
            app.settings.saveToRegistry();
            applyOBSMode(app);
            break;
        case CMD_OBS_TRANSPARENT:
            app.settings.obsCaptureMode = 1;
            app.settings.saveToRegistry();
            applyOBSMode(app);
            break;
        case CMD_OBS_GREENSCREEN:
            app.settings.obsCaptureMode = 2;
            app.settings.saveToRegistry();
            applyOBSMode(app);
            break;
        case CMD_TOGGLE_OBS_HIDE:
            app.settings.obsHideDesktop = !app.settings.obsHideDesktop;
            app.settings.saveToRegistry();
            applyOBSMode(app);
            break;
        case CMD_TOGGLE_EDIT:
            app.settings.isEditMode = !app.settings.isEditMode;
            break;
        case CMD_LANG_ZH:
            app.settings.isEnglish = false;
            app.settings.saveToRegistry();
            break;
        case CMD_LANG_EN:
            app.settings.isEnglish = true;
            app.settings.saveToRegistry();
            break;
        case CMD_TABLET_AUTO:
            app.settings.tabletMode = 0;
            app.settings.saveToRegistry();
            break;
        case CMD_TABLET_ON:
            app.settings.tabletMode = 1;
            app.settings.saveToRegistry();
            break;
        case CMD_TABLET_OFF:
            app.settings.tabletMode = 2;
            app.settings.saveToRegistry();
            break;
        case CMD_IMPORT_SKIN:
            showDressingRoom(app);
            break;
        case CMD_OPEN_WORKSHOP:
            ShellExecuteW(nullptr, L"open", L"https://chanke17.github.io/KuroCat-Workshop/", nullptr, nullptr, SW_SHOWNORMAL);
            break;
        case CMD_OPEN_SKINS_DIR:
            SkinLibrary::ensureDir();
            ShellExecuteW(nullptr, L"open", SkinLibrary::dir().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            break;
        case CMD_IMPORT_KB:
            chooseKeyboardSkinFile(app);
            break;
        case CMD_OPEN_KB_PLAZA:
            ShellExecuteW(nullptr, L"open", L"https://chanke17.github.io/KuroCat-Workshop/#keyboards", nullptr, nullptr, SW_SHOWNORMAL);
            break;
        case CMD_OPEN_KB_DIR:
            KeyboardSkinLibrary::ensureDir();
            ShellExecuteW(nullptr, L"open", KeyboardSkinLibrary::directory().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            break;
        case CMD_COAT_BLACK:
            app.settings.customSkin.clear();
            app.settings.coatTheme = CatCoatTheme::ClassicBlack;
            app.settings.eyeTheme = EyeColorTheme::NeonChartreuse;
            app.settings.saveToRegistry();
            break;
        case CMD_COAT_SIAMESE:
            app.settings.customSkin.clear();
            app.settings.coatTheme = CatCoatTheme::Siamese;
            app.settings.eyeTheme = EyeColorTheme::SiameseBlue;
            app.settings.saveToRegistry();
            break;
        case CMD_COAT_TABBY:
            app.settings.customSkin.clear();
            app.settings.coatTheme = CatCoatTheme::Tabby;
            app.settings.eyeTheme = EyeColorTheme::OliveGold;
            app.settings.saveToRegistry();
            break;
        case CMD_COAT_SHORTHAIR:
        case CMD_COAT_ORANGE_WHITE:
            app.settings.customSkin.clear();
            app.settings.coatTheme = cmd == CMD_COAT_SHORTHAIR ? CatCoatTheme::Shorthair : CatCoatTheme::OrangeWhite;
            app.settings.eyeTheme = cmd == CMD_COAT_SHORTHAIR ? EyeColorTheme::ReferenceHazel : EyeColorTheme::WarmAmber;
            app.settings.saveToRegistry();
            break;
        case CMD_COAT_BRITISH:
            app.settings.customSkin.clear();
            app.settings.coatTheme = CatCoatTheme::BritishShorthair;
            app.settings.eyeTheme = EyeColorTheme::WarmAmber;
            app.settings.saveToRegistry();
            break;
        case CMD_COAT_DAZED:
            app.settings.customSkin.clear();
            app.settings.coatTheme = CatCoatTheme::Dazed;
            app.settings.eyeTheme = EyeColorTheme::SoftAmber;
            app.settings.saveToRegistry();
            break;
        case CMD_EYE_SOFT_AMBER: app.settings.eyeTheme = EyeColorTheme::SoftAmber; app.settings.saveToRegistry(); break;
        case CMD_EYE_REFERENCE_HAZEL: app.settings.eyeTheme = EyeColorTheme::ReferenceHazel; app.settings.saveToRegistry(); break;
        case CMD_EYE_OLIVE_GOLD: app.settings.eyeTheme = EyeColorTheme::OliveGold; app.settings.saveToRegistry(); break;
        case CMD_KB_MILK:
            app.settings.customKeyboard.clear();
            app.settings.keyboardTheme = KeyboardTheme::ClassicMilk;
            app.settings.saveToRegistry();
            break;
        case CMD_KB_HACKER:
            app.settings.customKeyboard.clear();
            app.settings.keyboardTheme = KeyboardTheme::MidnightHacker;
            app.settings.saveToRegistry();
            break;
        case CMD_KB_RETRO:
            app.settings.customKeyboard.clear();
            app.settings.keyboardTheme = KeyboardTheme::RetroCream;
            app.settings.saveToRegistry();
            break;
        case CMD_KB_GRAPHITE:
            app.settings.customKeyboard.clear();
            app.settings.keyboardTheme = KeyboardTheme::Graphite;
            app.settings.saveToRegistry();
            break;
        case CMD_EYE_LIME:  app.settings.eyeTheme = EyeColorTheme::NeonChartreuse; app.settings.saveToRegistry(); break;
        case CMD_EYE_BLUE:  app.settings.eyeTheme = EyeColorTheme::SiameseBlue; app.settings.saveToRegistry(); break;
        case CMD_EYE_AMBER: app.settings.eyeTheme = EyeColorTheme::WarmAmber; app.settings.saveToRegistry(); break;
        case CMD_EYE_CYAN:  app.settings.eyeTheme = EyeColorTheme::IceCyan; app.settings.saveToRegistry(); break;
        case CMD_SCALE_65:  app.settings.scale = 0.65f; applyWindowScaleAndPosition(app, true); break;
        case CMD_SCALE_85:  app.settings.scale = 0.85f; applyWindowScaleAndPosition(app, true); break;
        case CMD_SCALE_100: app.settings.scale = 1.00f; applyWindowScaleAndPosition(app, true); break;
        case CMD_SCALE_125: app.settings.scale = 1.25f; applyWindowScaleAndPosition(app, true); break;
        case CMD_SCALE_150: app.settings.scale = 1.50f; applyWindowScaleAndPosition(app, true); break;
        case CMD_DEMO_BURST:
            app.demoActive = true;
            app.demoStartTime = nowSeconds();
            app.demoStepIdx = 0;
            break;
        case CMD_RESET_POS:
            app.settings.posX = -99999;
            app.settings.posY = -99999;
            applyWindowScaleAndPosition(app, false);
            break;
        case CMD_CHECK_UPDATE:
            if (app.menuOwnerHwnd) startAsyncUpdateCheck(app.menuOwnerHwnd, true, 0);
            break;
        case CMD_OPEN_UPDATE: {
            startInAppUpdate(app);
            break;
        }
        case CMD_RELAUNCH_ADMIN:
            // The elevated copy starts with --takeover and waits for this one to exit.
            if (relaunchSelfElevated(L"--takeover --no-elevate")) quitApp(app);
            break;
        case CMD_TOGGLE_RUN_AS_ADMIN:
            app.settings.runAsAdmin = !app.settings.runAsAdmin;
            app.settings.saveToRegistry();
            if (app.settings.runAsAdmin && !app.isElevated && relaunchSelfElevated(L"--takeover --no-elevate")) {
                quitApp(app);
            }
            break;
        case CMD_OPEN_SETTINGS:
            showSettingsWindow(app);
            break;
        case CMD_QUIT:
            quitApp(app);
            break;
        default:
            if (cmd >= CMD_SKIN_SELECT_BASE && cmd < CMD_SKIN_SELECT_BASE + CMD_SKIN_RANGE && cmd - CMD_SKIN_SELECT_BASE < app.skins.skins.size()) {
                app.settings.customSkin = app.skins.skins[cmd - CMD_SKIN_SELECT_BASE].id;
                app.settings.saveToRegistry();
            } else if (cmd >= CMD_SKIN_DELETE_BASE && cmd < CMD_SKIN_DELETE_BASE + CMD_SKIN_RANGE && cmd - CMD_SKIN_DELETE_BASE < app.skins.skins.size()) {
                confirmDeleteSkin(app, app.skins.skins[cmd - CMD_SKIN_DELETE_BASE].id);
            } else if (cmd >= CMD_KB_SELECT_BASE && cmd < CMD_KB_SELECT_BASE + CMD_SKIN_RANGE && cmd - CMD_KB_SELECT_BASE < app.kbLibrary.skins.size()) {
                app.settings.customKeyboard = app.kbLibrary.skins[cmd - CMD_KB_SELECT_BASE].id;
                app.settings.saveToRegistry();
            } else if (cmd >= CMD_KB_DELETE_BASE && cmd < CMD_KB_DELETE_BASE + CMD_SKIN_RANGE && cmd - CMD_KB_DELETE_BASE < app.kbLibrary.skins.size()) {
                confirmDeleteKeyboardSkin(app, app.kbLibrary.skins[cmd - CMD_KB_DELETE_BASE].id);
            }
            break;
    }
    refreshDressingRoom(app);   // keep the 换装间 preview in sync with menu skin switches
}

bool physicsLooksSane(const CatPhysics& p) {
    const float vals[] = {
        p.leftPaw.x.value, p.leftPaw.y.value, p.rightPaw.x.value, p.rightPaw.y.value,
        p.hindFoot.x.value, p.hindFoot.y.value, p.bodyShiftX.value, p.bodyTilt.value,
        p.bodySquash.value, p.headTilt.value, p.headYaw.value, p.headPitch.value,
        p.headOffsetY.value, p.eyeGaze.x.value, p.eyeGaze.y.value, p.pupilScale.value,
        p.huntIntensity.value, p.scruffDrag.value, p.scruffSwing.value, p.scruffSwingY.value, p.scruffTailSwing.value,
        p.douniLeftRaise.value, p.douniRightRaise.value,
        p.douniLeftKeyTarget.x.value, p.douniLeftKeyTarget.y.value,
        p.douniRightKeyTarget.x.value, p.douniRightKeyTarget.y.value,
        p.douniMouseBlend.value, p.douniLoafBlend.value,
        p.kbImpactY.value, p.kbImpactTilt.value,
        p.mouseOffset.x.value, p.mouseOffset.y.value, p.time
    };
    for (float v : vals) {
        if (!std::isfinite(v) || std::fabs(v) > 1.0e6f) return false;
    }
    return true;
}

void tickAndRender(AppState& app) {
    if (!app.isVisible || app.hiddenByFullscreen || !app.hwnd) return;

    double now = nowSeconds();
    app.lastRenderTime = now;
    float dt = (float)(now - app.lastTickTime);
    app.lastTickTime = now;
    if (dt <= 0.0f || dt > 0.2f) dt = 1.0f / 60.0f;

    // Optional menu demo typing burst
    if (app.demoActive) {
        static const PhysicalKey demoKeys[] = {
            PhysicalKey::W, PhysicalKey::A, PhysicalKey::S, PhysicalKey::D,
            PhysicalKey::F, PhysicalKey::J, PhysicalKey::O, PhysicalKey::E,
            PhysicalKey::Space, PhysicalKey::LeftShift
        };
        int targetIdx = (int)((now - app.demoStartTime) / 0.13);
        while (app.demoStepIdx <= targetIdx && app.demoStepIdx < 10) {
            app.physics.triggerKeyDown(demoKeys[app.demoStepIdx], app.settings, activeSkin(app).typingStyle == SkinTypingStyle::Douni, &app.input.activeKeys);
            app.demoStepIdx++;
        }
        if (app.demoStepIdx >= 10) app.demoActive = false;
    }

    RECT wr = {};
    GetWindowRect(app.hwnd, &wr);
    int w = std::max(1, (int)(wr.right - wr.left));
    int h = std::max(1, (int)(wr.bottom - wr.top));

    const SkinSpec& curSkin = activeSkin(app);
    app.physics.step(dt, wr, app.input, app.settings,
                     curSkin.typingStyle == SkinTypingStyle::Douni,
                     curSkin.renderStyle == SkinRenderStyle::Douni);
    if (!physicsLooksSane(app.physics)) {
        // A single non-finite value would otherwise make every later frame draw nothing (the cat
        // "vanishes" until restart). Rebuild the pose but keep the session statistics.
        int pets = app.physics.totalPetSessions;
        app.physics = CatPhysics();
        app.physics.totalPetSessions = pets;
    }

    // Smart Click-Through + Petting Detection
    if (!app.isDragging && app.activeResizeCorner < 0 && !app.isRotating) {
        POINT cur = {};
        GetCursorPos(&cur);
        if (PtInRect(&wr, cur)) {
            Pt rawCp = {
                (float)(cur.x - wr.left) / (float)w * CatPhysics::CanvasWidth,
                (float)(cur.y - wr.top)  / (float)h * CatPhysics::CanvasHeight
            };
            if (app.settings.isEditMode) {
                setClickThrough(app.hwnd, false);
                app.hasLastHover = false;
            } else {
                Pt cp = CatRenderer::canvasPointToCatSpace(rawCp, app.settings.flipHorizontal, app.settings.rotationDeg);
                CatHitZone zone = CatRenderer::hitTest(cp, app.physics, activeSkin(app));
                app.physics.updateTailHover(&cp);
                if (zone == CatHitZone::Head) {
                    bool skipPetForHunt = (curSkin.renderStyle == SkinRenderStyle::Douni && app.physics.huntIntensity.value > 0.20f);
                    if (!skipPetForHunt && app.hasLastHover) {
                        float d = std::hypot(cp.x - app.lastHoverCanvas.x, cp.y - app.lastHoverCanvas.y);
                        if (d > 1.2f && d < 80.0f) {
                            app.physics.registerPetStroke(cp, d, true, app.settings);
                        }
                    }
                    app.lastHoverCanvas = cp;
                    app.hasLastHover = true;
                } else {
                    app.hasLastHover = false;
                }
                // Holding Alt temporarily unlocks click-through so user can ALWAYS right-click or drag the cat!
                bool altHeld = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
                bool clickThrough = (app.settings.fullClickThrough && !altHeld) || (zone == CatHitZone::None);
                setClickThrough(app.hwnd, clickThrough);
            }
        } else {
            app.hasLastHover = false;
            app.physics.updateTailHover(nullptr);
            setClickThrough(app.hwnd, true);
        }
    }

    if (app.pendingMenuAfterAlt && (GetAsyncKeyState(VK_MENU) & 0x8000) == 0) {
        app.pendingMenuAfterAlt = false;
        POINT pt = app.pendingMenuPt;
        showContextMenu(app, pt);
    }

    // Render 32-bit Premultiplied ARGB frame directly into DIB and present via UpdateLayeredWindow
    app.ensureBuffer(w, h);
    if (!app.memDC || !app.dibBits) return;

    Gdiplus::Bitmap bmp(w, h, w * 4, PixelFormat32bppPARGB, static_cast<BYTE*>(app.dibBits));
    Gdiplus::Graphics g(&bmp);
    g.Clear(app.settings.obsCaptureMode == 2 ? Gdiplus::Color(255, 0, 255, 0) : Gdiplus::Color(0, 0, 0, 0));
    g.ScaleTransform((float)w / CatPhysics::CanvasWidth, (float)h / CatPhysics::CanvasHeight);
    CatRenderer::draw(g, app.physics, app.settings, activeSkin(app), activeKeyboardSkin(app));

    POINT ptDst = { wr.left, wr.top };
    SIZE szWnd = { w, h };
    POINT ptSrc = { 0, 0 };
    BLENDFUNCTION blend = {};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;

    HDC screenDC = GetDC(nullptr);
    BOOL presented = UpdateLayeredWindow(app.hwnd, screenDC, &ptDst, &szWnd, app.memDC, &ptSrc, 0, &blend, ULW_ALPHA);
    ReleaseDC(nullptr, screenDC);
    if (presented) {
        app.ulwFailures = 0;
    } else if (++app.ulwFailures > 30) {
        // Surface lost (typically after Win+L / display reconnect): rebuild the DIB next frame.
        app.ulwFailures = 0;
        app.destroyBuffer();
    }
}

/// Renders at most once per FRAME_INTERVAL; called from both the main loop and WM_TIMER (the latter
/// keeps the cat alive while a menu / message box runs its own modal loop).
void renderIfDue(AppState& app) {
    double now = nowSeconds();
    if (now < app.nextFrameTime) return;
    app.nextFrameTime = (now - app.nextFrameTime < FRAME_INTERVAL) ? app.nextFrameTime + FRAME_INTERVAL
                                                                    : now + FRAME_INTERVAL;
    tickAndRender(app);
}

LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    AppState* app = g_app;
    switch (msg) {
        case WM_INPUT:
            if (app) app->input.handleRawInput(lParam);
            return 0;

        case WM_TIMER:
            if (wParam == TIMER_RENDER && app) renderIfDue(*app);
            return 0;

        case WM_WTSSESSION_CHANGE:
            if (app && hwnd == app->hwnd) {
                switch (wParam) {
                    case WTS_SESSION_LOCK:
                    case WTS_CONSOLE_DISCONNECT:
                    case WTS_REMOTE_DISCONNECT:
                        // Win+L: the key-ups for Win and L go to the secure desktop, never to us.
                        app->input.resetAllState();
                        break;
                    case WTS_SESSION_UNLOCK:
                    case WTS_SESSION_LOGON:
                    case WTS_CONSOLE_CONNECT:
                    case WTS_REMOTE_CONNECT:
                        recoverAfterSystemEvent(*app);
                        break;
                    default:
                        break;
                }
            }
            return 0;

        case WM_POWERBROADCAST:
            if (app && hwnd == app->hwnd &&
                (wParam == PBT_APMRESUMEAUTOMATIC || wParam == PBT_APMRESUMESUSPEND)) {
                recoverAfterSystemEvent(*app);
            }
            return TRUE;

        case WM_DISPLAYCHANGE:
            // Fullscreen games often switch resolution; the cat could end up off-screen or with a
            // stale surface. Re-place it (without touching the saved position) and rebuild the buffer.
            if (app && hwnd == app->hwnd) recoverAfterSystemEvent(*app);
            return 0;

        case WM_REQUEST_QUIT:
            if (app) quitApp(*app);
            return 0;

        case WM_SETCURSOR:
            if (LOWORD(lParam) == HTCLIENT) {
                if (app && hwnd == app->hwnd && app->settings.isEditMode) {
                    RECT wr = {};
                    GetWindowRect(hwnd, &wr);
                    int w = std::max(1, (int)(wr.right - wr.left));
                    int h = std::max(1, (int)(wr.bottom - wr.top));
                    POINT cur = {};
                    GetCursorPos(&cur);
                    Pt rawCp = {
                        (float)(cur.x - wr.left) / (float)w * CatPhysics::CanvasWidth,
                        (float)(cur.y - wr.top)  / (float)h * CatPhysics::CanvasHeight
                    };
                    int hit = CatRenderer::hitTestEditOverlay(rawCp);
                    if (hit == 0 || hit == 3) { SetCursor(LoadCursorW(nullptr, IDC_SIZENWSE)); return TRUE; }
                    if (hit == 1 || hit == 2) { SetCursor(LoadCursorW(nullptr, IDC_SIZENESW)); return TRUE; }
                    if (hit == 20) { SetCursor(LoadCursorW(nullptr, IDC_HAND)); return TRUE; }
                    if (hit == 12) { SetCursor(LoadCursorW(nullptr, IDC_SIZEALL)); return TRUE; }
                    SetCursor(LoadCursorW(nullptr, IDC_ARROW));
                    return TRUE;
                }
                SetCursor(LoadCursorW(nullptr, IDC_HAND));
                return TRUE;
            }
            break;

        case WM_LBUTTONDBLCLK:
            if (app && hwnd == app->hwnd) {
                if (app->settings.isEditMode) {
                    RECT wr = {};
                    GetWindowRect(hwnd, &wr);
                    int w = std::max(1, (int)(wr.right - wr.left));
                    int h = std::max(1, (int)(wr.bottom - wr.top));
                    POINT cur = {};
                    GetCursorPos(&cur);
                    Pt rawCp = {
                        (float)(cur.x - wr.left) / (float)w * CatPhysics::CanvasWidth,
                        (float)(cur.y - wr.top)  / (float)h * CatPhysics::CanvasHeight
                    };
                    if (CatRenderer::hitTestEditOverlay(rawCp) == 20) {
                        app->settings.rotationDeg = 0.0f;
                        app->settings.saveToRegistry();
                        refreshSettingsWindow(*app);
                        return 0;
                    }
                }
                app->settings.isEditMode = !app->settings.isEditMode;
            }
            return 0;

        case WM_LBUTTONDOWN: {
            bool altHeld = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
            if (app && hwnd == app->hwnd && (app->settings.isEditMode || !app->settings.fullClickThrough || altHeld)) {
                RECT wr = {};
                GetWindowRect(hwnd, &wr);
                int w = std::max(1, (int)(wr.right - wr.left));
                int h = std::max(1, (int)(wr.bottom - wr.top));
                POINT cur = {};
                GetCursorPos(&cur);
                if (app->settings.isEditMode) {
                    Pt rawCp = {
                        (float)(cur.x - wr.left) / (float)w * CatPhysics::CanvasWidth,
                        (float)(cur.y - wr.top)  / (float)h * CatPhysics::CanvasHeight
                    };
                    int hit = CatRenderer::hitTestEditOverlay(rawCp);
                    if (hit == 20) {
                        app->isRotating = true;
                        app->rotateMoved = false;
                        float cx = (float)wr.left + (float)w * 0.5f;
                        float cy = (float)wr.top + (float)h * 0.5f;
                        app->rotateStartAngle = std::atan2((float)cur.y - cy, (float)cur.x - cx);
                        app->rotateStartDeg = app->settings.rotationDeg;
                        SetCapture(hwnd);
                        return 0;
                    }
                    if (hit >= 0 && hit <= 3) {
                        app->activeResizeCorner = hit;
                        // Windows screen Y is top-down: anchor opposite corner
                        if (hit == 0) app->resizeAnchorScreenPt = { wr.right, wr.bottom };
                        else if (hit == 1) app->resizeAnchorScreenPt = { wr.left, wr.bottom };
                        else if (hit == 2) app->resizeAnchorScreenPt = { wr.right, wr.top };
                        else app->resizeAnchorScreenPt = { wr.left, wr.top };
                        SetCapture(hwnd);
                        return 0;
                    }
                    if (hit < 0) {
                        app->settings.isEditMode = false;
                        return 0;
                    }
                }
                app->isDragging = true;
                app->dragStartCursor = cur;
                app->dragStartWinOrigin = { wr.left, wr.top };
                app->physics.setDragging(true, 0.0f);
                SetCapture(hwnd);
            }
            return 0;
        }

        case WM_MOUSEMOVE:
            if (app && app->isRotating) {
                RECT wr = {};
                GetWindowRect(hwnd, &wr);
                int w = std::max(1, (int)(wr.right - wr.left));
                int h = std::max(1, (int)(wr.bottom - wr.top));
                POINT cur = {};
                GetCursorPos(&cur);
                float cx = (float)wr.left + (float)w * 0.5f;
                float cy = (float)wr.top + (float)h * 0.5f;
                float angle = std::atan2((float)cur.y - cy, (float)cur.x - cx);
                float deltaDeg = (angle - app->rotateStartAngle) * (180.0f / 3.14159265f);
                while (deltaDeg > 180.0f) deltaDeg -= 360.0f;
                while (deltaDeg < -180.0f) deltaDeg += 360.0f;
                if (std::fabs(deltaDeg) > 1.5f) app->rotateMoved = true;
                if (app->rotateMoved) {
                    float deg = app->rotateStartDeg + deltaDeg;
                    while (deg > 180.0f) deg -= 360.0f;
                    while (deg <= -180.0f) deg += 360.0f;
                    // Soft-snap to cardinal angles within +/-5 deg
                    const float snaps[5] = { 0.0f, 90.0f, -90.0f, 180.0f, -180.0f };
                    for (float s : snaps) {
                        if (std::fabs(deg - s) <= 5.0f) { deg = (s == -180.0f ? 180.0f : s); break; }
                    }
                    app->settings.rotationDeg = deg;
                }
                return 0;
            }
            if (app && app->activeResizeCorner >= 0) {
                POINT cur = {};
                GetCursorPos(&cur);
                float dx = std::fabs((float)(cur.x - app->resizeAnchorScreenPt.x));
                float dy = std::fabs((float)(cur.y - app->resizeAnchorScreenPt.y));
                float newScale = std::clamp(std::max(dx / CatPhysics::CanvasWidth, dy / CatPhysics::CanvasHeight), 0.45f, 2.50f);
                int nw = (int)std::round(CatPhysics::CanvasWidth * newScale);
                int nh = (int)std::round(CatPhysics::CanvasHeight * newScale);
                int nx = (app->activeResizeCorner == 0 || app->activeResizeCorner == 2)
                    ? (app->resizeAnchorScreenPt.x - nw) : app->resizeAnchorScreenPt.x;
                int ny = (app->activeResizeCorner == 0 || app->activeResizeCorner == 1)
                    ? (app->resizeAnchorScreenPt.y - nh) : app->resizeAnchorScreenPt.y;
                app->settings.scale = newScale;
                SetWindowPos(hwnd, nullptr, nx, ny, nw, nh, SWP_NOZORDER | SWP_NOACTIVATE);
                return 0;
            }
            if (app && app->isDragging) {
                POINT cur = {};
                GetCursorPos(&cur);
                RECT wr = {};
                GetWindowRect(hwnd, &wr);
                int nx = app->dragStartWinOrigin.x + (cur.x - app->dragStartCursor.x);
                int ny = app->dragStartWinOrigin.y + (cur.y - app->dragStartCursor.y);
                float stepDx = (float)(nx - wr.left);
                float stepDy = (float)(ny - wr.top);
                float rad = -app->settings.rotationDeg * (3.14159265f / 180.0f);
                float c = std::cos(rad), s = std::sin(rad);
                float localDx = stepDx * c - stepDy * s;
                float localDy = stepDx * s + stepDy * c;
                app->physics.setDragging(true, app->settings.flipHorizontal ? -localDx : localDx, localDy);
                SetWindowPos(hwnd, nullptr, nx, ny, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            }
            return 0;

        case WM_LBUTTONUP:
            if (app && app->isRotating) {
                app->isRotating = false;
                ReleaseCapture();
                if (!app->rotateMoved) {
                    app->settings.flipHorizontal = !app->settings.flipHorizontal;
                }
                app->settings.saveToRegistry();
                refreshSettingsWindow(*app);
                return 0;
            }
            if (app && app->activeResizeCorner >= 0) {
                app->activeResizeCorner = -1;
                ReleaseCapture();
                RECT wr = {};
                GetWindowRect(hwnd, &wr);
                app->settings.posX = wr.left;
                app->settings.posY = wr.top;
                app->settings.saveToRegistry();
                refreshSettingsWindow(*app);
                return 0;
            }
            if (app && app->isDragging) {
                app->isDragging = false;
                app->physics.setDragging(false);
                ReleaseCapture();
                RECT wr = {};
                GetWindowRect(hwnd, &wr);
                app->settings.posX = wr.left;
                app->settings.posY = wr.top;
                app->settings.saveToRegistry();
                refreshSettingsWindow(*app);
            }
            return 0;

        case WM_RBUTTONDOWN:
            if (app && hwnd == app->hwnd) {
                bool altHeld = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
                if (!app->settings.fullClickThrough || altHeld) {
                    app->rButtonDown = true;
                }
            }
            return 0;

        case WM_RBUTTONUP:
            if (app && hwnd == app->hwnd && app->rButtonDown) {
                app->rButtonDown = false;
                POINT pt = {};
                GetCursorPos(&pt);
                if ((GetAsyncKeyState(VK_MENU) & 0x8000) != 0) {
                    // Defer opening the popup menu until Alt is released; otherwise
                    // Windows' TrackPopupMenu modal loop cancels the menu on WM_SYSKEYUP(VK_MENU).
                    app->pendingMenuAfterAlt = true;
                    app->pendingMenuPt = pt;
                } else {
                    showContextMenu(*app, pt);
                }
            }
            return 0;

        case WM_CANCELMODE:
        case WM_CAPTURECHANGED:
            if (app && hwnd == app->hwnd) {
                app->rButtonDown = false;
                if (app->isDragging) {
                    app->isDragging = false;
                    app->physics.setDragging(false);
                }
            }
            return 0;

        case WM_TRAYICON:
            if (app && (lParam == WM_RBUTTONUP || lParam == WM_LBUTTONUP || lParam == WM_CONTEXTMENU)) {
                POINT pt = {};
                GetCursorPos(&pt);
                showContextMenu(*app, pt);
            }
            return 0;

        case WM_HOTKEY:
            if (app && wParam == HOTKEY_CLICKTHROUGH_ID) {
                app->settings.fullClickThrough = !app->settings.fullClickThrough;
                app->settings.saveToRegistry();
                if (app->settings.fullClickThrough) {
                    showTrayBalloon(*app,
                        L"KuroCat \u5DF2\u5F00\u542F\u5B8C\u5168\u9F20\u6807\u7A7F\u900F",
                        L"\u518D\u6B21\u6309 Ctrl+Alt+Shift+P \u6216\u6309\u4F4F Alt \u53F3\u952E\u5C0F\u732B\u5373\u53EF\u53D6\u6D88");
                } else {
                    showTrayBalloon(*app,
                        L"KuroCat \u5DF2\u53D6\u6D88\u5B8C\u5168\u9F20\u6807\u7A7F\u900F",
                        L"\u73B0\u5728\u53EF\u4EE5\u6B63\u5E38\u62D6\u52A8\u3001\u6478\u5934\u548C\u53F3\u952E\u6253\u5F00\u83DC\u5355\u4E86");
                }
            }
            return 0;

        case WM_WAKE_EXISTING:
            if (app) {
                // Re-launching KuroCat.exe unlocks click-through, restores visibility & opens settings menu!
                if (app->settings.fullClickThrough) {
                    app->settings.fullClickThrough = false;
                    app->settings.saveToRegistry();
                }
                if (!app->isVisible) {
                    app->isVisible = true;
                    ShowWindow(app->hwnd, SW_SHOWNOACTIVATE);
                }
                POINT pt = {};
                GetCursorPos(&pt);
                showContextMenu(*app, pt);
            }
            return 0;

        case WM_UPDATE_CHECK_DONE: {
            auto* res = reinterpret_cast<UpdateCheckResult*>(lParam);
            if (res) {
                if (app && res->ok && res->hasUpdate) {
                    app->availableUpdateVersion = res->latestTag;
                    app->availableUpdateUrl = res->releaseUrl;
                    // Show WHICH exe is running: the usual cause of "I already updated but it still
                    // says there's a new version" is an old copy (Desktop / autostart) still running.
                    std::wstring msg = L"\u53D1\u73B0 KuroCat \u65B0\u7248\u672C " + res->latestTag
                        + L"\n\u5F53\u524D\u7248\u672C\uFF1Av" + APP_VERSION
                        + L"\n\u5F53\u524D\u8FD0\u884C\u7684\u7A0B\u5E8F\uFF1A" + currentExePath()
                        + L"\n\n\u5982\u679C\u4F60\u5DF2\u7ECF\u4E0B\u8F7D\u8FC7\u65B0\u7248\uFF0C\u76F4\u63A5\u53CC\u51FB\u65B0\u7248 exe \u5373\u53EF\u81EA\u52A8\u66FF\u6362\u6389\u8FD9\u4E2A\u65E7\u7248\u3002"
                        + L"\n\n\u662F\u5426\u7ACB\u5373\u5728\u8F6F\u4EF6\u5185\u81EA\u52A8\u4E0B\u8F7D\u5E76\u66F4\u65B0\u91CD\u542F\uFF1F";
                    if (MessageBoxW(nullptr, msg.c_str(), L"KuroCat \u66F4\u65B0\u63D0\u9192",
                                    MB_YESNO | MB_ICONINFORMATION | MB_TOPMOST) == IDYES) {
                        startInAppUpdate(*app);
                    }
                } else if (res->manual) {
                    if (res->ok) {
                        std::wstring msg = std::wstring(L"\u5F53\u524D\u7248\u672C v") + APP_VERSION + L" \u5DF2\u662F\u6700\u65B0\u7248\u672C\u3002";
                        MessageBoxW(nullptr, msg.c_str(), L"KuroCat \u68C0\u67E5\u66F4\u65B0", MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
                    } else {
                        MessageBoxW(nullptr, L"\u65E0\u6CD5\u8FDE\u63A5\u5230\u66F4\u65B0\u670D\u52A1\u5668\uFF0C\u8BF7\u7A0D\u540E\u91CD\u8BD5\u6216\u68C0\u67E5\u7F51\u7EDC\u3002",
                                    L"KuroCat \u68C0\u67E5\u66F4\u65B0", MB_OK | MB_ICONWARNING | MB_TOPMOST);
                    }
                }
                delete res;
            }
            return 0;
        }

        case WM_UPDATE_DOWNLOAD_DONE: {
            auto* res = reinterpret_cast<UpdateDownloadResult*>(lParam);
            if (res) {
                if (app) app->isUpdating = false;
                bool launched = false;
                if (res->ok && !res->newExePath.empty() && !res->targetExePath.empty()) {
                    std::wstring args = L"--apply-update " + std::to_wstring(GetCurrentProcessId())
                                      + L" \"" + res->targetExePath + L"\"";
                    launched = launchProcessArgs(res->newExePath, args);
                }
                if (launched) {
                    PostQuitMessage(0);
                } else {
                    if (MessageBoxW(nullptr,
                                    L"\u81EA\u52A8\u4E0B\u8F7D\u6216\u66FF\u6362\u5B89\u88C5\u5305\u5931\u8D25\uFF0C\u662F\u5426\u6253\u5F00\u4E0B\u8F7D\u9875\u624B\u52A8\u4E0B\u8F7D\uFF1F",
                                    L"KuroCat \u81EA\u52A8\u66F4\u65B0", MB_YESNO | MB_ICONWARNING | MB_TOPMOST) == IDYES) {
                        const wchar_t* url = (app && !app->availableUpdateUrl.empty())
                            ? app->availableUpdateUrl.c_str()
                            : UPDATE_PAGE_URL;
                        ShellExecuteW(nullptr, L"open", url, nullptr, nullptr, SW_SHOWNORMAL);
                    }
                }
                delete res;
            }
            return 0;
        }

        case WM_DROPFILES:
            // A .kurocat dropped onto the cat is imported and worn right away.
            if (app) importDroppedFiles(*app, (HDROP)wParam);
            else DragFinish((HDROP)wParam);
            return 0;

        case WM_DESTROY:
            if (hwnd == (app ? app->hwnd : nullptr)) {
                KillTimer(hwnd, TIMER_RENDER);
                if (app) {
                    if (app->sessionNotifyRegistered) {
                        WTSUnRegisterSessionNotification(hwnd);
                        app->sessionNotifyRegistered = false;
                    }
                    if (app->foregroundHook) {
                        UnhookWinEvent(app->foregroundHook);
                        app->foregroundHook = nullptr;
                    }
                    if (app->menuOwnerHwnd) {
                        UnregisterHotKey(app->menuOwnerHwnd, HOTKEY_CLICKTHROUGH_ID);
                        RemovePropW(app->menuOwnerHwnd, VERSION_PROP);
                        DestroyWindow(app->menuOwnerHwnd);
                        app->menuOwnerHwnd = nullptr;
                    }
                    Shell_NotifyIconW(NIM_DELETE, &app->nid);
                    if (app->trayIcon) DestroyIcon(app->trayIcon);
                    if (app->dressingRoom) DestroyWindow(app->dressingRoom);
                    app->destroyBuffer();
                }
                PostQuitMessage(0);
            }
            return 0;
    }
    if (g_msgTaskbarCreated != 0 && msg == g_msgTaskbarCreated && app && hwnd == app->menuOwnerHwnd) {
        registerTrayIcon(*app);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void CALLBACK onForegroundChanged(HWINEVENTHOOK, DWORD, HWND, LONG, LONG, DWORD, DWORD) {
    // A game just came to the front: many make themselves topmost right after showing, so
    // re-assert our own topmost state shortly afterwards instead of staying buried ("vanished").
    if (g_app) g_app->pendingTopmostTime = nowSeconds() + 0.4;
}

/// Asks the already-running KuroCat to exit and waits for it. Builds <= 1.3.2 do not know
/// WM_REQUEST_QUIT, but they shut down cleanly when their overlay window receives WM_CLOSE.
bool shutDownRunningInstance(HWND owner, DWORD timeoutMs) {
    DWORD pid = 0;
    if (owner) GetWindowThreadProcessId(owner, &pid);
    HANDLE hProc = pid ? OpenProcess(SYNCHRONIZE, FALSE, pid) : nullptr;
    if (owner) PostMessageW(owner, WM_REQUEST_QUIT, 0, 0);
    if (HWND overlay = FindWindowW(OVERLAY_CLASS, nullptr)) PostMessageW(overlay, WM_CLOSE, 0, 0);
    bool gone = false;
    if (hProc) {
        gone = (WaitForSingleObject(hProc, timeoutMs) == WAIT_OBJECT_0);
        CloseHandle(hProc);
    } else {
        for (DWORD waited = 0; waited <= timeoutMs; waited += 100) {
            if (!FindWindowW(TRAY_OWNER_CLASS, nullptr) && !FindWindowW(OVERLAY_CLASS, nullptr)) { gone = true; break; }
            Sleep(100);
        }
    }
    return gone;
}

/// Native Win32 self-update handler (no .bat scripts or hidden cmd.exe).
/// Returns true if this process was invoked solely to apply an update and should now exit.
bool handleNativeSelfUpdateIfRequested() {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return false;

    DWORD oldPid = 0;
    std::wstring targetPath;
    std::wstring cleanupPath;
    for (int i = 1; i < argc; ++i) {
        if (_wcsicmp(argv[i], L"--apply-update") == 0 && i + 2 < argc) {
            oldPid = (DWORD)wcstoul(argv[i + 1], nullptr, 10);
            targetPath = argv[i + 2];
            i += 2;
        } else if (_wcsicmp(argv[i], L"--cleanup-update") == 0 && i + 1 < argc) {
            cleanupPath = argv[i + 1];
            i += 1;
        }
    }
    LocalFree(argv);

    if (!cleanupPath.empty()) {
        for (int attempt = 0; attempt < 10; ++attempt) {
            if (DeleteFileW(cleanupPath.c_str()) || GetLastError() == ERROR_FILE_NOT_FOUND) break;
            Sleep(150);
        }
    }

    if (targetPath.empty()) return false;

    if (oldPid != 0 && oldPid != GetCurrentProcessId()) {
        HANDLE hOld = OpenProcess(SYNCHRONIZE, FALSE, oldPid);
        if (hOld) {
            WaitForSingleObject(hOld, 12000);
            CloseHandle(hOld);
        }
    }

    wchar_t myPath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, myPath, MAX_PATH);
    std::wstring myPathStr(myPath);

    std::wstring launchPath = targetPath;
    bool copied = false;
    for (int attempt = 0; attempt < 20; ++attempt) {
        if (CopyFileW(myPathStr.c_str(), targetPath.c_str(), FALSE)) {
            copied = true;
            break;
        }
        Sleep(250);
    }

    // Per-user fallback folder if the original directory is read-only or locked
    if (!copied) {
        wchar_t localAppData[MAX_PATH] = {};
        GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, MAX_PATH);
        if (localAppData[0]) {
            std::wstring fbDir = std::wstring(localAppData) + L"\\Programs";
            CreateDirectoryW(fbDir.c_str(), nullptr);
            fbDir += L"\\KuroCat";
            CreateDirectoryW(fbDir.c_str(), nullptr);
            std::wstring fbExe = fbDir + L"\\KuroCat.exe";
            if (CopyFileW(myPathStr.c_str(), fbExe.c_str(), FALSE)) {
                launchPath = fbExe;
                copied = true;
            }
        }
    }

    if (copied) {
        launchProcessArgs(launchPath, L"--takeover --cleanup-update \"" + myPathStr + L"\"");
        return true;
    }
    // If both copies failed, simply continue running from the downloaded exe itself
    return false;
}

} // namespace

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int) {
    // Enable best available DPI awareness (Per-Monitor V2 on Win10/11, Per-Monitor on Win8.1, System DPI on Win7)
    enableBestDpiAwareness();

    if (handleNativeSelfUpdateIfRequested()) {
        return 0;
    }

    const int myVersion = versionToNumber(APP_VERSION);
    const bool forceTakeover = commandLineHasFlag(L"--takeover");   // admin relaunch
    const bool noElevate = commandLineHasFlag(L"--no-elevate");
    const wchar_t* kMutexName = L"Local\\KuroCatDesktopPetSingleInstance";

    // Single-instance guard. Re-launching the SAME version wakes the existing cat (cancels full
    // click-through and pops the menu so users never get stuck). Launching a NEWER version (e.g. a
    // freshly downloaded exe while the old one still sits in the tray or autostarts from another
    // folder) now replaces the old process instead of silently waking it — previously the old
    // version kept running and kept reporting "new version available".
    HANDLE hMutex = CreateMutexW(nullptr, FALSE, kMutexName);
    DWORD mutexErr = GetLastError();
    bool alreadyRunning = (hMutex && mutexErr == ERROR_ALREADY_EXISTS) || (!hMutex && mutexErr == ERROR_ACCESS_DENIED);
    if (alreadyRunning) {
        HWND owner = FindWindowW(TRAY_OWNER_CLASS, nullptr);
        if (!owner && forceTakeover) {
            // The instance we replace is already shutting down; give it a moment to disappear.
            for (int i = 0; i < 50 && FindWindowW(OVERLAY_CLASS, nullptr); ++i) Sleep(100);
        } else if (!owner) {
            if (hMutex) CloseHandle(hMutex);
            return 0;
        }
        if (owner) {
            int runningVersion = (int)(INT_PTR)GetPropW(owner, VERSION_PROP);
            bool replace = forceTakeover || runningVersion < myVersion;
            if (!replace || !shutDownRunningInstance(owner, 6000)) {
                PostMessageW(owner, WM_WAKE_EXISTING, 0, 0);
                if (hMutex) CloseHandle(hMutex);
                return 0;
            }
        }
        if (!hMutex) hMutex = CreateMutexW(nullptr, FALSE, kMutexName);
    }

    AppState app;
    g_app = &app;
    app.settings.loadFromRegistry();
    app.skins.reload();
    app.kbLibrary.reload();
    app.isElevated = isCurrentProcessElevated();

    // "Always run as administrator": hand over to an elevated copy (one UAC prompt). If the user
    // declines, simply carry on non-elevated.
    if (app.settings.runAsAdmin && !app.isElevated && !noElevate) {
        if (hMutex) { CloseHandle(hMutex); hMutex = nullptr; }
        if (relaunchSelfElevated(L"--takeover --no-elevate")) {
            g_app = nullptr;
            return 0;
        }
        hMutex = CreateMutexW(nullptr, FALSE, kMutexName);
    }

    g_msgTaskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");

    Gdiplus::GdiplusStartupInput gdipInput;
    ULONG_PTR gdipToken = 0;
    Gdiplus::GdiplusStartup(&gdipToken, &gdipInput, nullptr);

    // Refresh auto-start exe path if user enabled auto-start and moved KuroCat.exe
    if (isAutoStartEnabled()) {
        setAutoStartEnabled(true);
    }

    app.input.onKeyDown = [&](PhysicalKey k) {
        app.physics.triggerKeyDown(k, app.settings, activeSkin(app).typingStyle == SkinTypingStyle::Douni, &app.input.activeKeys);
    };
    app.input.onMouseClick = [&](int btn, bool down) {
        app.physics.triggerMouseClick(btn, down, app.settings, activeSkin(app).renderStyle == SkinRenderStyle::Douni);
    };

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_DBLCLKS;
    wc.lpfnWndProc = wndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = OVERLAY_CLASS;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassExW(&wc);

    // Dedicated hidden tray/menu owner window (NO WS_EX_NOACTIVATE / WS_EX_TRANSPARENT)
    WNDCLASSEXW trayWc = {};
    trayWc.cbSize = sizeof(trayWc);
    trayWc.lpfnWndProc = wndProc;
    trayWc.hInstance = hInst;
    trayWc.lpszClassName = TRAY_OWNER_CLASS;
    RegisterClassExW(&trayWc);

    app.menuOwnerHwnd = CreateWindowExW(
        WS_EX_TOOLWINDOW,
        trayWc.lpszClassName,
        L"KuroCatTray",
        WS_POPUP,
        0, 0, 0, 0,
        nullptr, nullptr, hInst, nullptr
    );
    if (app.menuOwnerHwnd) {
        SetPropW(app.menuOwnerHwnd, VERSION_PROP, (HANDLE)(INT_PTR)myVersion);
        // When elevated, UIPI would drop messages from the (non-elevated) taskbar and from a
        // re-launched KuroCat. Explicitly allow the few messages we rely on.
        const UINT allowed[] = { WM_TRAYICON, WM_WAKE_EXISTING, WM_REQUEST_QUIT };
        for (UINT m : allowed) ChangeWindowMessageFilterEx(app.menuOwnerHwnd, m, MSGFLT_ALLOW, nullptr);
        if (g_msgTaskbarCreated) ChangeWindowMessageFilterEx(app.menuOwnerHwnd, g_msgTaskbarCreated, MSGFLT_ALLOW, nullptr);
    }

    int initW = (int)std::round(CatPhysics::CanvasWidth * app.settings.scale);
    int initH = (int)std::round(CatPhysics::CanvasHeight * app.settings.scale);
    int initX = 0, initY = 0;
    if (app.settings.posX != -99999 && app.settings.posY != -99999) {
        initX = app.settings.posX; initY = app.settings.posY;
    } else {
        computeDefaultOrigin(initW, initH, initX, initY);
    }

    DWORD exStyle = WS_EX_LAYERED | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT
                  | (app.settings.obsCaptureMode == 0 ? WS_EX_TOOLWINDOW : 0)
                  | (app.settings.alwaysOnTop ? WS_EX_TOPMOST : 0);

    app.hwnd = CreateWindowExW(
        exStyle,
        wc.lpszClassName,
        L"KuroCat",
        WS_POPUP,
        initX, initY, initW, initH,
        nullptr, nullptr, hInst, nullptr
    );
    if (!app.hwnd) {
        if (app.menuOwnerHwnd) DestroyWindow(app.menuOwnerHwnd);
        Gdiplus::GdiplusShutdown(gdipToken);
        if (hMutex) CloseHandle(hMutex);
        return 1;
    }

    applyWindowScaleAndPosition(app, false);
    DragAcceptFiles(app.hwnd, TRUE);   // drop a .kurocat onto the cat to wear it
    allowDropsWhenElevated(app.hwnd);
    app.input.registerRawInput(app.hwnd);
    app.input.installKeyboardHook();
    if (app.settings.obsCaptureMode > 0 && app.settings.obsHideDesktop) {
        applyOBSMode(app);
    }
    registerTrayIcon(app);

    // Win+L / fast-user-switch notifications (see recoverAfterSystemEvent)
    app.sessionNotifyRegistered = WTSRegisterSessionNotification(app.hwnd, NOTIFY_FOR_THIS_SESSION) != FALSE;
    // Foreground changes (games launching) -> re-assert topmost
    app.foregroundHook = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, nullptr,
                                         onForegroundChanged, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);

    // Global shortcut Ctrl+Alt+Shift+P to toggle full click-through anytime!
    if (app.menuOwnerHwnd) {
        RegisterHotKey(app.menuOwnerHwnd, HOTKEY_CLICKTHROUGH_ID, MOD_CONTROL | MOD_ALT | MOD_SHIFT | MOD_NOREPEAT, 'P');
        startAsyncUpdateCheck(app.menuOwnerHwnd, false, 2500);
    }

    ShowWindow(app.hwnd, SW_SHOWNOACTIVATE);
    app.lastTickTime = nowSeconds();
    tickAndRender(app);
    app.nextFrameTime = nowSeconds() + FRAME_INTERVAL;
    SetTimer(app.hwnd, TIMER_RENDER, 16, nullptr);

    // Frame loop. WM_TIMER is only generated when the queue is empty, so a 1000-8000 Hz gaming mouse
    // (one WM_INPUT per report) could starve it and freeze the cat. Here frames are driven by time:
    // drain messages for at most ~10 ms, then render if a frame is due. WM_TIMER still calls
    // renderIfDue() so the cat keeps animating inside menus / message boxes (their own modal loops).
    MSG msg = {};
    int exitCode = 0;
    bool running = true;
    while (running) {
        double now = nowSeconds();
        double waitSec = (app.isVisible && !app.hiddenByFullscreen) ? (app.nextFrameTime - now) : 0.25;
        if (waitSec > 0.0) {
            MsgWaitForMultipleObjectsEx(0, nullptr, (DWORD)(waitSec * 1000.0), QS_ALLINPUT, MWMO_INPUTAVAILABLE);
        }
        double drainUntil = nowSeconds() + 0.010;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                running = false;
                exitCode = (int)msg.wParam;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (nowSeconds() > drainUntil) break;
        }
        if (!running) break;
        renderIfDue(app);
        housekeeping(app);
    }

    g_app = nullptr;
    Gdiplus::GdiplusShutdown(gdipToken);
    if (hMutex) CloseHandle(hMutex);
    return exitCode;
}
