// What the launcher asks of the system beyond SDL: disk space, a file manager with the log selected, one instance,
// the dark title bar, the settings file. Windows first; on Linux the simple equivalents.
#include "launcher.h"
#include <cstring>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <shlobj.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>
#endif

#ifdef _WIN32
static std::wstring wide(const std::string &s) {
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(n > 0 ? n - 1 : 0, L'\0');
    if (n > 1)
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n);
    return w;
}
#endif

std::string exe_dir() {
    std::string d = SDL_GetBasePath() ? SDL_GetBasePath() : "./";
    while (d.size() > 1 && (d.back() == '/' || d.back() == '\\'))
        d.pop_back();
    return d;
}

std::string path_join(const std::string &a, const std::string &b) {
#ifdef _WIN32
    const char sep = '\\';
#else
    const char sep = '/';
#endif
    if (a.empty())
        return b;
    return a.back() == '/' || a.back() == '\\' ? a + b : a + sep + b;
}

static bool path_type(const std::string &p, SDL_PathType want) {
    SDL_PathInfo info;
    return SDL_GetPathInfo(p.c_str(), &info) && info.type == want;
}
bool path_is_dir(const std::string &p) { return path_type(p, SDL_PATHTYPE_DIRECTORY); }
bool path_is_file(const std::string &p) { return path_type(p, SDL_PATHTYPE_FILE); }

static std::string existing_ancestor(std::string p) {   // the folder may not exist yet: its nearest parent that does
    while (!p.empty() && !path_is_dir(p)) {
        size_t cut = p.find_last_of("/\\");
        if (cut == std::string::npos || cut == 0)
            break;
        p.resize(cut);
        if (p.size() == 2 && p[1] == ':')
            p += '\\';
    }
    return p;
}

double disk_free_gb(const std::string &folder) {
    std::string p = existing_ancestor(folder);
#ifdef _WIN32
    ULARGE_INTEGER free;
    if (!GetDiskFreeSpaceExW(wide(p).c_str(), &free, nullptr, nullptr))
        return -1;
    return free.QuadPart / 1073741824.0;
#else
    struct statvfs s;
    if (statvfs(p.c_str(), &s) != 0)
        return -1;
    return (double)s.f_bavail * s.f_frsize / 1073741824.0;
#endif
}

std::string drive_of(const std::string &folder) {
#ifdef _WIN32
    if (folder.size() >= 2 && folder[1] == ':')
        return folder.substr(0, 2);
#endif
    return existing_ancestor(folder);
}

void reveal_in_folder(const std::string &file) {
#ifdef _WIN32
    std::wstring w = wide(file);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    PIDLIST_ABSOLUTE pidl = ILCreateFromPathW(w.c_str());
    if (pidl) {
        HRESULT hr = SHOpenFolderAndSelectItems(pidl, 0, nullptr, 0);
        ILFree(pidl);
        if (SUCCEEDED(hr))
            return;
    }
    std::wstring dir = w.substr(0, w.find_last_of(L"\\/"));
    ShellExecuteW(nullptr, L"open", dir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#else
    // [temporary] Linux: the folder only; the file selected needs D-Bus org.freedesktop.FileManager1.ShowItems
    std::string dir = file.substr(0, file.find_last_of('/'));
    SDL_OpenURL(("file://" + dir).c_str());
#endif
}

bool single_instance(const char *title) {
#ifdef _WIN32
    CreateMutexW(nullptr, TRUE, L"Local\\UltraTearsLauncher");
    if (GetLastError() != ERROR_ALREADY_EXISTS)
        return true;
    HWND other = FindWindowW(nullptr, wide(title).c_str());
    if (other) {
        ShowWindow(other, SW_RESTORE);
        SetForegroundWindow(other);
    }
    return false;
#else
    (void)title;
    const char *run = SDL_getenv("XDG_RUNTIME_DIR");
    std::string lock = std::string(run ? run : "/tmp") + "/ultratears-launcher.lock";
    int fd = open(lock.c_str(), O_CREAT | O_RDWR, 0600);
    return fd < 0 || flock(fd, LOCK_EX | LOCK_NB) == 0;   // the descriptor stays open while the launcher runs
#endif
}

void window_dark_title(SDL_Window *w) {
#ifdef _WIN32
    HWND hwnd = (HWND)SDL_GetPointerProperty(SDL_GetWindowProperties(w), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
    if (!hwnd)
        return;
    BOOL dark = TRUE;
    DwmSetWindowAttribute(hwnd, 20, &dark, sizeof dark);   // DWMWA_USE_IMMERSIVE_DARK_MODE (Windows 10 20H1+)
    COLORREF caption = RGB(15, 15, 15);
    DwmSetWindowAttribute(hwnd, 35, &caption, sizeof caption);   // DWMWA_CAPTION_COLOR (Windows 11; ignored on 10)
#else
    (void)w;
#endif
}

Lang system_lang() {
    int n = 0;
    SDL_Locale **loc = SDL_GetPreferredLocales(&n);
    Lang l = LANG_EN;
    if (loc && n > 0 && loc[0]->language)
        l = lang_from_code(loc[0]->language);
    SDL_free(loc);
    return l;
}

int logical_cpus() { return SDL_max(1, SDL_GetNumLogicalCPUCores()); }

// ---- settings: ultratears-launcher.ini next to the launcher, key=value lines in UTF-8
static std::string settings_path() { return path_join(exe_dir(), "ultratears-launcher.ini"); }

Settings settings_load() {
    Settings s;
    size_t size = 0;
    char *text = (char *)SDL_LoadFile(settings_path().c_str(), &size);
    if (!text)
        return s;
    std::string all(text, size);
    SDL_free(text);
    size_t pos = 0;
    while (pos < all.size()) {
        size_t end = all.find('\n', pos);
        if (end == std::string::npos)
            end = all.size();
        std::string line = all.substr(pos, end - pos);
        pos = end + 1;
        while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
            line.pop_back();
        size_t eq = line.find('=');
        if (eq == std::string::npos || line[0] == '#')
            continue;
        std::string k = line.substr(0, eq), v = line.substr(eq + 1);
        if (k == "lang")
            s.lang = v;
        else if (k == "dump")
            s.dump = v;
        else if (k == "install")
            s.install = v;
    }
    return s;
}

void settings_save(const Settings &s) {
    std::string text = "# UltraTears Launcher\nlang=" + s.lang + "\ndump=" + s.dump + "\ninstall=" + s.install + "\n";
    SDL_SaveFile(settings_path().c_str(), text.data(), text.size());
}

bool start_game(const std::string &folder) {
    std::string exe = path_join(folder, "UltraTears.exe");
#ifdef _WIN32
    std::wstring cmd = L"\"" + wide(exe) + L"\"";
    STARTUPINFOW si = {sizeof si};
    PROCESS_INFORMATION pi;
    if (!CreateProcessW(nullptr, &cmd[0], nullptr, nullptr, FALSE, DETACHED_PROCESS, nullptr, wide(folder).c_str(), &si, &pi))
        return false;
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
#else
    pid_t pid = fork();
    if (pid == 0) {
        if (chdir(folder.c_str()) == 0)
            execl(exe.c_str(), exe.c_str(), (char *)nullptr);
        _exit(127);
    }
    return pid > 0;
#endif
}
