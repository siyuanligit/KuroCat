#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <X11/Xatom.h>
#ifdef None
#undef None
#endif
#include <cairo.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <limits.h>
#include <string>
#include <thread>
#include <vector>
#include <algorithm>
#include <unistd.h>
#include "KeyMap.hpp"
#include "InputEngine.hpp"
#include "CatPhysics.hpp"
#include "CatRenderer.hpp"
#include "SkinLibrary.hpp"

#include <sys/stat.h>
#include <sys/file.h>
#include <sys/types.h>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <csignal>
#include <cerrno>
#include <dirent.h>
#include <fcntl.h>
#include <glib-unix.h>

#define KUROCAT_VERSION_STR "1.4.3"
static constexpr const char* APP_VERSION = KUROCAT_VERSION_STR;
// Other KuroCat builds scan installed binaries for this marker (see readBinaryVersion) to find the
// newest copy WITHOUT executing it. Always derived from KUROCAT_VERSION_STR.
__attribute__((used)) static const char kVersionMarker[] = "KUROCAT_VERSION=" KUROCAT_VERSION_STR;
static constexpr const char* UPDATE_PAGE_URL = "https://kurocat-download.app.workbuddy.host/?sharecode=JVAABIVZ7L";
#if defined(__aarch64__) || defined(_M_ARM64)
static constexpr const char* UPDATE_TAR_URL = "https://kurocat-download.app.workbuddy.host/downloads/KuroCat-Linux-arm64.tar.gz?sharecode=JVAABIVZ7L";
#else
static constexpr const char* UPDATE_TAR_URL = "https://kurocat-download.app.workbuddy.host/downloads/KuroCat-Linux-x86_64.tar.gz?sharecode=JVAABIVZ7L";
#endif

struct LinuxApp {
    GtkWidget* window = nullptr;
    GtkWidget* drawArea = nullptr;
    G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    GtkStatusIcon* trayIcon = nullptr;
    G_GNUC_END_IGNORE_DEPRECATIONS

    CatSettings settings;
    InputEngine input;
    CatPhysics physics;
    SkinLibrary skins;
    KeyboardSkinLibrary kbLibrary;
    GtkWidget* dressingRoom = nullptr;   // 「换装间」 import window (nullptr when closed)
    GtkWidget* dressingStatus = nullptr;
    GtkWidget* dressingSubtitle = nullptr;
    GtkWidget* dressingPreview = nullptr;
    GtkWidget* dressingDrop = nullptr;
    bool dropHover = false;

    GtkWidget* settingsWindow = nullptr;
    GtkWidget* setCoatCombo = nullptr;
    GtkWidget* setEyeCombo = nullptr;
    GtkWidget* setKbCombo = nullptr;
    GtkWidget* setScaleSlider = nullptr;
    GtkWidget* setScaleLabel = nullptr;
    GtkWidget* setRotSlider = nullptr;
    GtkWidget* setRotLabel = nullptr;
    GtkWidget* setFlipChk = nullptr;
    GtkWidget* setOutlineChk = nullptr;
    GtkWidget* setAngerChk = nullptr;
    GtkWidget* setPetChk = nullptr;
    GtkWidget* setBadgesChk = nullptr;
    GtkWidget* setLegendsChk = nullptr;
    GtkWidget* setMarqueeChk = nullptr;
    GtkWidget* setEasterChk = nullptr;
    GtkWidget* setTabletCombo = nullptr;
    GtkWidget* setTopmostChk = nullptr;
    GtkWidget* setAutohideChk = nullptr;
    GtkWidget* setClickThroughChk = nullptr;
    GtkWidget* setAutostartChk = nullptr;
    GtkWidget* setHighFpsChk = nullptr;
    GtkWidget* setObsCombo = nullptr;
    GtkWidget* setObsHideChk = nullptr;
    GtkWidget* setLangCombo = nullptr;
    bool settingsUpdating = false;

    bool isVisible = true;
    bool hiddenByFullscreen = false;
    double lastFullscreenCheck = 0.0;
    bool isDragging = false;
    int activeResizeCorner = -1;
    double resizeAnchorX = 0.0, resizeAnchorY = 0.0;
    bool isRotating = false;
    bool rotateMoved = false;
    double rotateStartAngle = 0.0;
    float rotateStartDeg = 0.0f;
    bool clickThroughTemporarilyUnlocked = false;
    double dragStartRootX = 0.0, dragStartRootY = 0.0;
    double lastDragRootX = 0.0;
    double lastDragRootY = 0.0;
    int dragStartWinX = 0, dragStartWinY = 0;
    bool hasSavedObsPos = false;
    int savedObsPosX = 0, savedObsPosY = 0;

    bool hasLastHover = false;
    Pt lastHoverCanvas = { 0.0f, 0.0f };
    double lastTickTime = 0.0;

    bool demoActive = false;
    double demoStartTime = 0.0;
    int demoStepIdx = 0;

    std::string availableUpdateVersion;
    std::string availableUpdateUrl;
    bool isUpdating = false;

    // Cached window geometry (updated from configure-event). Querying it from the X server every
    // frame cost several synchronous round-trips per frame.
    int winX = 0, winY = 0, winW = 360, winH = 320;
    double lastGeometryRefresh = 0.0;
};

static LinuxApp* g_app = nullptr;
static int g_lockFd = -1;
static guint g_frameIntervalMs = 0;

// MARK: - Versions of installed copies / single instance

static int versionToNumber(const std::string& v) {
    int parts[3] = { 0, 0, 0 };
    int idx = 0;
    for (char c : v) {
        if (c >= '0' && c <= '9') parts[idx] = parts[idx] * 10 + (c - '0');
        else if (c == '.') { if (++idx >= 3) break; }
        else if (c != 'v' && c != 'V') break;
    }
    return parts[0] * 10000 + parts[1] * 100 + parts[2];
}

/// Reads the version baked into a kurocat binary without running it (0 = unknown / not kurocat).
/// New builds carry "KUROCAT_VERSION=x.y.z"; builds up to 1.3.2 only contain "KuroCat/x.y.z"
/// (their HTTP User-Agent).
static int readBinaryVersion(const std::string& path) {
    struct stat st{};
    if (stat(path.c_str(), &st) != 0 || !S_ISREG(st.st_mode) || st.st_size <= 0 || st.st_size > 64 * 1024 * 1024) return 0;
    std::ifstream f(path, std::ios::binary);
    if (!f) return 0;
    std::string data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    auto scan = [&](const std::string& needle) -> int {
        size_t pos = 0;
        while ((pos = data.find(needle, pos)) != std::string::npos) {
            size_t p = pos + needle.size();
            if (p < data.size() && std::isdigit((unsigned char)data[p])) {
                size_t e = p;
                while (e < data.size() && (std::isdigit((unsigned char)data[e]) || data[e] == '.')) e++;
                return versionToNumber(data.substr(p, e - p));
            }
            pos = p;
        }
        return 0;
    };
    int v = scan(std::string("KUROCAT_") + "VERSION=");
    if (v == 0) v = scan(std::string("KuroCat") + "/");
    return v;
}

static std::string homeDir() {
    const char* home = std::getenv("HOME");
    return std::string(home && home[0] ? home : "/tmp");
}

static std::string userLocalBinPath() {
    return homeDir() + "/.local/bin/kurocat";
}

static std::string resolvedPath(const std::string& p) {
    char buf[PATH_MAX] = {0};
    if (realpath(p.c_str(), buf)) return std::string(buf);
    return p;
}

/// Password-free updates install into ~/.local/bin while a .deb copy may still sit in /usr/bin
/// (and vice-versa after a later .deb upgrade). Whatever launcher was used — menu entry,
/// autostart, terminal — always run the NEWEST installed copy, so an old copy can't keep
/// popping up "new version available" after the user already updated.
static void redirectToNewestInstalledCopy(char** argv) {
    char selfBuf[PATH_MAX] = {0};
    ssize_t n = readlink("/proc/self/exe", selfBuf, sizeof(selfBuf) - 1);
    std::string self = n > 0 ? std::string(selfBuf, (size_t)n) : std::string();
    int best = versionToNumber(APP_VERSION);
    std::string bestPath;
    const std::string candidates[] = { userLocalBinPath(), "/usr/bin/kurocat", "/usr/local/bin/kurocat" };
    for (const auto& c : candidates) {
        if (access(c.c_str(), X_OK) != 0) continue;
        std::string real = resolvedPath(c);
        if (real == self) continue;
        int v = readBinaryVersion(real);
        if (v > best) { best = v; bestPath = real; }
    }
    if (bestPath.empty()) return;
    setenv("KUROCAT_NO_REDIRECT", "1", 1);
    execv(bestPath.c_str(), argv);
    // exec failed: keep running this copy
}

static std::string lockFilePath() {
    const char* rt = std::getenv("XDG_RUNTIME_DIR");
    std::string dir = (rt && rt[0] && access(rt, W_OK) == 0) ? std::string(rt) : std::string("/tmp");
    return dir + "/kurocat-" + std::to_string((long)getuid()) + ".lock";
}

static bool waitForPidExit(pid_t pid, int timeoutMs) {
    for (int waited = 0; waited <= timeoutMs; waited += 100) {
        if (kill(pid, 0) != 0 && errno == ESRCH) return true;
        usleep(100 * 1000);
    }
    return false;
}

/// Builds <= 1.3.2 had no single-instance lock at all, so a freshly installed version would run
/// NEXT to the old one (two cats, and the old one still nagging about updates). Find them via
/// /proc (same user, process name "kurocat") and stop them.
static void terminateLegacyInstances() {
    pid_t self = getpid();
    uid_t uid = getuid();
    std::vector<pid_t> victims;
    if (DIR* d = opendir("/proc")) {
        while (struct dirent* ent = readdir(d)) {
            if (!std::isdigit((unsigned char)ent->d_name[0])) continue;
            pid_t pid = (pid_t)std::atol(ent->d_name);
            if (pid <= 1 || pid == self) continue;
            std::string base = std::string("/proc/") + ent->d_name;
            struct stat st{};
            if (stat(base.c_str(), &st) != 0 || st.st_uid != uid) continue;
            std::ifstream comm(base + "/comm");
            std::string name;
            std::getline(comm, name);
            if (name == "kurocat") victims.push_back(pid);
        }
        closedir(d);
    }
    for (pid_t v : victims) kill(v, SIGTERM);
    for (pid_t v : victims) {
        if (!waitForPidExit(v, 3000)) kill(v, SIGKILL);
    }
}

static bool claimLock(int fd) {
    if (flock(fd, LOCK_EX | LOCK_NB) != 0) return false;
    std::string content = std::to_string((long)getpid()) + " " + std::to_string(versionToNumber(APP_VERSION)) + "\n";
    if (ftruncate(fd, 0) == 0) {
        ssize_t w = pwrite(fd, content.data(), content.size(), 0);
        (void)w;
    }
    g_lockFd = fd;
    return true;
}

/// Returns false when this process should exit because an equal/newer KuroCat is already running
/// (that one is woken up instead: shown again, click-through cancelled, menu opened).
static bool acquireSingleInstance() {
    int fd = open(lockFilePath().c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0600);
    if (fd < 0) return true; // can't lock (odd /tmp setup): never block startup over it
    if (claimLock(fd)) {
        terminateLegacyInstances();
        return true;
    }
    char buf[64] = {0};
    ssize_t r = pread(fd, buf, sizeof(buf) - 1, 0);
    (void)r;
    long otherPid = 0;
    int otherVer = 0;
    std::sscanf(buf, "%ld %d", &otherPid, &otherVer);
    if (otherPid <= 0 || otherVer >= versionToNumber(APP_VERSION)) {
        if (otherPid > 0) kill((pid_t)otherPid, SIGUSR1);
        close(fd);
        return false;
    }
    kill((pid_t)otherPid, SIGTERM); // older KuroCat: replace it
    for (int i = 0; i < 50; ++i) {
        usleep(100 * 1000);
        if (claimLock(fd)) {
            terminateLegacyInstances();
            return true;
        }
    }
    kill((pid_t)otherPid, SIGUSR1);
    close(fd);
    return false;
}

static void releaseSingleInstance() {
    if (g_lockFd >= 0) {
        close(g_lockFd);
        g_lockFd = -1;
    }
}

// MARK: - Launch at Login (Freedesktop XDG Autostart: Ubuntu/Debian/Deepin/统信 UOS/KDE/GNOME)

static std::string getAutostartDir() {
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    if (xdg && xdg[0] != '\0') return std::string(xdg) + "/autostart";
    const char* home = std::getenv("HOME");
    return std::string(home ? home : "/tmp") + "/.config/autostart";
}

static std::string getAutostartFilePath() {
    return getAutostartDir() + "/kurocat.desktop";
}

static std::string getSelfExePath() {
    char buf[PATH_MAX] = {0};
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len > 0) {
        buf[len] = '\0';
        return std::string(buf);
    }
    return "kurocat";
}

static bool isAutoStartEnabled() {
    return access(getAutostartFilePath().c_str(), F_OK) == 0;
}

static bool setAutoStartEnabled(bool enable) {
    std::string path = getAutostartFilePath();
    if (!enable) {
        std::remove(path.c_str());
        return true;
    }
    std::string dir = getAutostartDir();
    g_mkdir_with_parents(dir.c_str(), 0755);
    std::string exe = getSelfExePath();
    std::ofstream f(path);
    if (!f.is_open()) return false;
    f << "[Desktop Entry]\n"
      << "Type=Application\n"
      << "Name=KuroCat\n"
      << "Name[zh_CN]=KuroCat 黑猫桌宠\n"
      << "Comment=Native desktop keyboard & mouse cat pet\n"
      << "Exec=\"" << exe << "\"\n"
      << "Icon=kurocat\n"
      << "Terminal=false\n"
      << "X-GNOME-Autostart-enabled=true\n";
    return f.good();
}

static const SkinSpec& activeSkin(const LinuxApp& app);

static void updateInputShape(LinuxApp& app) {
    if (!app.window || !gtk_widget_get_realized(app.window)) return;
    if (app.settings.isEditMode) {
        gtk_widget_input_shape_combine_region(app.window, nullptr);
        return;
    }
    if ((app.settings.fullClickThrough && !app.clickThroughTemporarilyUnlocked)
        || (app.settings.obsCaptureMode > 0 && app.settings.obsHideDesktop)) {
        cairo_region_t* empty = cairo_region_create();
        gtk_widget_input_shape_combine_region(app.window, empty);
        cairo_region_destroy(empty);
        return;
    }

    // Smart per-area input mask: transparent corners pass clicks straight through,
    // while the cat head/body & mini keyboard remain interactive for petting, dragging & right-click!
    float s = app.settings.scale;
    cairo_region_t* reg = cairo_region_create();
    if (std::fabs(app.settings.rotationDeg) > 1.0f) {
        cairo_rectangle_int_t rotBox = { (int)(28 * s), (int)(24 * s), (int)(304 * s), (int)(272 * s) };
        cairo_region_union_rectangle(reg, &rotBox);
    } else if (activeSkin(app).renderStyle == SkinRenderStyle::Douni) {
        int headX = app.settings.flipHorizontal ? (int)(32 * s) : (int)(168 * s);
        cairo_rectangle_int_t headBox = { headX, (int)(64 * s), (int)(160 * s), (int)(120 * s) };
        cairo_rectangle_int_t bodyBox = { (int)(20 * s), (int)(116 * s), (int)(320 * s), (int)(168 * s) };
        cairo_region_union_rectangle(reg, &headBox);
        cairo_region_union_rectangle(reg, &bodyBox);
    } else {
        cairo_rectangle_int_t catBox = {
            (int)(128 * s), (int)(72 * s), (int)(116 * s), (int)(172 * s)
        };
        cairo_rectangle_int_t kbBox = {
            (int)(56 * s), (int)(230 * s), (int)(242 * s), (int)(72 * s)
        };
        cairo_region_union_rectangle(reg, &catBox);
        cairo_region_union_rectangle(reg, &kbBox);
    }
    gtk_widget_input_shape_combine_region(app.window, reg);
    cairo_region_destroy(reg);
}

static void applyOBSMode(LinuxApp& app) {
    if (!app.window) return;
    bool obsOn = (app.settings.obsCaptureMode > 0);
    bool hideDesk = (obsOn && app.settings.obsHideDesktop);
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(app.window), !obsOn);
    gtk_window_set_skip_pager_hint(GTK_WINDOW(app.window), !obsOn);
    if (hideDesk) {
        if (!app.hasSavedObsPos) {
            gtk_window_get_position(GTK_WINDOW(app.window), &app.savedObsPosX, &app.savedObsPosY);
            app.hasSavedObsPos = true;
        }
        gtk_window_set_keep_above(GTK_WINDOW(app.window), FALSE);
        gtk_window_set_keep_below(GTK_WINDOW(app.window), TRUE);
        if (GdkWindow* gw = gtk_widget_get_window(app.window)) gdk_window_lower(gw);
        gtk_window_move(GTK_WINDOW(app.window), -10000, -10000);
        if (app.isVisible) gtk_widget_show(app.window);
    } else {
        gtk_window_set_keep_below(GTK_WINDOW(app.window), FALSE);
        gtk_window_set_keep_above(GTK_WINDOW(app.window), app.settings.alwaysOnTop ? TRUE : FALSE);
        if (app.hasSavedObsPos) {
            gtk_window_move(GTK_WINDOW(app.window), app.savedObsPosX, app.savedObsPosY);
            app.hasSavedObsPos = false;
        }
    }
    updateInputShape(app);
    gtk_widget_queue_draw(app.drawArea);
}

static bool isOtherWindowFullscreen(LinuxApp& app) {
    GdkDisplay* gdkDpy = gdk_display_get_default();
    if (!gdkDpy || !GDK_IS_X11_DISPLAY(gdkDpy)) return false;
    Display* dpy = gdk_x11_display_get_xdisplay(gdkDpy);
    if (!dpy) return false;
    Atom netActive = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", True);
    Atom netWmState = XInternAtom(dpy, "_NET_WM_STATE", True);
    Atom netWmFs = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", True);
    if (!netActive || !netWmState || !netWmFs) return false;

    Atom actualType = 0;
    int actualFmt = 0;
    unsigned long nitems = 0, bytesAfter = 0;
    unsigned char* prop = nullptr;
    if (XGetWindowProperty(dpy, DefaultRootWindow(dpy), netActive, 0, 1, False, XA_WINDOW,
                           &actualType, &actualFmt, &nitems, &bytesAfter, &prop) != Success || !prop || nitems == 0) {
        if (prop) XFree(prop);
        return false;
    }
    Window activeWin = *(Window*)prop;
    XFree(prop);
    if (!activeWin) return false;
    if (app.window) {
        if (GdkWindow* gw = gtk_widget_get_window(app.window)) {
            if (GDK_IS_X11_WINDOW(gw) && activeWin == gdk_x11_window_get_xid(gw)) return false;
        }
    }

    Atom stType = 0;
    int stFmt = 0;
    unsigned long stItems = 0, stAfter = 0;
    unsigned char* stProp = nullptr;
    if (XGetWindowProperty(dpy, activeWin, netWmState, 0, 32, False, XA_ATOM,
                           &stType, &stFmt, &stItems, &stAfter, &stProp) != Success || !stProp) {
        return false;
    }
    bool isFs = false;
    Atom* atoms = (Atom*)stProp;
    for (unsigned long i = 0; i < stItems; ++i) {
        if (atoms[i] == netWmFs) { isFs = true; break; }
    }
    XFree(stProp);
    return isFs;
}

static void getDefaultOrigin(int w, int h, int& outX, int& outY) {
    GdkDisplay* dpy = gdk_display_get_default();
    GdkMonitor* mon = dpy ? gdk_display_get_primary_monitor(dpy) : nullptr;
    if (!mon && dpy) mon = gdk_display_get_monitor(dpy, 0);
    GdkRectangle geo = { 0, 0, 1440, 900 };
    if (mon) gdk_monitor_get_workarea(mon, &geo);
    outX = geo.x + geo.width - w - 24;
    outY = geo.y + geo.height - h - 16;
}

static void applyWindowScaleAndPos(LinuxApp& app, bool anchorBottomCenter) {
    int newW = (int)std::round(CatPhysics::CanvasWidth * app.settings.scale);
    int newH = (int)std::round(CatPhysics::CanvasHeight * app.settings.scale);

    int x = app.settings.posX;
    int y = app.settings.posY;
    if (anchorBottomCenter && app.window) {
        int oldX = 0, oldY = 0, oldW = 0, oldH = 0;
        gtk_window_get_position(GTK_WINDOW(app.window), &oldX, &oldY);
        gtk_window_get_size(GTK_WINDOW(app.window), &oldW, &oldH);
        x = oldX + oldW / 2 - newW / 2;
        y = oldY + oldH - newH;
    } else if (x == -99999 || y == -99999) {
        getDefaultOrigin(newW, newH, x, y);
    }

    app.settings.posX = x;
    app.settings.posY = y;
    gtk_window_resize(GTK_WINDOW(app.window), newW, newH);
    gtk_window_move(GTK_WINDOW(app.window), x, y);
    gtk_window_set_keep_above(GTK_WINDOW(app.window), app.settings.alwaysOnTop ? TRUE : FALSE);
    updateInputShape(app);
    app.settings.save();
}

static void openUrlInBrowser(const std::string& url) {
    pid_t pid = fork();
    if (pid == 0) {
        execlp("xdg-open", "xdg-open", url.c_str(), nullptr);
        _exit(127);
    }
}

struct LinuxUpdateResult {
    bool ok = false;
    bool manual = false;
    bool hasUpdate = false;
    std::string latestTag;
    std::string releaseUrl;
};

static std::string extractJsonField(const std::string& json, const char* fieldName) {
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

static bool isSemVerNewer(const std::string& lhs, const std::string& rhs) {
    auto parse = [](const std::string& v) {
        std::vector<int> parts;
        size_t i = 0;
        while (i < v.size() && (v[i] == 'v' || v[i] == 'V' || v[i] == ' ')) i++;
        int cur = 0; bool inNum = false;
        for (; i < v.size(); ++i) {
            if (v[i] >= '0' && v[i] <= '9') { cur = cur * 10 + (v[i] - '0'); inNum = true; }
            else if (v[i] == '.') { parts.push_back(inNum ? cur : 0); cur = 0; inNum = false; }
            else break;
        }
        if (inNum) parts.push_back(cur);
        return parts;
    };
    auto a = parse(lhs), b = parse(rhs);
    size_t n = std::max(a.size(), b.size());
    for (size_t i = 0; i < n; ++i) {
        int va = i < a.size() ? a[i] : 0;
        int vb = i < b.size() ? b[i] : 0;
        if (va != vb) return va > vb;
    }
    return false;
}

static std::string extractVersionFromHtml(const std::string& html) {
    auto parseVerAt = [&](size_t pos) -> std::string {
        if (pos >= html.size() || (html[pos] != 'v' && html[pos] != 'V')) return "";
        size_t i = pos + 1;
        int dots = 0;
        std::string ver = "v";
        while (i < html.size()) {
            char c = html[i];
            if (c >= '0' && c <= '9') { ver.push_back(c); }
            else if (c == '.') { dots++; ver.push_back(c); }
            else break;
            i++;
        }
        return (dots >= 2 && ver.size() > 5) ? ver : "";
    };

    size_t mPos = html.find("已发布");
    if (mPos != std::string::npos) {
        size_t searchStart = (mPos > 32) ? (mPos - 32) : 0;
        for (size_t i = mPos; i > searchStart; --i) {
            if (html[i - 1] == 'v' || html[i - 1] == 'V') {
                std::string v = parseVerAt(i - 1);
                if (!v.empty()) return v;
            }
        }
    }
    size_t vPos = html.find("版本 ");
    if (vPos != std::string::npos) {
        std::string v = parseVerAt(vPos + 7);
        if (!v.empty()) return v;
    }
    return "";
}

static std::string fetchUrlBody(const char* url) {
    std::string cmd = std::string("curl -fsSL --max-time 8 -A KuroCat/") + APP_VERSION + " '" + url +
        "' 2>/dev/null || wget -qO- --timeout=8 '" + url + "' 2>/dev/null";
    std::string body;
    if (FILE* fp = popen(cmd.c_str(), "r")) {
        char buf[1024];
        while (size_t n = std::fread(buf, 1, sizeof(buf), fp)) body.append(buf, n);
        pclose(fp);
    }
    return body;
}

static void startInAppUpdate();

static void startAsyncUpdateCheck(bool manual, int delayMs = 0) {
    std::thread([manual, delayMs]() {
        if (delayMs > 0) usleep((useconds_t)delayMs * 1000);
        auto* res = new LinuxUpdateResult();
        res->manual = manual;

        std::string body = fetchUrlBody("https://kurocat-download.app.workbuddy.host/version.json?sharecode=JVAABIVZ7L");
        std::string tag = extractJsonField(body, "tag_name");
        std::string url = extractJsonField(body, "html_url");

        if (tag.empty()) {
            std::string html = fetchUrlBody(UPDATE_PAGE_URL);
            tag = extractVersionFromHtml(html);
        }

        if (!tag.empty()) {
            res->ok = true;
            res->latestTag = tag;
            res->releaseUrl = url.empty() ? UPDATE_PAGE_URL : url;
            res->hasUpdate = isSemVerNewer(tag, APP_VERSION);
        }

        g_idle_add(+[](gpointer data) -> gboolean {
            auto* r = static_cast<LinuxUpdateResult*>(data);
            if (g_app && r->ok && r->hasUpdate) {
                g_app->availableUpdateVersion = r->latestTag;
                g_app->availableUpdateUrl = r->releaseUrl;
                GtkWidget* dlg = gtk_message_dialog_new(
                    nullptr, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_YES_NO,
                    "发现 KuroCat 新版本 %s", r->latestTag.c_str());
                gtk_message_dialog_format_secondary_text(
                    GTK_MESSAGE_DIALOG(dlg),
                    "当前版本：v%s\n最新版本：%s\n\n是否立即在软件内自动下载并更新重启？",
                    APP_VERSION, r->latestTag.c_str());
                int resp = gtk_dialog_run(GTK_DIALOG(dlg));
                gtk_widget_destroy(dlg);
                if (resp == GTK_RESPONSE_YES) {
                    startInAppUpdate();
                }
            } else if (r->manual) {
                GtkWidget* dlg = gtk_message_dialog_new(
                    nullptr, GTK_DIALOG_MODAL,
                    r->ok ? GTK_MESSAGE_INFO : GTK_MESSAGE_WARNING, GTK_BUTTONS_OK,
                    "%s", r->ok ? "已是最新版本" : "检查更新失败");
                gtk_message_dialog_format_secondary_text(
                    GTK_MESSAGE_DIALOG(dlg),
                    "%s", r->ok ? "当前版本已是最新版本。" : "无法连接到更新服务器，请稍后重试或检查网络连接。");
                gtk_dialog_run(GTK_DIALOG(dlg));
                gtk_widget_destroy(dlg);
            }
            delete r;
            return G_SOURCE_REMOVE;
        }, res);
    }).detach();
}

static std::string shQuote(const std::string& s) {
    std::string out = "'";
    for (char c : s) {
        if (c == '\'') out += "'\\''";
        else out += c;
    }
    return out + "'";
}

/// Writes a per-user launcher (~/.local/share/applications overrides /usr/share/applications for
/// the same file name) so the app menu starts the per-user copy after a password-free update.
static void writeUserDesktopEntry(const std::string& exePath) {
    std::string dir = homeDir() + "/.local/share/applications";
    g_mkdir_with_parents(dir.c_str(), 0755);
    std::ofstream f(dir + "/kurocat.desktop");
    if (!f.is_open()) return;
    f << "[Desktop Entry]\n"
      << "Name=KuroCat\n"
      << "Name[zh_CN]=KuroCat 黑猫键盘桌宠\n"
      << "Comment=Full-state keyboard & mouse desktop cat pet\n"
      << "Comment[zh_CN]=打字/游戏全状态监听键盘小黑猫桌宠\n"
      << "Exec=\"" << exePath << "\"\n"
      << "Icon=kurocat\n"
      << "Terminal=false\n"
      << "Type=Application\n"
      << "Categories=Utility;Amusement;\n"
      << "StartupNotify=false\n";
}

static void startInAppUpdate() {
    if (!g_app || g_app->isUpdating) return;
    g_app->isUpdating = true;

    std::thread([]() {
        bool ok = false;
        char exeBuf[PATH_MAX] = {};
        ssize_t exeLen = readlink("/proc/self/exe", exeBuf, sizeof(exeBuf) - 1);
        std::string exePath = (exeLen > 0) ? std::string(exeBuf, exeLen) : "";
        const std::string deletedSuffix = " (deleted)"; // binary replaced on disk while running (e.g. .deb upgrade)
        if (exePath.size() > deletedSuffix.size() &&
            exePath.compare(exePath.size() - deletedSuffix.size(), deletedSuffix.size(), deletedSuffix) == 0) {
            exePath.erase(exePath.size() - deletedSuffix.size());
        }
        std::string targetPath = exePath;

        std::string workDir = "/tmp/kurocat_upd_" + std::to_string((long)getpid());
        std::string tarPath = workDir + "/update.tar.gz";
        std::string newBin = workDir + "/kurocat";

        if (!exePath.empty()) {
            std::string dlCmd = "mkdir -p " + shQuote(workDir) + " && (curl -fsSL --max-time 60 -o " + shQuote(tarPath) +
                " " + shQuote(UPDATE_TAR_URL) + " || wget -qO " + shQuote(tarPath) + " --timeout=60 " + shQuote(UPDATE_TAR_URL) +
                ") && tar -xzf " + shQuote(tarPath) + " -C " + shQuote(workDir);
            if (std::system(dlCmd.c_str()) == 0) {
                struct stat st{};
                if (stat(newBin.c_str(), &st) == 0 && st.st_size > 10000) {
                    size_t slash = exePath.rfind('/');
                    std::string exeDir = (slash != std::string::npos) ? exePath.substr(0, slash) : ".";
                    bool perUser = (access(exeDir.c_str(), W_OK) != 0);
                    if (perUser) {
                        // System-wide install (.deb in /usr/bin): previously this asked for the login
                        // password via pkexec. Install for this user instead — no password needed —
                        // and redirectToNewestInstalledCopy() makes every launcher pick it up.
                        targetPath = userLocalBinPath();
                    }
                    std::string targetDir = targetPath.substr(0, targetPath.rfind('/'));
                    std::string tmpTarget = targetPath + ".new";
                    std::string installCmd = "mkdir -p " + shQuote(targetDir) +
                        " && cp -f " + shQuote(newBin) + " " + shQuote(tmpTarget) +
                        " && chmod 755 " + shQuote(tmpTarget) +
                        " && mv -f " + shQuote(tmpTarget) + " " + shQuote(targetPath);
                    if (std::system(installCmd.c_str()) == 0) {
                        ok = true;
                        if (perUser) {
                            writeUserDesktopEntry(targetPath);
                            std::string iconDir = homeDir() + "/.local/share/icons/hicolor/scalable/apps";
                            std::string iconCmd = "[ -f " + shQuote(workDir + "/kurocat.svg") + " ] && mkdir -p " + shQuote(iconDir) +
                                " && cp -f " + shQuote(workDir + "/kurocat.svg") + " " + shQuote(iconDir + "/kurocat.svg");
                            (void)std::system(iconCmd.c_str());
                        }
                    }
                }
            }
        }
        std::string cleanupCmd = "rm -rf " + shQuote(workDir);
        (void)std::system(cleanupCmd.c_str());

        auto* res = new std::pair<bool, std::string>(ok, targetPath);
        g_idle_add(+[](gpointer data) -> gboolean {
            auto* p = static_cast<std::pair<bool, std::string>*>(data);
            if (g_app) g_app->isUpdating = false;
            if (p->first && !p->second.empty()) {
                // Hand the single-instance lock to the new binary before it starts.
                releaseSingleInstance();
                pid_t pid = fork();
                if (pid == 0) {
                    execl(p->second.c_str(), p->second.c_str(), nullptr);
                    _exit(127);
                }
                gtk_main_quit();
            } else {
                GtkWidget* dlg = gtk_message_dialog_new(
                    nullptr, GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING, GTK_BUTTONS_YES_NO,
                    "自动更新失败");
                gtk_message_dialog_format_secondary_text(
                    GTK_MESSAGE_DIALOG(dlg),
                    "自动下载或替换安装包失败，是否打开下载页手动下载？");
                int resp = gtk_dialog_run(GTK_DIALOG(dlg));
                gtk_widget_destroy(dlg);
                if (resp == GTK_RESPONSE_YES) {
                    std::string url = (g_app && !g_app->availableUpdateUrl.empty())
                        ? g_app->availableUpdateUrl : UPDATE_PAGE_URL;
                    openUrlInBrowser(url);
                }
            }
            delete p;
            return G_SOURCE_REMOVE;
        }, res);
    }).detach();
}

static GdkPixbuf* createCatTrayPixbuf() {
    constexpr int sz = 32;
    cairo_surface_t* surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, sz, sz);
    cairo_t* cr = cairo_create(surf);
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    // Matte black cat head silhouette
    cairo_set_source_rgba(cr, 0.085, 0.085, 0.10, 1.0);
    cairo_move_to(cr, 4, 15); cairo_line_to(cr, 6, 3); cairo_line_to(cr, 14, 11); cairo_close_path(cr); cairo_fill(cr);
    cairo_move_to(cr, 28, 15); cairo_line_to(cr, 26, 3); cairo_line_to(cr, 18, 11); cairo_close_path(cr); cairo_fill(cr);
    cairo_save(cr); cairo_translate(cr, 16, 18.5); cairo_scale(cr, 12.0, 10.5); cairo_arc(cr, 0, 0, 1.0, 0, 6.2832); cairo_restore(cr); cairo_fill(cr);

    // Chartreuse glowing eyes
    cairo_set_source_rgba(cr, 0.79, 0.95, 0.25, 1.0);
    cairo_save(cr); cairo_translate(cr, 11.5, 17.2); cairo_scale(cr, 3.0, 2.6); cairo_arc(cr, 0, 0, 1.0, 0, 6.2832); cairo_restore(cr); cairo_fill(cr);
    cairo_save(cr); cairo_translate(cr, 20.5, 17.2); cairo_scale(cr, 3.0, 2.6); cairo_arc(cr, 0, 0, 1.0, 0, 6.2832); cairo_restore(cr); cairo_fill(cr);

    cairo_set_source_rgba(cr, 0.08, 0.08, 0.10, 1.0);
    cairo_save(cr); cairo_translate(cr, 11.5, 17.2); cairo_scale(cr, 0.9, 1.9); cairo_arc(cr, 0, 0, 1.0, 0, 6.2832); cairo_restore(cr); cairo_fill(cr);
    cairo_save(cr); cairo_translate(cr, 20.5, 17.2); cairo_scale(cr, 0.9, 1.9); cairo_arc(cr, 0, 0, 1.0, 0, 6.2832); cairo_restore(cr); cairo_fill(cr);

    GdkPixbuf* pb = gdk_pixbuf_get_from_surface(surf, 0, 0, sz, sz);
    cairo_destroy(cr);
    cairo_surface_destroy(surf);
    return pb;
}

// MARK: - Skins (built-in coats + imported .kurocat files)

static const SkinSpec& activeSkin(const LinuxApp& app) {
    if (!app.settings.customSkin.empty())
        if (const SkinSpec* s = app.skins.find(app.settings.customSkin)) return *s;
    return builtInSkin(app.settings.coatTheme);
}

static KeyboardSkinSpec activeKeyboardSkin(const LinuxApp& app) {
    if (!app.settings.customKeyboard.empty())
        if (const KeyboardSkinSpec* s = app.kbLibrary.find(app.settings.customKeyboard)) return *s;
    return KeyboardSkinSpec::builtInTheme((int)app.settings.keyboardTheme);
}

static bool hasSuffixCI(const std::string& s, const char* suffix) {
    size_t slen = std::strlen(suffix);
    if (s.size() < slen) return false;
    for (size_t i = 0; i < slen; ++i) {
        if (std::tolower((unsigned char)s[s.size() - slen + i]) != std::tolower((unsigned char)suffix[i])) return false;
    }
    return true;
}

static void showMessage(GtkMessageType type, const char* title, const std::string& detail) {
    GtkWindow* parent = g_app && g_app->dressingRoom ? GTK_WINDOW(g_app->dressingRoom) : nullptr;
    GtkWidget* dlg = gtk_message_dialog_new(parent, GTK_DIALOG_MODAL, type, GTK_BUTTONS_NONE, "%s", title);
    if (!detail.empty()) gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dlg), "%s", detail.c_str());
    gtk_dialog_add_button(GTK_DIALOG(dlg), "知道了", GTK_RESPONSE_OK);
    gtk_window_set_title(GTK_WINDOW(dlg), "KuroCat 换装间");
    gtk_window_set_keep_above(GTK_WINDOW(dlg), TRUE);
    gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg);
}

static void refreshDressingRoom(LinuxApp& app);
static void selectCustomSkin(LinuxApp& app, const std::string& id) {
    app.settings.customSkin = id;
    app.settings.save();
    updateInputShape(app);
    if (app.window) gtk_widget_queue_draw(app.drawArea);
    refreshDressingRoom(app);
}

static void selectCustomKeyboard(LinuxApp& app, const std::string& id) {
    app.settings.customKeyboard = id;
    app.settings.save();
    if (app.window) gtk_widget_queue_draw(app.drawArea);
    refreshDressingRoom(app);
}

// 「换装间」 status line: kind 0 = neutral, 1 = success, 2 = error (colours come from the window CSS).
static void setDressingStatus(LinuxApp& app, const std::string& text, int kind) {
    if (!app.dressingStatus) return;
    gtk_label_set_text(GTK_LABEL(app.dressingStatus), text.c_str());
    GtkStyleContext* sc = gtk_widget_get_style_context(app.dressingStatus);
    gtk_style_context_remove_class(sc, "ok");
    gtk_style_context_remove_class(sc, "err");
    if (kind == 1) gtk_style_context_add_class(sc, "ok");
    if (kind == 2) gtk_style_context_add_class(sc, "err");
}

static void refreshSettingsWindow(LinuxApp& app);

static void refreshDressingRoom(LinuxApp& app) {
    if (app.dressingSubtitle) {
        const SkinSpec& skin = activeSkin(app);
        gchar* name = g_markup_escape_text(skin.name.c_str(), -1);
        std::string markup = std::string("<span size='small' foreground='#A8998D'>正在穿</span>\n"
                                         "<span size='large' weight='bold' foreground='#5A4A40'>") + name + "</span>";
        g_free(name);
        if (!skin.author.empty()) {
            gchar* author = g_markup_escape_text(skin.author.c_str(), -1);
            markup += std::string("\n<span size='small' foreground='#A8998D'>作者 ") + author + "</span>";
            g_free(author);
        }
        gtk_label_set_markup(GTK_LABEL(app.dressingSubtitle), markup.c_str());
    }
    if (app.dressingPreview) gtk_widget_queue_draw(app.dressingPreview);
    refreshSettingsWindow(app);
}

// Imports one file; returns true on success (and switches to it). Errors are shown in Chinese.
static bool importSkinFile(LinuxApp& app, const std::string& path) {
    std::string id, error;
    if (!app.skins.importFile(path, id, error)) {
        std::string base = path.substr(path.find_last_of('/') + 1);
        // With 换装间 open the red status line explains it in place; a dialog is only for drops onto the cat.
        if (app.dressingRoom) setDressingStatus(app, "没能导入「" + base + "」：" + error, 2);
        else showMessage(GTK_MESSAGE_ERROR, "无法导入这个皮肤", "「" + base + "」：" + error);
        return false;
    }
    selectCustomSkin(app, id);
    const SkinSpec* s = app.skins.find(id);
    std::string msg = "✓ 已换上「" + (s ? s->name : std::string("新皮肤")) + "」";
    if (s && !s->author.empty()) msg += "（作者：" + s->author + "）";
    setDressingStatus(app, msg, 1);
    refreshDressingRoom(app);
    return true;
}

// Imports one keyboard skin file (.kurokb / keyboard.json); returns true on success.
static bool importKeyboardSkinFile(LinuxApp& app, const std::string& path) {
    std::string id, error;
    if (!app.kbLibrary.importFile(path, id, error)) {
        std::string base = path.substr(path.find_last_of('/') + 1);
        if (app.dressingRoom) setDressingStatus(app, "没能导入键盘皮肤「" + base + "」：" + error, 2);
        else showMessage(GTK_MESSAGE_ERROR, "无法导入这个键盘皮肤", "「" + base + "」：" + error);
        return false;
    }
    selectCustomKeyboard(app, id);
    const KeyboardSkinSpec* kb = app.kbLibrary.find(id);
    std::string msg = "✓ 已换上键盘皮肤「" + (kb ? kb->name : std::string("新键盘")) + "」";
    if (kb && !kb->author.empty()) msg += "（作者：" + kb->author + "）";
    setDressingStatus(app, msg, 1);
    refreshDressingRoom(app);
    return true;
}

static bool importDroppedFile(LinuxApp& app, const std::string& path, bool strictExt = false) {
    if (KeyboardSkinLibrary::looksLikeKeyboardFile(path)) {
        return importKeyboardSkinFile(app, path);
    }
    if (hasSuffixCI(path, ".kurocat") || !strictExt) {
        return importSkinFile(app, path);
    }
    return false;
}

struct DropBatch {
    gchar** uris = nullptr;
    bool strictExt = false;
};

static void onDragDataReceived(GtkWidget*, GdkDragContext*, gint, gint,
                               GtkSelectionData* data, guint, guint, gpointer userData) {
    gchar** uris = gtk_selection_data_get_uris(data);
    if (!uris) return;
    auto* batch = new DropBatch{ uris, userData != nullptr };
    g_idle_add(+[](gpointer p) -> gboolean {
        auto* b = static_cast<DropBatch*>(p);
        for (int i = 0; b->uris[i] && g_app; ++i) {
            gchar* file = g_filename_from_uri(b->uris[i], nullptr, nullptr);
            if (file) {
                importDroppedFile(*g_app, file, b->strictExt);
                g_free(file);
            }
        }
        g_strfreev(b->uris);
        delete b;
        return G_SOURCE_REMOVE;
    }, batch);
}

// GTK finishes the drop itself (GTK_DEST_DEFAULT_ALL); the import (which may show a modal error
// dialog) runs from an idle callback so the drag source is never left waiting.
static void enableSkinDrop(GtkWidget* widget, bool gtkHighlight = true, bool strictExt = false) {
    gtk_drag_dest_set(widget, gtkHighlight ? GTK_DEST_DEFAULT_ALL : (GtkDestDefaults)(GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_DROP),
                      nullptr, 0, GDK_ACTION_COPY);
    gtk_drag_dest_add_uri_targets(widget);
    g_signal_connect(widget, "drag-data-received", G_CALLBACK(onDragDataReceived),
                     (gpointer)(intptr_t)(strictExt ? 1 : 0));
}

static void chooseKeyboardSkinFile() {
    bool en = g_app && g_app->settings.isEnglish;
    GtkWidget* chooser = gtk_file_chooser_dialog_new(
        en ? "Choose Keyboard Skin (.kurokb / .json)" : "选择键盘皮肤文件 (.kurokb / .json)",
        g_app && g_app->dressingRoom ? GTK_WINDOW(g_app->dressingRoom) : nullptr,
        GTK_FILE_CHOOSER_ACTION_OPEN,
        en ? "Cancel" : "取消", GTK_RESPONSE_CANCEL,
        en ? "Import" : "导入", GTK_RESPONSE_ACCEPT, nullptr);
    GtkFileFilter* filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, en ? "KuroCat Keyboard Skin (*.kurokb, *.json)" : "KuroCat 键盘皮肤 (*.kurokb, *.json)");
    gtk_file_filter_add_pattern(filter, "*.kurokb");
    gtk_file_filter_add_pattern(filter, "*.json");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(chooser), filter);
    if (gtk_dialog_run(GTK_DIALOG(chooser)) == GTK_RESPONSE_ACCEPT) {
        gchar* file = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser));
        gtk_widget_destroy(chooser);
        if (file) { importKeyboardSkinFile(*g_app, file); g_free(file); }
        return;
    }
    gtk_widget_destroy(chooser);
}

static void chooseSkinFile() {
    GtkWidget* chooser = gtk_file_chooser_dialog_new("选择 .kurocat 皮肤文件",
        g_app->dressingRoom ? GTK_WINDOW(g_app->dressingRoom) : nullptr, GTK_FILE_CHOOSER_ACTION_OPEN,
        "取消", GTK_RESPONSE_CANCEL, "导入", GTK_RESPONSE_ACCEPT, nullptr);
    GtkFileFilter* filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "KuroCat 皮肤 (*.kurocat)");
    gtk_file_filter_add_pattern(filter, "*.kurocat");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(chooser), filter);
    if (gtk_dialog_run(GTK_DIALOG(chooser)) == GTK_RESPONSE_ACCEPT) {
        gchar* file = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser));
        gtk_widget_destroy(chooser);
        if (file) { importSkinFile(*g_app, file); g_free(file); }
        return;
    }
    gtk_widget_destroy(chooser);
}

// MARK: 「换装间」 window (warm cream card UI: live preview of the worn skin, dashed drop zone, pill button)

static void roundedRectPath(cairo_t* cr, double x, double y, double w, double h, double r) {
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + w - r, y + r, r, -M_PI / 2, 0);
    cairo_arc(cr, x + w - r, y + h - r, r, 0, M_PI / 2);
    cairo_arc(cr, x + r, y + h - r, r, M_PI / 2, M_PI);
    cairo_arc(cr, x + r, y + r, r, M_PI, 1.5 * M_PI);
    cairo_close_path(cr);
}

static void drawPaw(cairo_t* cr, double cx, double cy, double k) {
    cairo_save(cr);
    cairo_translate(cr, cx, cy);
    cairo_scale(cr, k, k);
    cairo_new_path(cr);
    cairo_save(cr); cairo_translate(cr, 0, 6); cairo_scale(cr, 9.5, 7.8); cairo_arc(cr, 0, 0, 1, 0, 2 * M_PI); cairo_restore(cr);
    cairo_fill(cr);
    const double toes[4][2] = {{-10.5, -5.5}, {-3.8, -11.5}, {3.8, -11.5}, {10.5, -5.5}};
    for (const auto& t : toes) {
        cairo_save(cr); cairo_translate(cr, t[0], t[1]); cairo_scale(cr, 3.7, 4.4); cairo_arc(cr, 0, 0, 1, 0, 2 * M_PI); cairo_restore(cr);
        cairo_fill(cr);
    }
    cairo_restore(cr);
}

static void drawCenteredText(cairo_t* cr, GtkWidget* w, const char* text, const char* font, double cx, double y, double r, double g, double b) {
    PangoLayout* layout = gtk_widget_create_pango_layout(w, text);
    PangoFontDescription* fd = pango_font_description_from_string(font);
    pango_layout_set_font_description(layout, fd);
    int tw = 0, th = 0;
    pango_layout_get_pixel_size(layout, &tw, &th);
    cairo_move_to(cr, cx - tw / 2.0, y);
    cairo_set_source_rgb(cr, r, g, b);
    pango_cairo_show_layout(cr, layout);
    pango_font_description_free(fd);
    g_object_unref(layout);
}

static void showDressingRoom() {
    if (g_app->dressingRoom) { refreshDressingRoom(*g_app); gtk_window_present(GTK_WINDOW(g_app->dressingRoom)); return; }

    static GtkCssProvider* css = nullptr;
    if (!css) {
        css = gtk_css_provider_new();
        gtk_css_provider_load_from_data(css,
            "window.kc-dressing { background-color: #FBF7F0; }"
            ".kc-title { font-size: 21px; font-weight: 800; color: #3B2F2A; }"
            ".kc-subtitle { font-size: 13px; color: #7E6E62; }"
            ".kc-hint { font-size: 11.5px; color: #A8998D; }"
            "button.kc-primary { background-image: none; background-color: #E8964E; border: none; border-radius: 20px;"
            "  padding: 9px 30px; box-shadow: 0 2px 8px rgba(232,150,78,0.35); }"
            "button.kc-primary:hover { background-color: #F0A562; }"
            "button.kc-primary:active { background-color: #D7843E; box-shadow: none; }"
            "button.kc-primary label { color: #FFFFFF; font-weight: 700; font-size: 14px; }"
            ".kc-status { font-size: 12.5px; color: #8A7B70; }"
            ".kc-status.ok { color: #3E8E5A; font-weight: 600; }"
            ".kc-status.err { color: #C2493B; font-weight: 600; }"
            ".kc-footer { font-size: 10.5px; color: #B3A69B; }",
            -1, nullptr);
        // Screen-wide so child labels (e.g. the button's) are styled too; every rule is scoped to kc-* classes.
        gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(css),
                                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }

    GtkWidget* win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    g_app->dressingRoom = win;
    gtk_window_set_title(GTK_WINDOW(win), "换装间");
    gtk_window_set_resizable(GTK_WINDOW(win), FALSE);
    gtk_window_set_keep_above(GTK_WINDOW(win), TRUE);
    gtk_window_set_position(GTK_WINDOW(win), GTK_WIN_POS_CENTER);
    gtk_container_set_border_width(GTK_CONTAINER(win), 22);
    auto style = [](GtkWidget* w, const char* cls) { gtk_style_context_add_class(gtk_widget_get_style_context(w), cls); };
    style(win, "kc-dressing");

    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_container_add(GTK_CONTAINER(win), box);

    // Header: live preview of the worn skin + title
    GtkWidget* header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 18);
    gtk_box_pack_start(GTK_BOX(box), header, FALSE, FALSE, 0);
    GtkWidget* preview = gtk_drawing_area_new();
    gtk_widget_set_size_request(preview, 150, 134);
    g_app->dressingPreview = preview;
    g_signal_connect(preview, "draw", G_CALLBACK((+[](GtkWidget* w, cairo_t* cr, gpointer) -> gboolean {
        double W = gtk_widget_get_allocated_width(w), H = gtk_widget_get_allocated_height(w);
        roundedRectPath(cr, 0.5, 0.5, W - 1, H - 1, 18);
        cairo_set_source_rgb(cr, 0.953, 0.914, 0.863);
        cairo_fill_preserve(cr);
        cairo_clip(cr);
        static CatPhysics pose;   // a calm sitting pose
        pose.time = 0.7f;
        CatSettings settings = g_app->settings;
        settings.showKeyLegends = false;
        settings.isEditMode = false;
        settings.flipHorizontal = false;
        settings.rotationDeg = 0.0f;
        settings.obsCaptureMode = 0;
        cairo_save(cr);
        cairo_scale(cr, W / 290.0, H / 260.0);
        cairo_translate(cr, -40, -36);
        CatRenderer::draw(cr, pose, settings, activeSkin(*g_app), activeKeyboardSkin(*g_app));
        cairo_restore(cr);
        return FALSE;
    })), nullptr);
    gtk_box_pack_start(GTK_BOX(header), preview, FALSE, FALSE, 0);

    GtkWidget* titles = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_valign(titles, GTK_ALIGN_CENTER);
    GtkWidget* title = gtk_label_new("换装间");
    style(title, "kc-title");
    gtk_widget_set_halign(title, GTK_ALIGN_START);
    GtkWidget* subtitle = gtk_label_new("");
    style(subtitle, "kc-subtitle");
    gtk_widget_set_halign(subtitle, GTK_ALIGN_START);
    gtk_label_set_ellipsize(GTK_LABEL(subtitle), PANGO_ELLIPSIZE_END);
    gtk_label_set_max_width_chars(GTK_LABEL(subtitle), 20);
    g_app->dressingSubtitle = subtitle;
    GtkWidget* hint = gtk_label_new("导入皮肤工坊做好的 .kurocat 文件，\n小猫会立刻换上新毛色。");
    style(hint, "kc-hint");
    gtk_widget_set_halign(hint, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(titles), title, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(titles), subtitle, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(titles), hint, FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(header), titles, TRUE, TRUE, 0);

    // Drop zone: dashed rounded card with a paw; highlights while a file hovers; click = pick a file
    GtkWidget* drop = gtk_drawing_area_new();
    gtk_widget_set_size_request(drop, 400, 150);
    gtk_widget_add_events(drop, GDK_BUTTON_PRESS_MASK);
    g_app->dressingDrop = drop;
    g_signal_connect(drop, "draw", G_CALLBACK((+[](GtkWidget* w, cairo_t* cr, gpointer) -> gboolean {
        double W = gtk_widget_get_allocated_width(w), H = gtk_widget_get_allocated_height(w);
        bool hover = g_app->dropHover;
        roundedRectPath(cr, 1.5, 1.5, W - 3, H - 3, 16);
        if (hover) cairo_set_source_rgb(cr, 1.0, 0.945, 0.886); else cairo_set_source_rgb(cr, 1.0, 0.992, 0.976);
        cairo_fill_preserve(cr);
        double dash[2] = {7, 5};
        cairo_set_dash(cr, dash, hover ? 0 : 2, 0);
        cairo_set_line_width(cr, hover ? 2.4 : 2.0);
        if (hover) cairo_set_source_rgb(cr, 0.910, 0.588, 0.306); else cairo_set_source_rgb(cr, 0.890, 0.765, 0.627);
        cairo_stroke(cr);
        cairo_set_dash(cr, nullptr, 0, 0);
        cairo_set_source_rgba(cr, 0.910, 0.588, 0.306, hover ? 1.0 : 0.85);
        drawPaw(cr, W / 2, H / 2 - 26, hover ? 1.15 : 1.0);
        drawCenteredText(cr, w, hover ? "松手就换上啦" : "把 .kurocat 皮肤拖到这里", "Sans Bold 12", W / 2, H / 2 + 2, 0.353, 0.290, 0.251);
        drawCenteredText(cr, w, "也可以直接拖到桌面上的小猫身上 · 点这里选择文件", "Sans 9", W / 2, H / 2 + 26, 0.627, 0.561, 0.510);
        return FALSE;
    })), nullptr);
    g_signal_connect(drop, "button-press-event", G_CALLBACK(+[](GtkWidget*, GdkEventButton* e, gpointer) -> gboolean {
        if (e->button == 1) g_idle_add(+[](gpointer) -> gboolean { chooseSkinFile(); return G_SOURCE_REMOVE; }, nullptr);
        return TRUE;
    }), nullptr);
    enableSkinDrop(drop, false);
    g_signal_connect(drop, "drag-motion", G_CALLBACK(+[](GtkWidget* w, GdkDragContext*, gint, gint, guint, gpointer) -> gboolean {
        if (!g_app->dropHover) { g_app->dropHover = true; gtk_widget_queue_draw(w); }
        return FALSE;
    }), nullptr);
    g_signal_connect(drop, "drag-leave", G_CALLBACK(+[](GtkWidget* w, GdkDragContext*, guint, gpointer) {
        g_app->dropHover = false;
        gtk_widget_queue_draw(w);
    }), nullptr);
    gtk_box_pack_start(GTK_BOX(box), drop, FALSE, FALSE, 0);

    GtkWidget* button = gtk_button_new_with_label("选择文件…");
    style(button, "kc-primary");
    gtk_widget_set_halign(button, GTK_ALIGN_CENTER);
    g_signal_connect(button, "clicked", G_CALLBACK(+[](GtkButton*, gpointer) { chooseSkinFile(); }), nullptr);
    gtk_box_pack_start(GTK_BOX(box), button, FALSE, FALSE, 0);

    g_app->dressingStatus = gtk_label_new("");
    style(g_app->dressingStatus, "kc-status");
    gtk_label_set_line_wrap(GTK_LABEL(g_app->dressingStatus), TRUE);
    gtk_label_set_line_wrap_mode(GTK_LABEL(g_app->dressingStatus), PANGO_WRAP_WORD_CHAR);
    gtk_label_set_max_width_chars(GTK_LABEL(g_app->dressingStatus), 44);
    gtk_label_set_justify(GTK_LABEL(g_app->dressingStatus), GTK_JUSTIFY_CENTER);
    gtk_box_pack_start(GTK_BOX(box), g_app->dressingStatus, FALSE, FALSE, 0);

    std::string where = SkinLibrary::dir();
    if (const char* home = std::getenv("HOME"); home && home[0] && where.rfind(home, 0) == 0) where = "~" + where.substr(std::strlen(home));
    GtkWidget* footer = gtk_label_new(("皮肤独立保存在 " + where + "（软件更新会自动保留）  ·  右键小猫 → 猫咪皮肤 可切换或备份").c_str());
    style(footer, "kc-footer");
    gtk_label_set_line_wrap(GTK_LABEL(footer), TRUE);
    gtk_label_set_max_width_chars(GTK_LABEL(footer), 56);
    gtk_label_set_justify(GTK_LABEL(footer), GTK_JUSTIFY_CENTER);
    gtk_label_set_selectable(GTK_LABEL(footer), TRUE);
    gtk_box_pack_start(GTK_BOX(box), footer, FALSE, FALSE, 0);

    setDressingStatus(*g_app, "支持拖入多个文件；坏掉的文件会告诉你哪里不对。", 0);
    refreshDressingRoom(*g_app);
    g_signal_connect(win, "destroy", G_CALLBACK(+[](GtkWidget*, gpointer) {
        g_app->dressingRoom = nullptr;
        g_app->dressingStatus = nullptr;
        g_app->dressingSubtitle = nullptr;
        g_app->dressingPreview = nullptr;
        g_app->dressingDrop = nullptr;
        g_app->dropHover = false;
    }), nullptr);
    gtk_widget_show_all(win);
}

static void confirmDeleteSkin(const std::string& id) {
    const SkinSpec* s = g_app->skins.find(id);
    if (!s) return;
    GtkWidget* dlg = gtk_message_dialog_new(nullptr, GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
        "删除皮肤「%s」？", s->name.c_str());
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dlg), "会从皮肤文件夹里删除这个文件，删除后需要重新导入。");
    gtk_dialog_add_buttons(GTK_DIALOG(dlg), "取消", GTK_RESPONSE_CANCEL, "删除", GTK_RESPONSE_ACCEPT, nullptr);
    gtk_window_set_title(GTK_WINDOW(dlg), "KuroCat");
    gtk_window_set_keep_above(GTK_WINDOW(dlg), TRUE);
    int response = gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg);
    if (response != GTK_RESPONSE_ACCEPT) return;
    if (!g_app->skins.remove(id)) { showMessage(GTK_MESSAGE_ERROR, "删除失败", "无法删除 " + SkinLibrary::dir() + "/" + id + ".kurocat"); return; }
    if (g_app->settings.customSkin == id) selectCustomSkin(*g_app, "");
    refreshDressingRoom(*g_app);
}

static void confirmDeleteKeyboardSkin(const std::string& id) {
    const KeyboardSkinSpec* kb = g_app->kbLibrary.find(id);
    if (!kb) return;
    GtkWidget* dlg = gtk_message_dialog_new(nullptr, GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
        "删除键盘皮肤「%s」？", kb->name.c_str());
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dlg), "会从键盘皮肤文件夹里删除这个文件，删除后需要重新导入。");
    gtk_dialog_add_buttons(GTK_DIALOG(dlg), "取消", GTK_RESPONSE_CANCEL, "删除", GTK_RESPONSE_ACCEPT, nullptr);
    gtk_window_set_title(GTK_WINDOW(dlg), "KuroCat");
    gtk_window_set_keep_above(GTK_WINDOW(dlg), TRUE);
    int response = gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg);
    if (response != GTK_RESPONSE_ACCEPT) return;
    if (!g_app->kbLibrary.remove(id)) {
        showMessage(GTK_MESSAGE_ERROR, "删除失败", "无法删除 " + KeyboardSkinLibrary::directory() + "/" + id + ".kurokb");
        return;
    }
    if (g_app->settings.customKeyboard == id) selectCustomKeyboard(*g_app, "");
    refreshDressingRoom(*g_app);
}

static void showSettingsWindow();

static void refreshSettingsWindow(LinuxApp& app) {
    if (!app.settingsWindow) return;
    app.settingsUpdating = true;
    bool en = app.settings.isEnglish;
    auto tr = [&](const char* zh, const char* eng) -> const char* { return en ? eng : zh; };

    if (app.setCoatCombo) {
        gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(app.setCoatCombo));
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app.setCoatCombo), tr("经典小黑猫", "Classic Black Cat"));
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app.setCoatCombo), tr("挖煤暹罗猫", "Siamese Cat"));
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app.setCoatCombo), tr("美短（银虎斑加白）", "Silver Tabby Shorthair"));
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app.setCoatCombo), tr("橘白猫", "Orange & White Cat"));
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app.setCoatCombo), tr("英短蓝猫（灰毛橘黄瞳）", "British Shorthair (Gray)"));
        int sel = (int)app.settings.coatTheme;
        for (size_t i = 0; i < app.skins.skins.size(); ++i) {
            gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app.setCoatCombo), app.skins.skins[i].name.c_str());
            if (!app.settings.customSkin.empty() && app.skins.skins[i].id == app.settings.customSkin) sel = 5 + (int)i;
        }
        gtk_combo_box_set_active(GTK_COMBO_BOX(app.setCoatCombo), sel);
    }

    if (app.setEyeCombo) {
        gtk_combo_box_set_active(GTK_COMBO_BOX(app.setEyeCombo), (int)app.settings.eyeTheme);
    }

    if (app.setKbCombo) {
        app.kbLibrary.reload();
        gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(app.setKbCombo));
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app.setKbCombo), tr("经典奶白暖黄", "Classic Milk White"));
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app.setKbCombo), tr("暗夜极客黑", "Midnight Hacker Black"));
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app.setKbCombo), tr("复古奶油灰", "Retro Cream Gray"));
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app.setKbCombo), tr("深灰素色", "Graphite Gray"));
        int sel = (int)app.settings.keyboardTheme;
        for (size_t i = 0; i < app.kbLibrary.skins.size(); ++i) {
            gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app.setKbCombo), app.kbLibrary.skins[i].name.c_str());
            if (!app.settings.customKeyboard.empty() && app.kbLibrary.skins[i].id == app.settings.customKeyboard) sel = 4 + (int)i;
        }
        gtk_combo_box_set_active(GTK_COMBO_BOX(app.setKbCombo), sel);
    }

    if (app.setScaleSlider && app.setScaleLabel) {
        double pct = std::clamp((double)std::lround(app.settings.scale * 100.0f), 45.0, 250.0);
        gtk_range_set_value(GTK_RANGE(app.setScaleSlider), pct);
        char buf[32]; std::snprintf(buf, sizeof(buf), "%.0f%%", pct);
        gtk_label_set_text(GTK_LABEL(app.setScaleLabel), buf);
    }
    if (app.setRotSlider && app.setRotLabel) {
        double deg = std::clamp((double)std::lround(app.settings.rotationDeg), -180.0, 180.0);
        gtk_range_set_value(GTK_RANGE(app.setRotSlider), deg);
        char buf[32]; std::snprintf(buf, sizeof(buf), "%.0f\xC2\xB0", deg);
        gtk_label_set_text(GTK_LABEL(app.setRotLabel), buf);
    }

    auto setChk = [](GtkWidget* w, bool v) {
        if (w) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w), v ? TRUE : FALSE);
    };
    setChk(app.setFlipChk, app.settings.flipHorizontal);
    setChk(app.setOutlineChk, app.settings.whiteOutline);
    setChk(app.setAngerChk, app.settings.enableAnger);
    setChk(app.setPetChk, app.settings.petInteraction);
    setChk(app.setBadgesChk, app.settings.showKeyBadges);
    setChk(app.setLegendsChk, app.settings.showKeyLegends);
    setChk(app.setMarqueeChk, app.settings.keyboardMarquee);
    setChk(app.setEasterChk, app.settings.footEasterEgg || app.settings.tailEasterEgg);
    setChk(app.setTopmostChk, app.settings.alwaysOnTop);
    setChk(app.setAutohideChk, app.settings.autoHideFullscreen);
    setChk(app.setClickThroughChk, app.settings.fullClickThrough);
    setChk(app.setAutostartChk, isAutoStartEnabled());
    setChk(app.setHighFpsChk, app.settings.highFrameRate);
    setChk(app.setObsHideChk, app.settings.obsHideDesktop);
    if (app.setObsHideChk) gtk_widget_set_sensitive(app.setObsHideChk, app.settings.obsCaptureMode != 0);

    if (app.setTabletCombo) gtk_combo_box_set_active(GTK_COMBO_BOX(app.setTabletCombo), app.settings.tabletMode);
    if (app.setObsCombo) gtk_combo_box_set_active(GTK_COMBO_BOX(app.setObsCombo), app.settings.obsCaptureMode);
    if (app.setLangCombo) gtk_combo_box_set_active(GTK_COMBO_BOX(app.setLangCombo), app.settings.isEnglish ? 1 : 0);

    app.settingsUpdating = false;
}

static void showSettingsWindow() {
    LinuxApp& app = *g_app;
    if (app.settingsWindow) {
        refreshSettingsWindow(app);
        gtk_window_present(GTK_WINDOW(app.settingsWindow));
        return;
    }

    bool en = app.settings.isEnglish;
    auto tr = [&](const char* zh, const char* eng) -> const char* { return en ? eng : zh; };

    GtkWidget* win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), tr("KuroCat 设置", "KuroCat Settings"));
    gtk_window_set_default_size(GTK_WINDOW(win), 500, 620);
    gtk_window_set_position(GTK_WINDOW(win), GTK_WIN_POS_CENTER);
    app.settingsWindow = win;

    g_signal_connect(win, "destroy", G_CALLBACK(+[](GtkWidget*, gpointer) {
        g_app->settingsWindow = nullptr;
        g_app->setCoatCombo = g_app->setEyeCombo = g_app->setKbCombo = nullptr;
        g_app->setScaleSlider = g_app->setScaleLabel = g_app->setRotSlider = g_app->setRotLabel = nullptr;
        g_app->setFlipChk = g_app->setOutlineChk = g_app->setAngerChk = g_app->setPetChk = nullptr;
        g_app->setBadgesChk = g_app->setLegendsChk = g_app->setMarqueeChk = g_app->setEasterChk = nullptr;
        g_app->setTabletCombo = g_app->setTopmostChk = g_app->setAutohideChk = g_app->setClickThroughChk = nullptr;
        g_app->setAutostartChk = g_app->setHighFpsChk = g_app->setObsCombo = g_app->setObsHideChk = g_app->setLangCombo = nullptr;
    }), nullptr);

    GtkWidget* scroller = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(win), scroller);

    GtkWidget* root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_container_set_border_width(GTK_CONTAINER(root), 14);
    gtk_container_add(GTK_CONTAINER(scroller), root);

    auto makeSection = [&](const char* title) -> GtkWidget* {
        GtkWidget* frame = gtk_frame_new(title);
        GtkWidget* grid = gtk_grid_new();
        gtk_container_set_border_width(GTK_CONTAINER(grid), 10);
        gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
        gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
        gtk_container_add(GTK_CONTAINER(frame), grid);
        gtk_box_pack_start(GTK_BOX(root), frame, FALSE, FALSE, 0);
        return grid;
    };
    auto leftLabel = [](const char* txt) -> GtkWidget* {
        GtkWidget* l = gtk_label_new(txt);
        gtk_widget_set_halign(l, GTK_ALIGN_START);
        return l;
    };

    // Section 1: Appearance & Orientation
    {
        GtkWidget* g = makeSection(tr("外观与方向", "Appearance & Orientation"));
        int r = 0;

        gtk_grid_attach(GTK_GRID(g), leftLabel(tr("猫咪皮肤", "Cat Skin")), 0, r, 1, 1);
        app.setCoatCombo = gtk_combo_box_text_new();
        gtk_widget_set_hexpand(app.setCoatCombo, TRUE);
        g_signal_connect(app.setCoatCombo, "changed", G_CALLBACK(+[](GtkComboBox* cb, gpointer) {
            if (g_app->settingsUpdating) return;
            int idx = gtk_combo_box_get_active(cb);
            if (idx < 0) return;
            if (idx < 5) {
                auto coat = (CatCoatTheme)idx;
                g_app->settings.coatTheme = coat;
                g_app->settings.customSkin.clear();
                g_app->settings.eyeTheme = coat == CatCoatTheme::Siamese ? EyeColorTheme::SiameseBlue :
                    (coat == CatCoatTheme::ClassicBlack ? EyeColorTheme::NeonChartreuse :
                    (coat == CatCoatTheme::Shorthair ? EyeColorTheme::ReferenceHazel : EyeColorTheme::WarmAmber));
            } else if ((size_t)(idx - 5) < g_app->skins.skins.size()) {
                g_app->settings.customSkin = g_app->skins.skins[idx - 5].id;
            }
            g_app->settings.save();
            updateInputShape(*g_app);
            if (g_app->drawArea) gtk_widget_queue_draw(g_app->drawArea);
            refreshDressingRoom(*g_app);
        }), nullptr);
        gtk_grid_attach(GTK_GRID(g), app.setCoatCombo, 1, r, 1, 1);
        GtkWidget* dressBtn = gtk_button_new_with_label(tr("换装间…", "Wardrobe…"));
        g_signal_connect(dressBtn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer) { showDressingRoom(); }), nullptr);
        gtk_grid_attach(GTK_GRID(g), dressBtn, 2, r++, 1, 1);

        gtk_grid_attach(GTK_GRID(g), leftLabel(tr("瞳色切换", "Eye Color")), 0, r, 1, 1);
        app.setEyeCombo = gtk_combo_box_text_new();
        gtk_widget_set_hexpand(app.setEyeCombo, TRUE);
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app.setEyeCombo), tr("经典荧光黄绿", "Neon Chartreuse"));
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app.setEyeCombo), tr("暹罗宝石蓝", "Siamese Ice Blue"));
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app.setEyeCombo), tr("琥珀金瞳", "Warm Amber"));
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app.setEyeCombo), tr("冰晶浅蓝", "Ice Cyan"));
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app.setEyeCombo), tr("柔和琥珀", "Soft Amber"));
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app.setEyeCombo), tr("淡金青绿", "Golden Hazel"));
        g_signal_connect(app.setEyeCombo, "changed", G_CALLBACK(+[](GtkComboBox* cb, gpointer) {
            if (g_app->settingsUpdating) return;
            int idx = gtk_combo_box_get_active(cb);
            if (idx >= 0 && idx <= 5) {
                g_app->settings.eyeTheme = (EyeColorTheme)idx;
                g_app->settings.save();
                if (g_app->drawArea) gtk_widget_queue_draw(g_app->drawArea);
            }
        }), nullptr);
        gtk_grid_attach(GTK_GRID(g), app.setEyeCombo, 1, r, 1, 1);
        GtkWidget* editBtn = gtk_button_new_with_label(tr("框选调整", "Edit Box"));
        g_signal_connect(editBtn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer) {
            g_app->settings.isEditMode = !g_app->settings.isEditMode;
            updateInputShape(*g_app);
            if (g_app->drawArea) gtk_widget_queue_draw(g_app->drawArea);
        }), nullptr);
        gtk_grid_attach(GTK_GRID(g), editBtn, 2, r++, 1, 1);

        gtk_grid_attach(GTK_GRID(g), leftLabel(tr("键盘配色", "Keyboard")), 0, r, 1, 1);
        app.setKbCombo = gtk_combo_box_text_new();
        gtk_widget_set_hexpand(app.setKbCombo, TRUE);
        g_signal_connect(app.setKbCombo, "changed", G_CALLBACK(+[](GtkComboBox* cb, gpointer) {
            if (g_app->settingsUpdating) return;
            int idx = gtk_combo_box_get_active(cb);
            if (idx < 0) return;
            if (idx < 4) {
                g_app->settings.keyboardTheme = (KeyboardTheme)idx;
                g_app->settings.customKeyboard.clear();
            } else if ((size_t)(idx - 4) < g_app->kbLibrary.skins.size()) {
                g_app->settings.customKeyboard = g_app->kbLibrary.skins[idx - 4].id;
            }
            g_app->settings.save();
            if (g_app->drawArea) gtk_widget_queue_draw(g_app->drawArea);
        }), nullptr);
        gtk_grid_attach(GTK_GRID(g), app.setKbCombo, 1, r, 1, 1);
        GtkWidget* kbBtn = gtk_button_new_with_label(tr("导入键盘…", "Import…"));
        g_signal_connect(kbBtn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer) { chooseKeyboardSkinFile(); }), nullptr);
        gtk_grid_attach(GTK_GRID(g), kbBtn, 2, r++, 1, 1);

        gtk_grid_attach(GTK_GRID(g), leftLabel(tr("缩放大小", "Pet Scale")), 0, r, 1, 1);
        app.setScaleSlider = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 45.0, 250.0, 1.0);
        gtk_scale_set_draw_value(GTK_SCALE(app.setScaleSlider), FALSE);
        gtk_widget_set_hexpand(app.setScaleSlider, TRUE);
        g_signal_connect(app.setScaleSlider, "value-changed", G_CALLBACK(+[](GtkRange* range, gpointer) {
            if (g_app->settingsUpdating) return;
            double pct = std::round(gtk_range_get_value(range));
            g_app->settings.scale = (float)(pct / 100.0);
            applyWindowScaleAndPos(*g_app, true);
            if (g_app->setScaleLabel) {
                char buf[32]; std::snprintf(buf, sizeof(buf), "%.0f%%", pct);
                gtk_label_set_text(GTK_LABEL(g_app->setScaleLabel), buf);
            }
        }), nullptr);
        gtk_grid_attach(GTK_GRID(g), app.setScaleSlider, 1, r, 1, 1);
        app.setScaleLabel = gtk_label_new("100%");
        gtk_grid_attach(GTK_GRID(g), app.setScaleLabel, 2, r++, 1, 1);

        gtk_grid_attach(GTK_GRID(g), leftLabel(tr("朝向角度", "Rotation")), 0, r, 1, 1);
        app.setRotSlider = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, -180.0, 180.0, 1.0);
        gtk_scale_set_draw_value(GTK_SCALE(app.setRotSlider), FALSE);
        gtk_widget_set_hexpand(app.setRotSlider, TRUE);
        g_signal_connect(app.setRotSlider, "value-changed", G_CALLBACK(+[](GtkRange* range, gpointer) {
            if (g_app->settingsUpdating) return;
            double deg = std::round(gtk_range_get_value(range));
            if (std::fabs(deg) <= 3.0) deg = 0.0;
            else if (std::fabs(deg - 90.0) <= 3.0) deg = 90.0;
            else if (std::fabs(deg + 90.0) <= 3.0) deg = -90.0;
            g_app->settings.rotationDeg = (float)deg;
            g_app->settings.save();
            updateInputShape(*g_app);
            if (g_app->drawArea) gtk_widget_queue_draw(g_app->drawArea);
            if (g_app->setRotLabel) {
                char buf[32]; std::snprintf(buf, sizeof(buf), "%.0f\xC2\xB0", deg);
                gtk_label_set_text(GTK_LABEL(g_app->setRotLabel), buf);
            }
        }), nullptr);
        gtk_grid_attach(GTK_GRID(g), app.setRotSlider, 1, r, 1, 1);
        GtkWidget* rotBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        app.setRotLabel = gtk_label_new("0\xC2\xB0");
        GtkWidget* rotZeroBtn = gtk_button_new_with_label(tr("归零", "0\xC2\xB0"));
        g_signal_connect(rotZeroBtn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer) {
            g_app->settings.rotationDeg = 0.0f;
            g_app->settings.save();
            updateInputShape(*g_app);
            if (g_app->drawArea) gtk_widget_queue_draw(g_app->drawArea);
            refreshSettingsWindow(*g_app);
        }), nullptr);
        gtk_box_pack_start(GTK_BOX(rotBox), app.setRotLabel, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(rotBox), rotZeroBtn, FALSE, FALSE, 0);
        gtk_grid_attach(GTK_GRID(g), rotBox, 2, r++, 1, 1);

        GtkWidget* optRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
        app.setFlipChk = gtk_check_button_new_with_label(tr("水平翻转", "Flip Horizontal"));
        g_signal_connect(app.setFlipChk, "toggled", G_CALLBACK(+[](GtkToggleButton* b, gpointer) {
            if (g_app->settingsUpdating) return;
            g_app->settings.flipHorizontal = gtk_toggle_button_get_active(b);
            g_app->settings.save();
            updateInputShape(*g_app);
            if (g_app->drawArea) gtk_widget_queue_draw(g_app->drawArea);
        }), nullptr);
        app.setOutlineChk = gtk_check_button_new_with_label(tr("白色贴纸描边", "White Sticker Outline"));
        g_signal_connect(app.setOutlineChk, "toggled", G_CALLBACK(+[](GtkToggleButton* b, gpointer) {
            if (g_app->settingsUpdating) return;
            g_app->settings.whiteOutline = gtk_toggle_button_get_active(b);
            g_app->settings.save();
            if (g_app->drawArea) gtk_widget_queue_draw(g_app->drawArea);
        }), nullptr);
        GtkWidget* resetBtn = gtk_button_new_with_label(tr("重置位置与朝向", "Reset Pose"));
        g_signal_connect(resetBtn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer) {
            g_app->settings.scale = 1.0f;
            g_app->settings.rotationDeg = 0.0f;
            g_app->settings.flipHorizontal = false;
            g_app->settings.posX = -99999;
            g_app->settings.posY = -99999;
            applyWindowScaleAndPos(*g_app, false);
            refreshSettingsWindow(*g_app);
        }), nullptr);
        gtk_box_pack_start(GTK_BOX(optRow), app.setFlipChk, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(optRow), app.setOutlineChk, FALSE, FALSE, 0);
        gtk_box_pack_end(GTK_BOX(optRow), resetBtn, FALSE, FALSE, 0);
        gtk_grid_attach(GTK_GRID(g), optRow, 0, r++, 3, 1);
    }

    // Section 2: Interaction & Effects
    {
        GtkWidget* g = makeSection(tr("互动与特效", "Interaction & Effects"));
        int r = 0;

        app.setAngerChk = gtk_check_button_new_with_label(tr("高速打字生气表情", "Angry Expression on Fast Typing"));
        g_signal_connect(app.setAngerChk, "toggled", G_CALLBACK(+[](GtkToggleButton* b, gpointer) {
            if (g_app->settingsUpdating) return;
            g_app->settings.enableAnger = gtk_toggle_button_get_active(b);
            g_app->settings.save();
        }), nullptr);
        app.setPetChk = gtk_check_button_new_with_label(tr("摸猫互动与咕噜特效", "Petting & Purr Sparkles"));
        g_signal_connect(app.setPetChk, "toggled", G_CALLBACK(+[](GtkToggleButton* b, gpointer) {
            if (g_app->settingsUpdating) return;
            g_app->settings.petInteraction = gtk_toggle_button_get_active(b);
            g_app->settings.save();
        }), nullptr);
        gtk_grid_attach(GTK_GRID(g), app.setAngerChk, 0, r, 1, 1);
        gtk_grid_attach(GTK_GRID(g), app.setPetChk, 1, r++, 2, 1);

        app.setBadgesChk = gtk_check_button_new_with_label(tr("显示按键气泡", "Show Key & Click Badges"));
        g_signal_connect(app.setBadgesChk, "toggled", G_CALLBACK(+[](GtkToggleButton* b, gpointer) {
            if (g_app->settingsUpdating) return;
            g_app->settings.showKeyBadges = gtk_toggle_button_get_active(b);
            g_app->settings.save();
        }), nullptr);
        app.setLegendsChk = gtk_check_button_new_with_label(tr("显示键盘字符", "Show Keycap Legends"));
        g_signal_connect(app.setLegendsChk, "toggled", G_CALLBACK(+[](GtkToggleButton* b, gpointer) {
            if (g_app->settingsUpdating) return;
            g_app->settings.showKeyLegends = gtk_toggle_button_get_active(b);
            g_app->settings.save();
            if (g_app->drawArea) gtk_widget_queue_draw(g_app->drawArea);
        }), nullptr);
        gtk_grid_attach(GTK_GRID(g), app.setBadgesChk, 0, r, 1, 1);
        gtk_grid_attach(GTK_GRID(g), app.setLegendsChk, 1, r++, 2, 1);

        app.setMarqueeChk = gtk_check_button_new_with_label(tr("键盘跑马灯流光", "Keyboard RGB Marquee"));
        g_signal_connect(app.setMarqueeChk, "toggled", G_CALLBACK(+[](GtkToggleButton* b, gpointer) {
            if (g_app->settingsUpdating) return;
            g_app->settings.keyboardMarquee = gtk_toggle_button_get_active(b);
            g_app->settings.save();
        }), nullptr);
        app.setEasterChk = gtk_check_button_new_with_label(tr("踩键与甩尾彩蛋", "Hind-Foot & Tail Easter Eggs"));
        g_signal_connect(app.setEasterChk, "toggled", G_CALLBACK(+[](GtkToggleButton* b, gpointer) {
            if (g_app->settingsUpdating) return;
            bool on = gtk_toggle_button_get_active(b);
            g_app->settings.footEasterEgg = on;
            g_app->settings.tailEasterEgg = on;
            g_app->settings.save();
        }), nullptr);
        gtk_grid_attach(GTK_GRID(g), app.setMarqueeChk, 0, r, 1, 1);
        gtk_grid_attach(GTK_GRID(g), app.setEasterChk, 1, r++, 2, 1);

        gtk_grid_attach(GTK_GRID(g), leftLabel(tr("数位板模式", "Tablet Mode")), 0, r, 1, 1);
        app.setTabletCombo = gtk_combo_box_text_new();
        gtk_widget_set_hexpand(app.setTabletCombo, TRUE);
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app.setTabletCombo), tr("自动检测（连接数位板时自动切换）", "Auto-Detect (Switch When Connected)"));
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app.setTabletCombo), tr("始终显示数位板与触控笔", "Always Show Tablet & Stylus"));
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app.setTabletCombo), tr("始终显示鼠标", "Always Show Mouse"));
        g_signal_connect(app.setTabletCombo, "changed", G_CALLBACK(+[](GtkComboBox* cb, gpointer) {
            if (g_app->settingsUpdating) return;
            int idx = gtk_combo_box_get_active(cb);
            if (idx >= 0 && idx <= 2) {
                g_app->settings.tabletMode = idx;
                g_app->settings.save();
                if (g_app->drawArea) gtk_widget_queue_draw(g_app->drawArea);
            }
        }), nullptr);
        gtk_grid_attach(GTK_GRID(g), app.setTabletCombo, 1, r++, 2, 1);
    }

    // Section 3: Window & System
    {
        GtkWidget* g = makeSection(tr("窗口与系统", "Window & System"));
        int r = 0;

        app.setTopmostChk = gtk_check_button_new_with_label(tr("窗口置顶显示", "Always On Top"));
        g_signal_connect(app.setTopmostChk, "toggled", G_CALLBACK(+[](GtkToggleButton* b, gpointer) {
            if (g_app->settingsUpdating) return;
            g_app->settings.alwaysOnTop = gtk_toggle_button_get_active(b);
            applyWindowScaleAndPos(*g_app, false);
        }), nullptr);
        app.setAutohideChk = gtk_check_button_new_with_label(tr("全屏应用时自动隐藏", "Auto-Hide in Fullscreen"));
        g_signal_connect(app.setAutohideChk, "toggled", G_CALLBACK(+[](GtkToggleButton* b, gpointer) {
            if (g_app->settingsUpdating) return;
            g_app->settings.autoHideFullscreen = gtk_toggle_button_get_active(b);
            g_app->lastFullscreenCheck = 0.0;
            g_app->settings.save();
            if (!g_app->settings.autoHideFullscreen && g_app->hiddenByFullscreen) {
                g_app->hiddenByFullscreen = false;
                if (g_app->isVisible) gtk_widget_show(g_app->window);
            }
        }), nullptr);
        gtk_grid_attach(GTK_GRID(g), app.setTopmostChk, 0, r, 1, 1);
        gtk_grid_attach(GTK_GRID(g), app.setAutohideChk, 1, r++, 2, 1);

        app.setClickThroughChk = gtk_check_button_new_with_label(tr("完全鼠标穿透 (Alt 临时解锁)", "Full Click-Through (Hold Alt)"));
        g_signal_connect(app.setClickThroughChk, "toggled", G_CALLBACK(+[](GtkToggleButton* b, gpointer) {
            if (g_app->settingsUpdating) return;
            g_app->settings.fullClickThrough = gtk_toggle_button_get_active(b);
            updateInputShape(*g_app);
            g_app->settings.save();
        }), nullptr);
        app.setAutostartChk = gtk_check_button_new_with_label(tr("开机自动启动", "Launch at Login"));
        g_signal_connect(app.setAutostartChk, "toggled", G_CALLBACK(+[](GtkToggleButton* b, gpointer) {
            if (g_app->settingsUpdating) return;
            setAutoStartEnabled(gtk_toggle_button_get_active(b));
        }), nullptr);
        gtk_grid_attach(GTK_GRID(g), app.setClickThroughChk, 0, r, 1, 1);
        gtk_grid_attach(GTK_GRID(g), app.setAutostartChk, 1, r++, 2, 1);

        app.setHighFpsChk = gtk_check_button_new_with_label(tr("高帧率 60fps（电脑卡顿时请关闭）", "High FPS 60fps"));
        g_signal_connect(app.setHighFpsChk, "toggled", G_CALLBACK(+[](GtkToggleButton* b, gpointer) {
            if (g_app->settingsUpdating) return;
            g_app->settings.highFrameRate = gtk_toggle_button_get_active(b);
            g_app->settings.save();
        }), nullptr);
        gtk_grid_attach(GTK_GRID(g), app.setHighFpsChk, 0, r++, 3, 1);

        gtk_grid_attach(GTK_GRID(g), leftLabel(tr("OBS 直播模式", "OBS Capture")), 0, r, 1, 1);
        app.setObsCombo = gtk_combo_box_text_new();
        gtk_widget_set_hexpand(app.setObsCombo, TRUE);
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app.setObsCombo), tr("关闭（正常悬浮）", "Off (Normal Overlay)"));
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app.setObsCombo), tr("OBS 透明窗口捕获", "OBS Transparent Window"));
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app.setObsCombo), tr("OBS 绿幕模式 (#00FF00)", "OBS Green Screen"));
        g_signal_connect(app.setObsCombo, "changed", G_CALLBACK(+[](GtkComboBox* cb, gpointer) {
            if (g_app->settingsUpdating) return;
            int idx = gtk_combo_box_get_active(cb);
            if (idx >= 0 && idx <= 2) {
                g_app->settings.obsCaptureMode = idx;
                if (idx == 0) g_app->settings.obsHideDesktop = false;
                g_app->settings.save();
                applyOBSMode(*g_app);
                refreshSettingsWindow(*g_app);
            }
        }), nullptr);
        gtk_grid_attach(GTK_GRID(g), app.setObsCombo, 1, r, 1, 1);
        app.setObsHideChk = gtk_check_button_new_with_label(tr("后台隐藏桌面", "Hide Desktop"));
        g_signal_connect(app.setObsHideChk, "toggled", G_CALLBACK(+[](GtkToggleButton* b, gpointer) {
            if (g_app->settingsUpdating) return;
            g_app->settings.obsHideDesktop = gtk_toggle_button_get_active(b);
            g_app->settings.save();
            applyOBSMode(*g_app);
        }), nullptr);
        gtk_grid_attach(GTK_GRID(g), app.setObsHideChk, 2, r++, 1, 1);

        gtk_grid_attach(GTK_GRID(g), leftLabel(tr("界面语言", "Language")), 0, r, 1, 1);
        app.setLangCombo = gtk_combo_box_text_new();
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app.setLangCombo), "简体中文");
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app.setLangCombo), "English");
        g_signal_connect(app.setLangCombo, "changed", G_CALLBACK(+[](GtkComboBox* cb, gpointer) {
            if (g_app->settingsUpdating) return;
            bool newEn = (gtk_combo_box_get_active(cb) == 1);
            if (newEn != g_app->settings.isEnglish) {
                g_app->settings.isEnglish = newEn;
                g_app->settings.save();
                if (g_app->drawArea) gtk_widget_queue_draw(g_app->drawArea);
                GtkWidget* old = g_app->settingsWindow;
                if (old) gtk_widget_destroy(old);
                g_idle_add(+[](gpointer) -> gboolean { showSettingsWindow(); return G_SOURCE_REMOVE; }, nullptr);
            }
        }), nullptr);
        gtk_grid_attach(GTK_GRID(g), app.setLangCombo, 1, r, 1, 1);
        GtkWidget* dirsBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        GtkWidget* sDirBtn = gtk_button_new_with_label(tr("皮肤目录", "Skins"));
        g_signal_connect(sDirBtn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer) {
            SkinLibrary::ensureDir();
            openUrlInBrowser(SkinLibrary::dir());
        }), nullptr);
        GtkWidget* kDirBtn = gtk_button_new_with_label(tr("键盘目录", "Keyboards"));
        g_signal_connect(kDirBtn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer) {
            KeyboardSkinLibrary::ensureDir();
            openUrlInBrowser(KeyboardSkinLibrary::directory());
        }), nullptr);
        gtk_box_pack_start(GTK_BOX(dirsBox), sDirBtn, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(dirsBox), kDirBtn, TRUE, TRUE, 0);
        gtk_grid_attach(GTK_GRID(g), dirsBox, 2, r++, 1, 1);
    }

    refreshSettingsWindow(app);
    gtk_widget_show_all(win);
    gtk_window_present(GTK_WINDOW(win));
}

static void showPopupMenu(LinuxApp& app, GdkEventButton* evt) {
    GtkWidget* menu = gtk_menu_new();

    auto addDisabled = [&](const std::string& label) {
        GtkWidget* it = gtk_menu_item_new_with_label(label.c_str());
        gtk_widget_set_sensitive(it, FALSE);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), it);
    };
    auto addSep = [&]() {
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
    };
    auto addAction = [&](const char* label, GCallback cb, gpointer data = nullptr) {
        GtkWidget* it = gtk_menu_item_new_with_label(label);
        g_signal_connect(it, "activate", cb, data);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), it);
    };

    bool en = app.settings.isEnglish;
    auto tr = [&](const char* zh, const char* eng) -> const char* { return en ? eng : zh; };

    addDisabled(app.input.statusText());
    addDisabled(en
        ? ("Today " + std::to_string(app.input.totalKeystrokesToday)
            + "  ·  KPM " + std::to_string(app.input.currentKPM())
            + "  ·  Pets " + std::to_string(app.physics.totalPetSessions))
        : ("今日击键 " + std::to_string(app.input.totalKeystrokesToday)
            + "  ·  KPM " + std::to_string(app.input.currentKPM())
            + "  ·  摸猫 " + std::to_string(app.physics.totalPetSessions) + " 次"));
    addSep();

    addAction(app.isVisible ? tr("隐藏桌宠", "Hide Pet") : tr("显示桌宠", "Show Pet"), G_CALLBACK(+[](GtkMenuItem*, gpointer) {
        g_app->isVisible = !g_app->isVisible;
        g_app->hiddenByFullscreen = false;
        if (g_app->isVisible) gtk_widget_show(g_app->window);
        else gtk_widget_hide(g_app->window);
    }));

    addAction(app.settings.isEditMode
        ? tr("✓ 完成调整大小与方向", "✓ Finish Resize & Direction")
        : tr("调整大小与方向 (双击小猫)", "Resize & Direction (Double-Click Cat)"),
        G_CALLBACK(+[](GtkMenuItem*, gpointer) {
            g_app->settings.isEditMode = !g_app->settings.isEditMode;
            updateInputShape(*g_app);
            gtk_widget_queue_draw(g_app->drawArea);
        }));

    // Quick Cat Skin switcher submenu
    {
        GtkWidget* sub = gtk_menu_new();
        bool customActive = app.skins.find(app.settings.customSkin) != nullptr;
        auto addCoat = [&](const char* lbl, CatCoatTheme c) {
            GtkWidget* it = gtk_check_menu_item_new_with_label(lbl);
            gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(it), !customActive && app.settings.coatTheme == c);
            g_signal_connect(it, "activate", G_CALLBACK(+[](GtkMenuItem*, gpointer p) {
                auto coat = (CatCoatTheme)(intptr_t)p;
                g_app->settings.coatTheme = coat;
                g_app->settings.customSkin.clear();
                g_app->settings.eyeTheme = coat == CatCoatTheme::Siamese ? EyeColorTheme::SiameseBlue :
                    (coat == CatCoatTheme::ClassicBlack ? EyeColorTheme::NeonChartreuse :
                    (coat == CatCoatTheme::Shorthair ? EyeColorTheme::ReferenceHazel : EyeColorTheme::WarmAmber)); // OrangeWhite & BritishShorthair: warm amber
                g_app->settings.save();
                updateInputShape(*g_app);
                refreshDressingRoom(*g_app);
            }), (gpointer)(intptr_t)c);
            gtk_menu_shell_append(GTK_MENU_SHELL(sub), it);
        };
        addCoat(tr("经典小黑猫 (原版)", "Classic Black Cat"), CatCoatTheme::ClassicBlack);
        addCoat(tr("挖煤暹罗猫", "Siamese Cat"), CatCoatTheme::Siamese);
        addCoat(tr("美短（银虎斑加白）", "Silver Tabby Shorthair"), CatCoatTheme::Shorthair);
        addCoat(tr("橘白猫", "Orange & White Cat"), CatCoatTheme::OrangeWhite);
        addCoat(tr("英短蓝猫（灰毛橘黄瞳）", "British Shorthair (Gray)"), CatCoatTheme::BritishShorthair);
        if (!app.skins.skins.empty()) gtk_menu_shell_append(GTK_MENU_SHELL(sub), gtk_separator_menu_item_new());
        for (const SkinSpec& s : app.skins.skins) {
            GtkWidget* it = gtk_check_menu_item_new_with_label(s.name.c_str());
            gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(it), app.settings.customSkin == s.id);
            g_object_set_data_full(G_OBJECT(it), "skin-id", g_strdup(s.id.c_str()), g_free);
            g_signal_connect(it, "activate", G_CALLBACK(+[](GtkMenuItem* item, gpointer) {
                selectCustomSkin(*g_app, (const char*)g_object_get_data(G_OBJECT(item), "skin-id"));
            }), nullptr);
            gtk_menu_shell_append(GTK_MENU_SHELL(sub), it);
        }
        gtk_menu_shell_append(GTK_MENU_SHELL(sub), gtk_separator_menu_item_new());
        GtkWidget* importItem = gtk_menu_item_new_with_label(tr("导入皮肤 / 换装间…", "Import Skin / Wardrobe…"));
        g_signal_connect(importItem, "activate", G_CALLBACK(+[](GtkMenuItem*, gpointer) { showDressingRoom(); }), nullptr);
        gtk_menu_shell_append(GTK_MENU_SHELL(sub), importItem);

        GtkWidget* subWorkshopItem = gtk_menu_item_new_with_label(tr("打开皮肤工坊…", "Open Skin Workshop…"));
        g_signal_connect(subWorkshopItem, "activate", G_CALLBACK(+[](GtkMenuItem*, gpointer) {
            openUrlInBrowser("https://chanke17.github.io/KuroCat-Workshop/");
        }), nullptr);
        gtk_menu_shell_append(GTK_MENU_SHELL(sub), subWorkshopItem);

        GtkWidget* parent = gtk_menu_item_new_with_label(tr("猫咪皮肤 🐱", "Cat Skin 🐱"));
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(parent), sub);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), parent);
    }

    addAction(tr("🎨 皮肤工坊（制作皮肤）…", "🎨 Skin Workshop (Create Skins)…"), G_CALLBACK(+[](GtkMenuItem*, gpointer) {
        openUrlInBrowser("https://chanke17.github.io/KuroCat-Workshop/");
    }));

    addAction(tr("⚙️ 设置…", "⚙️ Settings…"), G_CALLBACK(+[](GtkMenuItem*, gpointer) {
        showSettingsWindow();
    }));

    addSep();
    if (!app.availableUpdateVersion.empty()) {
        std::string lbl = app.isUpdating
            ? (std::string(tr("⏳ 正在下载更新 ", "⏳ Downloading update ")) + app.availableUpdateVersion + "...")
            : (std::string(tr("🆕 发现新版本 ", "🆕 Update Available ")) + app.availableUpdateVersion);
        if (app.isUpdating) {
            GtkWidget* item = gtk_menu_item_new_with_label(lbl.c_str());
            gtk_widget_set_sensitive(item, FALSE);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
        } else {
            addAction(lbl.c_str(), G_CALLBACK(+[](GtkMenuItem*, gpointer) {
                startInAppUpdate();
            }));
        }
    } else {
        std::string lbl = std::string(tr("检查更新 (当前 v", "Check for Updates (v")) + APP_VERSION + ")";
        addAction(lbl.c_str(), G_CALLBACK(+[](GtkMenuItem*, gpointer) {
            startAsyncUpdateCheck(true, 0);
        }));
    }
    addSep();
    addAction(tr("退出 KuroCat", "Quit KuroCat"), G_CALLBACK(+[](GtkMenuItem*, gpointer) {
        releaseSingleInstance();
        gtk_main_quit();
    }));

    gtk_widget_show_all(menu);
    if (evt) {
        gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent*)evt);
    } else {
        gtk_menu_popup_at_pointer(GTK_MENU(menu), nullptr);
    }
}

static gboolean onDraw(GtkWidget* widget, cairo_t* cr, gpointer) {
    int w = gtk_widget_get_allocated_width(widget);
    int h = gtk_widget_get_allocated_height(widget);

    cairo_save(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    if (g_app && g_app->settings.obsCaptureMode == 2) {
        cairo_set_source_rgba(cr, 0.0, 1.0, 0.0, 1.0);
    } else {
        cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.0);
    }
    cairo_paint(cr);
    cairo_restore(cr);

    cairo_save(cr);
    cairo_scale(cr, (double)w / CatPhysics::CanvasWidth, (double)h / CatPhysics::CanvasHeight);
    CatRenderer::draw(cr, g_app->physics, g_app->settings, activeSkin(*g_app), activeKeyboardSkin(*g_app));
    cairo_restore(cr);
    return FALSE;
}

static gboolean onButtonPress(GtkWidget*, GdkEventButton* evt, gpointer) {
    if (evt->button == 3) {
        showPopupMenu(*g_app, evt);
        return TRUE;
    }
    if (evt->button == 1 && evt->type == GDK_2BUTTON_PRESS) {
        if (g_app->settings.isEditMode && g_app->winW > 0 && g_app->winH > 0) {
            Pt rawCp = {
                (float)(evt->x / (double)g_app->winW * CatPhysics::CanvasWidth),
                (float)(evt->y / (double)g_app->winH * CatPhysics::CanvasHeight)
            };
            if (CatRenderer::hitTestEditOverlay(rawCp) == 20) {
                g_app->isRotating = false;
                g_app->settings.rotationDeg = 0.0f;
                g_app->settings.save();
                updateInputShape(*g_app);
                gtk_widget_queue_draw(g_app->drawArea);
                refreshSettingsWindow(*g_app);
                return TRUE;
            }
        }
        g_app->settings.isEditMode = !g_app->settings.isEditMode;
        updateInputShape(*g_app);
        gtk_widget_queue_draw(g_app->drawArea);
        return TRUE;
    }
    if (evt->button == 1 && (g_app->settings.isEditMode || !g_app->settings.fullClickThrough || g_app->clickThroughTemporarilyUnlocked)) {
        if (g_app->settings.isEditMode && g_app->winW > 0 && g_app->winH > 0) {
            Pt rawCp = {
                (float)(evt->x / (double)g_app->winW * CatPhysics::CanvasWidth),
                (float)(evt->y / (double)g_app->winH * CatPhysics::CanvasHeight)
            };
            int hit = CatRenderer::hitTestEditOverlay(rawCp);
            if (hit == 20) {
                g_app->isRotating = true;
                g_app->rotateMoved = false;
                int wx = 0, wy = 0, ww = 0, wh = 0;
                gtk_window_get_position(GTK_WINDOW(g_app->window), &wx, &wy);
                gtk_window_get_size(GTK_WINDOW(g_app->window), &ww, &wh);
                double cx = wx + ww * 0.5;
                double cy = wy + wh * 0.5;
                g_app->rotateStartAngle = std::atan2(evt->y_root - cy, evt->x_root - cx);
                g_app->rotateStartDeg = g_app->settings.rotationDeg;
                return TRUE;
            }
            if (hit >= 0 && hit <= 3) {
                g_app->activeResizeCorner = hit;
                int wx = 0, wy = 0, ww = 0, wh = 0;
                gtk_window_get_position(GTK_WINDOW(g_app->window), &wx, &wy);
                gtk_window_get_size(GTK_WINDOW(g_app->window), &ww, &wh);
                if (hit == 0) { g_app->resizeAnchorX = wx + ww; g_app->resizeAnchorY = wy + wh; }
                else if (hit == 1) { g_app->resizeAnchorX = wx; g_app->resizeAnchorY = wy + wh; }
                else if (hit == 2) { g_app->resizeAnchorX = wx + ww; g_app->resizeAnchorY = wy; }
                else { g_app->resizeAnchorX = wx; g_app->resizeAnchorY = wy; }
                return TRUE;
            }
            if (hit < 0) {
                g_app->settings.isEditMode = false;
                updateInputShape(*g_app);
                gtk_widget_queue_draw(g_app->drawArea);
                return TRUE;
            }
        }
        g_app->isDragging = true;
        g_app->dragStartRootX = evt->x_root;
        g_app->dragStartRootY = evt->y_root;
        g_app->lastDragRootX = evt->x_root;
        g_app->lastDragRootY = evt->y_root;
        gtk_window_get_position(GTK_WINDOW(g_app->window), &g_app->dragStartWinX, &g_app->dragStartWinY);
        g_app->physics.setDragging(true, 0.0f, 0.0f);
        return TRUE;
    }
    return FALSE;
}

static gboolean onMotionNotify(GtkWidget*, GdkEventMotion* evt, gpointer) {
    if (g_app->isRotating) {
        int wx = 0, wy = 0, ww = 0, wh = 0;
        gtk_window_get_position(GTK_WINDOW(g_app->window), &wx, &wy);
        gtk_window_get_size(GTK_WINDOW(g_app->window), &ww, &wh);
        double cx = wx + ww * 0.5;
        double cy = wy + wh * 0.5;
        double angle = std::atan2(evt->y_root - cy, evt->x_root - cx);
        double deltaDeg = (angle - g_app->rotateStartAngle) * (180.0 / 3.14159265358979323846);
        while (deltaDeg > 180.0) deltaDeg -= 360.0;
        while (deltaDeg < -180.0) deltaDeg += 360.0;
        if (std::fabs(deltaDeg) > 1.5) g_app->rotateMoved = true;
        if (g_app->rotateMoved) {
            double deg = (double)g_app->rotateStartDeg + deltaDeg;
            while (deg > 180.0) deg -= 360.0;
            while (deg <= -180.0) deg += 360.0;
            const double snaps[5] = { 0.0, 90.0, -90.0, 180.0, -180.0 };
            for (double s : snaps) {
                if (std::fabs(deg - s) <= 5.0) { deg = (s == -180.0 ? 180.0 : s); break; }
            }
            g_app->settings.rotationDeg = (float)deg;
            gtk_widget_queue_draw(g_app->drawArea);
        }
        return TRUE;
    }
    if (g_app->activeResizeCorner >= 0) {
        double dx = std::fabs(evt->x_root - g_app->resizeAnchorX);
        double dy = std::fabs(evt->y_root - g_app->resizeAnchorY);
        float newScale = std::clamp((float)std::max(dx / CatPhysics::CanvasWidth, dy / CatPhysics::CanvasHeight), 0.45f, 2.50f);
        int nw = (int)std::round(CatPhysics::CanvasWidth * newScale);
        int nh = (int)std::round(CatPhysics::CanvasHeight * newScale);
        int nx = (g_app->activeResizeCorner == 0 || g_app->activeResizeCorner == 2)
            ? (int)std::round(g_app->resizeAnchorX - nw) : (int)std::round(g_app->resizeAnchorX);
        int ny = (g_app->activeResizeCorner == 0 || g_app->activeResizeCorner == 1)
            ? (int)std::round(g_app->resizeAnchorY - nh) : (int)std::round(g_app->resizeAnchorY);
        g_app->settings.scale = newScale;
        gtk_window_resize(GTK_WINDOW(g_app->window), nw, nh);
        gtk_window_move(GTK_WINDOW(g_app->window), nx, ny);
        return TRUE;
    }
    if (g_app->isDragging) {
        int nx = g_app->dragStartWinX + (int)std::round(evt->x_root - g_app->dragStartRootX);
        int ny = g_app->dragStartWinY + (int)std::round(evt->y_root - g_app->dragStartRootY);
        float dx = (float)(evt->x_root - g_app->lastDragRootX);
        float dy = (float)(evt->y_root - g_app->lastDragRootY);
        g_app->lastDragRootX = evt->x_root;
        g_app->lastDragRootY = evt->y_root;
        float rad = -g_app->settings.rotationDeg * (3.14159265f / 180.0f);
        float c = std::cos(rad), s = std::sin(rad);
        float localDx = dx * c - dy * s;
        float localDy = dx * s + dy * c;
        g_app->physics.setDragging(true, g_app->settings.flipHorizontal ? -localDx : localDx, localDy);
        gtk_window_move(GTK_WINDOW(g_app->window), nx, ny);
        return TRUE;
    }
    return FALSE;
}

static gboolean onButtonRelease(GtkWidget*, GdkEventButton* evt, gpointer) {
    if (evt->button == 1 && g_app->isRotating) {
        g_app->isRotating = false;
        if (!g_app->rotateMoved) {
            g_app->settings.flipHorizontal = !g_app->settings.flipHorizontal;
        }
        g_app->settings.save();
        updateInputShape(*g_app);
        gtk_widget_queue_draw(g_app->drawArea);
        refreshSettingsWindow(*g_app);
        return TRUE;
    }
    if (evt->button == 1 && g_app->activeResizeCorner >= 0) {
        g_app->activeResizeCorner = -1;
        gtk_window_get_position(GTK_WINDOW(g_app->window), &g_app->settings.posX, &g_app->settings.posY);
        g_app->settings.save();
        updateInputShape(*g_app);
        refreshSettingsWindow(*g_app);
        return TRUE;
    }
    if (evt->button == 1 && g_app->isDragging) {
        g_app->isDragging = false;
        g_app->physics.setDragging(false, 0.0f);
        gtk_window_get_position(GTK_WINDOW(g_app->window), &g_app->settings.posX, &g_app->settings.posY);
        g_app->settings.save();
        refreshSettingsWindow(*g_app);
        return TRUE;
    }
    return FALSE;
}

static void refreshWindowGeometry(LinuxApp& app) {
    if (!app.window) return;
    gtk_window_get_position(GTK_WINDOW(app.window), &app.winX, &app.winY);
    gtk_window_get_size(GTK_WINDOW(app.window), &app.winW, &app.winH);
    app.lastGeometryRefresh = nowSeconds();
}

/// Adaptive frame pacing. The old fixed 60fps redraw of a translucent always-on-top window made
/// software-composited desktops (统信 UOS / 麒麟 on weak or 国产 GPUs, VMs) so slow that only
/// typing still worked. Now: 30fps while the user is typing/moving/petting, 20fps when idle,
/// 10fps when the cat is asleep, 4fps when hidden. "高帧率 60fps" in the menu restores 60fps.
static guint desiredFrameIntervalMs(const LinuxApp& app) {
    if (!app.isVisible || app.hiddenByFullscreen) return 250;
    if (app.settings.highFrameRate) return 16;
    double now = nowSeconds();
    bool busy = app.isDragging || app.demoActive || !app.input.activeKeys.empty()
        || app.input.leftMouseDown || app.input.rightMouseDown
        || (now - app.input.lastAnyInputTime()) < 2.5
        || !app.physics.floatingKeycaps.empty() || !app.physics.tabletTrail.empty()
        || app.physics.scruffDrag.value > 0.02f || app.physics.scruffDropTimer > 0.01f
        || app.physics.douniSlapTimer > 0.01f || app.physics.kbSparkTimer > 0.005f
        || std::fabs(app.physics.kbImpactY.value) > 0.01f
        || std::fabs(app.physics.kbImpactTilt.value) > 0.01f
        || (app.physics.douniLoafBlend.value > 0.01f && app.physics.douniLoafBlend.value < 0.99f)
        || std::fabs(app.physics.douniLoafBlend.velocity) > 0.02f
        || (app.physics.douniMouseBlend.value > 0.01f && app.physics.douniMouseBlend.value < 0.99f)
        || std::fabs(app.physics.douniMouseBlend.velocity) > 0.05f
        || std::fabs(app.physics.douniLeftRaise.velocity) > 0.05f
        || std::fabs(app.physics.douniRightRaise.velocity) > 0.05f
        || app.physics.huntIntensity.value > 0.02f
        || app.physics.dizzyIntensity.value > 0.02f
        || app.physics.angerIntensity.value > 0.02f
        || app.physics.petHeadIntensity > 0.02f || app.physics.petBodyIntensity > 0.02f;
    if (busy) return 33;
    if (app.physics.sleepiness > 0.6f) return 100;
    return 50;
}

static gboolean onFrameTick(gpointer);

static void scheduleFrames(guint intervalMs) {
    g_frameIntervalMs = intervalMs;
    g_timeout_add(intervalMs, onFrameTick, nullptr);
}

static gboolean onFrameTick(gpointer) {
    if (!g_app) return G_SOURCE_REMOVE;
    if (!g_app->isVisible) {
        guint want = desiredFrameIntervalMs(*g_app);
        if (want != g_frameIntervalMs) { scheduleFrames(want); return G_SOURCE_REMOVE; }
        return G_SOURCE_CONTINUE;
    }

    double now = nowSeconds();
    if (now - g_app->lastFullscreenCheck > 0.5) {
        g_app->lastFullscreenCheck = now;
        bool obsBg = (g_app->settings.obsCaptureMode > 0 && g_app->settings.obsHideDesktop);
        if (g_app->settings.autoHideFullscreen && !obsBg) {
            bool fs = isOtherWindowFullscreen(*g_app);
            if (fs && !g_app->hiddenByFullscreen) {
                g_app->hiddenByFullscreen = true;
                gtk_widget_hide(g_app->window);
            } else if (!fs && g_app->hiddenByFullscreen) {
                g_app->hiddenByFullscreen = false;
                if (g_app->isVisible) {
                    gtk_widget_show(g_app->window);
                    if (g_app->settings.alwaysOnTop) gtk_window_set_keep_above(GTK_WINDOW(g_app->window), TRUE);
                    g_app->lastTickTime = now;
                }
            }
        } else if (g_app->hiddenByFullscreen) {
            g_app->hiddenByFullscreen = false;
            if (g_app->isVisible) {
                gtk_widget_show(g_app->window);
                if (g_app->settings.alwaysOnTop) gtk_window_set_keep_above(GTK_WINDOW(g_app->window), TRUE);
                g_app->lastTickTime = now;
            }
        }
    }
    if (g_app->hiddenByFullscreen) {
        guint want = desiredFrameIntervalMs(*g_app);
        if (want != g_frameIntervalMs) { scheduleFrames(want); return G_SOURCE_REMOVE; }
        return G_SOURCE_CONTINUE;
    }
    float dt = (float)(now - g_app->lastTickTime);
    g_app->lastTickTime = now;
    if (dt <= 0.0f || dt > 0.5f) dt = 1.0f / 30.0f;

    // Fallback GDK cursor query + Alt check — only needed without XInput2 (Wayland / no XI2).
    // With XI2 the input thread already tracks the cursor, so skip these two X round-trips per frame.
    float fbX = 0.0f, fbY = 0.0f;
    bool hasFb = false;
    bool gdkAltHeld = false;
    if (!g_app->input.isXI2Active) {
        if (GdkDisplay* dpy = gdk_display_get_default()) {
            if (GdkSeat* seat = gdk_display_get_default_seat(dpy)) {
                if (GdkDevice* ptr = gdk_seat_get_pointer(seat)) {
                    gint gx = 0, gy = 0;
                    gdk_device_get_position(ptr, nullptr, &gx, &gy);
                    fbX = (float)gx; fbY = (float)gy; hasFb = true;
                    if (GdkWindow* gw = gtk_widget_get_window(g_app->window)) {
                        GdkModifierType mask = (GdkModifierType)0;
                        gdk_window_get_device_position(gw, ptr, nullptr, nullptr, &mask);
                        if (mask & GDK_MOD1_MASK) gdkAltHeld = true;
                    }
                }
            }
        }
    }

    g_app->input.pollAndDrain(dt, fbX, fbY, hasFb);

    // Hold Alt to temporarily unlock Full Click-Through so the user can always right-click/drag!
    bool altHeld = gdkAltHeld
        || g_app->input.activeKeys.count(PhysicalKey::LeftOption) > 0
        || g_app->input.activeKeys.count(PhysicalKey::RightOption) > 0;
    if (altHeld != g_app->clickThroughTemporarilyUnlocked) {
        g_app->clickThroughTemporarilyUnlocked = altHeld;
        if (g_app->settings.fullClickThrough) {
            updateInputShape(*g_app);
        }
    }

    if (g_app->demoActive) {
        static const PhysicalKey demoKeys[] = {
            PhysicalKey::W, PhysicalKey::A, PhysicalKey::S, PhysicalKey::D,
            PhysicalKey::F, PhysicalKey::J, PhysicalKey::O, PhysicalKey::E,
            PhysicalKey::Space, PhysicalKey::LeftShift
        };
        int targetIdx = (int)((now - g_app->demoStartTime) / 0.13);
        while (g_app->demoStepIdx <= targetIdx && g_app->demoStepIdx < 10) {
            g_app->physics.triggerKeyDown(demoKeys[g_app->demoStepIdx], g_app->settings, activeSkin(*g_app).typingStyle, &g_app->input);
            g_app->demoStepIdx++;
        }
        if (g_app->demoStepIdx >= 10) g_app->demoActive = false;
    }

    // Geometry comes from configure-event; re-sync occasionally as a safety net.
    if (now - g_app->lastGeometryRefresh > 3.0) refreshWindowGeometry(*g_app);
    int winX = g_app->winX, winY = g_app->winY, winW = g_app->winW, winH = g_app->winH;

    // Petting detection using global cursor position (works in Smart AND Full Click-Through modes!)
    if (!g_app->isDragging && !g_app->isRotating && g_app->activeResizeCorner < 0 && winW > 0 && winH > 0) {
        Pt cur = g_app->input.lastCursorPos;
        if (cur.x >= winX && cur.x <= winX + winW && cur.y >= winY && cur.y <= winY + winH) {
            Pt rawCp = {
                (cur.x - (float)winX) / (float)winW * CatPhysics::CanvasWidth,
                (cur.y - (float)winY) / (float)winH * CatPhysics::CanvasHeight
            };
            Pt cp = CatRenderer::canvasPointToCatSpace(rawCp, g_app->settings.flipHorizontal, g_app->settings.rotationDeg);
            CatHitZone zone = CatRenderer::hitTest(cp, g_app->physics, activeSkin(*g_app));
            g_app->physics.updateTailHover(&cp);
            if (zone == CatHitZone::Head) {
                bool skipPetForHunt = (activeSkin(*g_app).renderStyle == SkinRenderStyle::Douni
                    && g_app->physics.huntIntensity.value > 0.20f);
                if (!skipPetForHunt && g_app->hasLastHover) {
                    float d = std::hypot(cp.x - g_app->lastHoverCanvas.x, cp.y - g_app->lastHoverCanvas.y);
                    if (d > 1.2f && d < 80.0f) {
                        g_app->physics.registerPetStroke(cp, d, true, g_app->settings);
                    }
                }
                g_app->lastHoverCanvas = cp;
                g_app->hasLastHover = true;
            } else {
                g_app->hasLastHover = false;
            }
        } else {
            g_app->hasLastHover = false;
            g_app->physics.updateTailHover(nullptr);
        }
    }

    // Physics clamps dt to 1/25s; sub-step slower idle frames so animations keep real-time speed.
    int substeps = std::max(1, std::min(4, (int)std::ceil(dt / 0.034f)));
    const SkinSpec& curSkin = activeSkin(*g_app);
    for (int i = 0; i < substeps; ++i) {
        g_app->physics.step(dt / (float)substeps, (float)winX, (float)winY, (float)winW,
                            g_app->input, g_app->settings, curSkin.typingStyle, curSkin.renderStyle);
    }
    gtk_widget_queue_draw(g_app->drawArea);

    guint want = desiredFrameIntervalMs(*g_app);
    if (want != g_frameIntervalMs) {
        scheduleFrames(want);
        return G_SOURCE_REMOVE;
    }
    return G_SOURCE_CONTINUE;
}

static gboolean onQuitSignal(gpointer) {
    releaseSingleInstance();
    gtk_main_quit();
    return G_SOURCE_REMOVE;
}

/// SIGUSR1 = "KuroCat was launched again": show the cat, cancel full click-through, open the menu.
static gboolean onWakeSignal(gpointer) {
    if (!g_app || !g_app->window) return G_SOURCE_CONTINUE;
    if (g_app->settings.fullClickThrough) {
        g_app->settings.fullClickThrough = false;
        g_app->settings.save();
        updateInputShape(*g_app);
    }
    if (!g_app->isVisible) {
        g_app->isVisible = true;
        gtk_widget_show(g_app->window);
    }
    showPopupMenu(*g_app, nullptr);
    return G_SOURCE_CONTINUE;
}

int main(int argc, char** argv) {
    if (argc > 1 && (std::strcmp(argv[1], "--version") == 0 || std::strcmp(argv[1], "-v") == 0)) {
        std::printf("KuroCat %s\n", APP_VERSION);
        return 0;
    }
    if (!std::getenv("KUROCAT_NO_REDIRECT")) redirectToNewestInstalledCopy(argv);
    unsetenv("KUROCAT_NO_REDIRECT");

    // The XInput2 reader runs on its own thread with its own Display. libX11 < 1.8 (统信 UOS 20,
    // 麒麟 V10, Debian 10/11) is not thread-safe unless this is called before any other Xlib call.
    XInitThreads();
    // Positioning, keep-above and click-through shapes need X11 (XWayland on Wayland sessions).
    gdk_set_allowed_backends("x11,*");
    gtk_init(&argc, &argv);

    LinuxApp app;
    g_app = &app;

    // Install signal handlers BEFORE publishing our PID in the lock file (SIGUSR1 would otherwise
    // kill us by default if a second launch pokes us during startup).
    g_unix_signal_add(SIGTERM, onQuitSignal, nullptr);
    g_unix_signal_add(SIGINT, onQuitSignal, nullptr);
    g_unix_signal_add(SIGHUP, onQuitSignal, nullptr);
    g_unix_signal_add(SIGUSR1, onWakeSignal, nullptr);
    if (!acquireSingleInstance()) {
        g_app = nullptr;
        return 0;
    }

    app.settings.load();
    app.skins.reload();
    app.kbLibrary.reload();
    for (int i = 1; i < argc; ++i) {
        if (!argv[i] || argv[i][0] == '-') continue;
        std::string arg = argv[i];
        if (KeyboardSkinLibrary::looksLikeKeyboardFile(arg) || hasSuffixCI(arg, ".kurocat")) {
            importDroppedFile(app, arg, true);
        }
    }
    if (isAutoStartEnabled()) {
        // Keep Exec path updated if user moved portable kurocat binary
        setAutoStartEnabled(true);
    }

    app.input.onKeyDown = [&](PhysicalKey k) { app.physics.triggerKeyDown(k, app.settings, activeSkin(app).typingStyle, &app.input); };
    app.input.onMouseClick = [&](int btn, bool down) { app.physics.triggerMouseClick(btn, down, app.settings, activeSkin(app).renderStyle); };
    app.input.start();

    app.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(app.window), "KuroCat");
    gtk_window_set_decorated(GTK_WINDOW(app.window), FALSE);
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(app.window), app.settings.obsCaptureMode == 0);
    gtk_window_set_skip_pager_hint(GTK_WINDOW(app.window), app.settings.obsCaptureMode == 0);
    gtk_window_set_accept_focus(GTK_WINDOW(app.window), FALSE);
    gtk_window_set_type_hint(GTK_WINDOW(app.window), app.settings.obsCaptureMode == 0 ? GDK_WINDOW_TYPE_HINT_UTILITY : GDK_WINDOW_TYPE_HINT_NORMAL);
    gtk_window_stick(GTK_WINDOW(app.window));
    gtk_widget_set_app_paintable(app.window, TRUE);

    GdkScreen* screen = gtk_widget_get_screen(app.window);
    if (GdkVisual* visual = gdk_screen_get_rgba_visual(screen)) {
        gtk_widget_set_visual(app.window, visual);
    }

    app.drawArea = gtk_drawing_area_new();
    gtk_container_add(GTK_CONTAINER(app.window), app.drawArea);

    gtk_widget_add_events(app.window, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK | GDK_STRUCTURE_MASK);
    g_signal_connect(app.drawArea, "draw", G_CALLBACK(onDraw), nullptr);
    g_signal_connect(app.window, "button-press-event", G_CALLBACK(onButtonPress), nullptr);
    g_signal_connect(app.window, "motion-notify-event", G_CALLBACK(onMotionNotify), nullptr);
    g_signal_connect(app.window, "button-release-event", G_CALLBACK(onButtonRelease), nullptr);
    g_signal_connect(app.window, "configure-event", G_CALLBACK(+[](GtkWidget*, GdkEventConfigure*, gpointer) -> gboolean {
        if (g_app) refreshWindowGeometry(*g_app);
        return FALSE;
    }), nullptr);
    g_signal_connect(app.window, "destroy", G_CALLBACK(gtk_main_quit), nullptr);
    enableSkinDrop(app.window, true, true); // .kurocat / .kurokb / keyboard.json dropped onto the cat is imported and worn right away
    g_signal_connect(app.window, "realize", G_CALLBACK(+[](GtkWidget* w, gpointer) {
        updateInputShape(*g_app);
        if (GdkWindow* gw = gtk_widget_get_window(w)) {
            GdkCursor* hand = gdk_cursor_new_from_name(gdk_window_get_display(gw), "grab");
            if (hand) {
                gdk_window_set_cursor(gw, hand);
                g_object_unref(hand);
            }
        }
    }), nullptr);

    // System tray icon (works cleanly on 统信 UOS, Deepin, KDE, XFCE, MATE!)
    G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    if (GdkPixbuf* pb = createCatTrayPixbuf()) {
        app.trayIcon = gtk_status_icon_new_from_pixbuf(pb);
        gtk_status_icon_set_tooltip_text(app.trayIcon, "KuroCat 黑猫键盘桌宠 (右键设置)");
        g_signal_connect(app.trayIcon, "popup-menu", G_CALLBACK(+[](GtkStatusIcon*, guint, guint, gpointer) {
            showPopupMenu(*g_app, nullptr);
        }), nullptr);
        g_signal_connect(app.trayIcon, "activate", G_CALLBACK(+[](GtkStatusIcon*, gpointer) {
            showPopupMenu(*g_app, nullptr);
        }), nullptr);
        g_object_unref(pb);
    }
    G_GNUC_END_IGNORE_DEPRECATIONS

    applyWindowScaleAndPos(app, false);
    gtk_widget_show_all(app.window);
    refreshWindowGeometry(app);

    app.lastTickTime = nowSeconds();
    scheduleFrames(desiredFrameIntervalMs(app));
    startAsyncUpdateCheck(false, 2500);

    gtk_main();
    app.input.stop();
    releaseSingleInstance();
    g_app = nullptr;
    return 0;
}
