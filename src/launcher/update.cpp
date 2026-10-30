// Updates of the builder package from GitHub Releases: at most one question a day (ultratears-launcher.ini, updates=off
// turns it off), the newest release's builder zip downloaded into update\ of the package, its size and SHA-256 (the
// digest GitHub keeps for every release file) checked before tools/apply_update.py puts it in place.
// [temporary] Linux: no HTTP client yet (WinHTTP on Windows); the Linux build will need one (libcurl or the curl tool).
#include "launcher.h"
#include <atomic>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include <winhttp.h>
#endif

static std::wstring wide(const std::string &s) {
#ifdef _WIN32
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(n > 0 ? n - 1 : 0, L'\0');
    if (n > 1)
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n);
    return w;
#else
    return std::wstring(s.begin(), s.end());
#endif
}

// GET url (https, redirects followed); sink gets the body in pieces and returns false to stop. total: Content-Length.
template <class Sink> static bool http_get(const std::string &url, Sink sink, uint64_t *total, std::string &err) {
#ifdef _WIN32
    std::wstring w = wide(url);
    URL_COMPONENTS u = {sizeof u};
    wchar_t host[256], path[2048];
    u.lpszHostName = host, u.dwHostNameLength = 256;
    u.lpszUrlPath = path, u.dwUrlPathLength = 2048;
    wchar_t extra[2048];
    u.lpszExtraInfo = extra, u.dwExtraInfoLength = 2048;
    if (!WinHttpCrackUrl(w.c_str(), 0, 0, &u)) {
        err = "bad url";
        return false;
    }
    std::wstring object = std::wstring(path, u.dwUrlPathLength) + std::wstring(extra, u.dwExtraInfoLength);
    HINTERNET s = WinHttpOpen(L"UltraTears-Launcher", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, nullptr, nullptr, 0);
    if (!s)   // before Windows 8.1
        s = WinHttpOpen(L"UltraTears-Launcher", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, nullptr, nullptr, 0);
    HINTERNET c = s ? WinHttpConnect(s, std::wstring(host, u.dwHostNameLength).c_str(), u.nPort, 0) : nullptr;
    HINTERNET r = c ? WinHttpOpenRequest(c, L"GET", object.c_str(), nullptr, nullptr, nullptr,
                                         u.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0) : nullptr;
    bool ok = false;
    if (r && WinHttpSendRequest(r, L"Accept: application/vnd.github+json\r\n", (DWORD)-1, nullptr, 0, 0, 0) &&
        WinHttpReceiveResponse(r, nullptr)) {
        DWORD status = 0, len = sizeof status;
        WinHttpQueryHeaders(r, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &status, &len, nullptr);
        wchar_t cl[32];
        DWORD cl_len = sizeof cl;
        if (total)
            *total = WinHttpQueryHeaders(r, WINHTTP_QUERY_CONTENT_LENGTH, nullptr, cl, &cl_len, nullptr) ? _wcstoui64(cl, nullptr, 10) : 0;
        if (status != 200) {
            err = "HTTP " + std::to_string(status);
        } else {
            ok = true;
            std::vector<char> buf(1 << 16);
            for (;;) {
                DWORD n = 0;
                if (!WinHttpReadData(r, buf.data(), (DWORD)buf.size(), &n)) {
                    ok = false, err = "connection lost (" + std::to_string(GetLastError()) + ")";
                    break;
                }
                if (n == 0)
                    break;
                if (!sink(buf.data(), (size_t)n)) {
                    ok = false, err = "stopped";
                    break;
                }
            }
        }
    } else {
        err = "no connection (" + std::to_string(GetLastError()) + ")";
    }
    for (HINTERNET h : {r, c, s})
        if (h)
            WinHttpCloseHandle(h);
    return ok;
#else
    (void)url, (void)sink, (void)total;
    err = "not available on this system yet";
    return false;
#endif
}

// "0.3.0" from "v0.3.0-beta" / "UltraTears 0.3.0 (commit …)"; compared as numbers
static bool parse_version(const std::string &s, int v[3]) {
    size_t i = s.find_first_of("0123456789");
    if (i == std::string::npos)
        return false;
    return SDL_sscanf(s.c_str() + i, "%d.%d.%d", &v[0], &v[1], &v[2]) == 3;
}

static bool newer(const std::string &a, const std::string &b) {   // a > b
    int x[3], y[3];
    if (!parse_version(a, x) || !parse_version(b, y))
        return false;
    for (int i = 0; i < 3; i++)
        if (x[i] != y[i])
            return x[i] > y[i];
    return false;
}

std::string package_version(const std::string &root) {
    size_t n = 0;
    char *t = (char *)SDL_LoadFile(path_join(root, "VERSION.txt").c_str(), &n);
    if (!t)
        return "";
    std::string s(t, n);
    SDL_free(t);
    int v[3];
    if (s.rfind("UltraTears ", 0) != 0 || !parse_version(s, v))
        return "";
    return std::to_string(v[0]) + "." + std::to_string(v[1]) + "." + std::to_string(v[2]);
}

// ---------------------------------------------------------------- the question to GitHub, on a thread
static struct {
    SDL_Mutex *lock = SDL_CreateMutex();
    std::atomic<int> check{0};   // 0 idle, 1 asking, 2 answered
    UpdateInfo found;
    bool has = false;
    std::string current;
    // the download
    std::atomic<int> dl{0};      // 0 idle, 1 running, 2 done, 3 failed
    std::atomic<uint64_t> got{0}, total{0};
    std::atomic<bool> stop{false};
    std::string error, zip;
    UpdateInfo want;
    std::string dir;
} G;

static int check_thread(void *) {
    std::string body, err;
    bool ok = http_get(std::string("https://api.github.com/repos/") + ULTRS_REPO_PATH + "/releases?per_page=10",
                       [&](const void *p, size_t n) { body.append((const char *)p, n); return body.size() < (8u << 20); },
                       nullptr, err);
    Json list;
    UpdateInfo best;
    if (ok && json_parse(body, list)) {
        for (size_t i = 0; i < list.size(); i++) {   // newest first; pre-releases count (the port is in beta)
            const Json &r = list[i];
            if (r["draft"].truth())
                continue;
            std::string tag = r["tag_name"].str();
            if (!newer(tag, G.current) || (best.version.size() && !newer(tag, best.version)))
                continue;
            const Json &assets = r["assets"];
            for (size_t k = 0; k < assets.size(); k++) {
                std::string name = assets[k]["name"].str(), digest = assets[k]["digest"].str();
                if (name.find("-builder-") == std::string::npos || name.find("windows-x86_64.zip") == std::string::npos ||
                    digest.rfind("sha256:", 0) != 0)   // no hash, no update
                    continue;
                int v[3];
                parse_version(tag, v);
                best.version = std::to_string(v[0]) + "." + std::to_string(v[1]) + "." + std::to_string(v[2]);
                best.name = name;
                best.url = assets[k]["browser_download_url"].str();
                best.size = (uint64_t)assets[k]["size"].num();
                best.sha256 = digest.substr(7);
                best.page = r["html_url"].str();
            }
        }
    }
    SDL_LockMutex(G.lock);
    G.found = best;
    G.has = !best.version.empty();
    SDL_UnlockMutex(G.lock);
    if (!ok)
        SDL_Log("update check: %s", err.c_str());
    G.check = 2;
    return 0;
}

void update_check_start(const std::string &current) {
    if (G.check == 1 || current.empty())
        return;
    G.current = current;
    G.check = 1;
    SDL_DetachThread(SDL_CreateThread(check_thread, "update-check", nullptr));
}

int update_check_poll(UpdateInfo *out) {
    if (G.check != 2)
        return 0;
    G.check = 0;
    SDL_LockMutex(G.lock);
    bool has = G.has;
    if (has && out)
        *out = G.found;
    SDL_UnlockMutex(G.lock);
    return has ? 1 : -1;
}

// ---------------------------------------------------------------- the download
#ifdef _WIN32
struct Sha256 {
    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_HASH_HANDLE h = nullptr;
    Sha256() {
        BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
        BCryptCreateHash(alg, &h, nullptr, 0, nullptr, 0, 0);
    }
    void add(const void *p, size_t n) { BCryptHashData(h, (PUCHAR)p, (ULONG)n, 0); }
    std::string hex() {
        unsigned char d[32];
        BCryptFinishHash(h, d, 32, 0);
        char s[65];
        for (int i = 0; i < 32; i++)
            SDL_snprintf(s + 2 * i, 3, "%02x", d[i]);
        return s;
    }
    ~Sha256() {
        if (h)
            BCryptDestroyHash(h);
        if (alg)
            BCryptCloseAlgorithmProvider(alg, 0);
    }
};
#else
struct Sha256 {   // [temporary] Linux: with its HTTP client
    void add(const void *, size_t) {}
    std::string hex() { return ""; }
};
#endif

static int download_thread(void *) {
    std::string part = path_join(G.dir, G.want.name + ".part"), zip = path_join(G.dir, G.want.name);
    SDL_CreateDirectory(G.dir.c_str());
    SDL_IOStream *f = SDL_IOFromFile(part.c_str(), "wb");
    std::string err;
    bool ok = false;
    if (!f) {
        err = SDL_GetError();
    } else {
        Sha256 sha;
        uint64_t total = 0;
        ok = http_get(G.want.url, [&](const void *p, size_t n) {
            if (G.stop || SDL_WriteIO(f, p, n) != n)
                return false;
            sha.add(p, n);
            G.got += n;
            return true;
        }, &total, err);
        if (!SDL_CloseIO(f) && ok)
            ok = false, err = "cannot write " + part;
        if (ok && G.got != G.want.size)
            ok = false, err = "the size is wrong: " + std::to_string((uint64_t)G.got) + " bytes, expected " + std::to_string(G.want.size);
        if (ok) {
            std::string h = sha.hex();
            if (h != G.want.sha256)
                ok = false, err = "the SHA-256 is wrong: " + h;
        }
        if (ok && !SDL_RenamePath(part.c_str(), zip.c_str()))
            ok = false, err = SDL_GetError();
        if (!ok)
            SDL_RemovePath(part.c_str());
    }
    SDL_LockMutex(G.lock);
    G.error = err;
    G.zip = zip;
    SDL_UnlockMutex(G.lock);
    G.dl = ok ? 2 : 3;
    return 0;
}

void update_download_start(const UpdateInfo &u, const std::string &dir) {
    if (G.dl == 1)
        return;
    G.want = u, G.dir = dir;
    G.got = 0, G.total = u.size, G.stop = false;
    G.dl = 1;
    SDL_DetachThread(SDL_CreateThread(download_thread, "update-download", nullptr));
}

int update_download_poll(uint64_t *got, uint64_t *total, std::string *zip_or_error) {
    if (got)
        *got = G.got;
    if (total)
        *total = G.total;
    int st = G.dl;
    if (st >= 2 && zip_or_error) {
        SDL_LockMutex(G.lock);
        *zip_or_error = st == 2 ? G.zip : G.error;
        SDL_UnlockMutex(G.lock);
    }
    return st;
}

void update_download_cancel() { G.stop = true; }

// The files apply_update.py moved aside (in use while it ran) and the downloaded zip: deleted at the next start.
void update_cleanup(const std::string &root) {
    std::string dir = path_join(root, "update"), list = path_join(dir, "cleanup.txt");
    size_t n = 0;
    char *t = (char *)SDL_LoadFile(list.c_str(), &n);
    std::string left;
    if (t) {
        std::string s(t, n);
        SDL_free(t);
        size_t pos = 0;
        while (pos < s.size()) {
            size_t e = s.find('\n', pos);
            if (e == std::string::npos)
                e = s.size();
            std::string p = s.substr(pos, e - pos);
            pos = e + 1;
            if (!p.empty() && p.back() == '\r')
                p.pop_back();
            if (p.empty() || p.find(".old-update") == std::string::npos)   // only what apply_update.py moved aside
                continue;
            for (int i = 0; i < 20 && path_is_file(p) && !SDL_RemovePath(p.c_str()); i++)
                SDL_Delay(100);   // the launcher that restarted us may still be closing
            if (path_is_file(p))
                left += p + "\n";
        }
    }
    if (left.empty()) {
        SDL_RemovePath(list.c_str());
        SDL_EnumerateDirectory(dir.c_str(), [](void *d, const char *, const char *name) {
            std::string f = path_join(*(std::string *)d, name);
            if (path_is_file(f) && (SDL_strstr(name, ".zip") || SDL_strstr(name, ".log")))
                SDL_RemovePath(f.c_str());
            return SDL_ENUM_CONTINUE;
        }, &dir);
        SDL_RemovePath(dir.c_str());   // only when empty
    } else {
        SDL_SaveFile(list.c_str(), left.data(), left.size());
    }
}
