// The launcher's screens (Nintendo/launcher-design): the dump folder, the build's progress, the hardware question, the
// failure, and Play once the game is built. Layout in design pixels (800x480), multiplied by the window's scale.
// Every clickable thing has a hover and a pressed look and the hand cursor; Tab / the gamepad move between them.
#include "launcher.h"
#include <cmath>
#include <cstring>

// ---------------------------------------------------------------- look
static const ImU32 C_BG = IM_COL32(12, 12, 12, 255);
static const ImU32 C_TEXT = IM_COL32(242, 242, 242, 255);
static const ImU32 C_SUB = IM_COL32(150, 150, 150, 255);
static const ImU32 C_DIM = IM_COL32(110, 110, 110, 255);
static const ImU32 C_CARD = IM_COL32(28, 28, 28, 255);
static const ImU32 C_CARD_HOVER = IM_COL32(36, 36, 36, 255);
static const ImU32 C_DARK = IM_COL32(46, 46, 46, 255);
static const ImU32 C_DARK_HOVER = IM_COL32(60, 60, 60, 255);
static const ImU32 C_DARK_DOWN = IM_COL32(38, 38, 38, 255);
static const ImU32 C_LIGHT = IM_COL32(236, 236, 236, 255);
static const ImU32 C_LIGHT_HOVER = IM_COL32(255, 255, 255, 255);
static const ImU32 C_LIGHT_DOWN = IM_COL32(210, 210, 210, 255);
static const ImU32 C_GREEN = IM_COL32(32, 122, 96, 255);
static const ImU32 C_GREEN_HOVER = IM_COL32(38, 145, 113, 255);
static const ImU32 C_GREEN_DOWN = IM_COL32(27, 104, 82, 255);
static const ImU32 C_BAR = IM_COL32(52, 52, 52, 255);
static const ImU32 C_BAR_FILL = IM_COL32(61, 214, 140, 255);
static const ImU32 C_BAR_FAIL = IM_COL32(96, 96, 96, 255);
static const ImU32 C_OK = IM_COL32(64, 179, 150, 255);
static const ImU32 C_ERR = IM_COL32(205, 109, 97, 255);
static const ImU32 C_PENDING = IM_COL32(58, 58, 58, 255);

static float S = 1;          // the window's scale
static ImVec2 O;             // where design pixel (0, 0) of the current region is on the screen
static ImDrawList *DL;
static SDL_Window *g_win;

static ImVec2 P(float x, float y) { return ImVec2(O.x + x * S, O.y + y * S); }

enum Style { DARK, LIGHT, PRIMARY, CARD };

static bool hovered_rect(ImVec2 a, ImVec2 b) {
    return ImGui::IsMouseHoveringRect(a, b) && ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
}

// An invisible ImGui item over [x, y, w, h] (design pixels): focus, keyboard and gamepad come with it.
static bool hit(const char *id, float x, float y, float w, float h, bool *hover, bool *down, bool enabled = true) {
    ImGui::SetCursorScreenPos(P(x, y));
    ImGui::PushID(id);
    if (!enabled)
        ImGui::BeginDisabled();
    bool clicked = ImGui::InvisibleButton("##hit", ImVec2(w * S, h * S));
    if (!enabled)
        ImGui::EndDisabled();
    ImGui::PopID();
    *hover = enabled && ImGui::IsItemHovered();
    *down = enabled && ImGui::IsItemActive();
    if (*hover)
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    if (enabled && ImGui::IsItemFocused() && ImGui::GetIO().NavVisible)   // keyboard / gamepad focus
        DL->AddRect(P(x - 3, y - 3), P(x + w + 3, y + h + 3), IM_COL32(61, 214, 140, 160), 9 * S, 0, 2 * S);
    return clicked;
}

static ImVec2 text_size(ImFont *f, const char *t) { return f->CalcTextSizeA(f->FontSize, FLT_MAX, 0, t); }

static void text(ImFont *f, float x, float y, ImU32 col, const char *t) { DL->AddText(f, f->FontSize, P(x, y), col, t); }

static float text_w(ImFont *f, const char *t) { return text_size(f, t).x / S; }   // in design pixels

// A path within width (design pixels): its middle left out ("D:\Games\…\game"); the whole path on hover.
static void path_text(ImFont *f, float x, float y, float width, const std::string &path, float *drawn_w = nullptr) {
    std::string s = path;
    if (text_w(f, s.c_str()) > width) {
        std::vector<size_t> cuts;   // where UTF-8 characters start
        for (size_t i = 0; i < path.size(); i++)
            if ((path[i] & 0xC0) != 0x80)
                cuts.push_back(i);
        size_t head = cuts.size() / 2, tail = cuts.size() / 2;
        while (head > 1) {
            s = path.substr(0, cuts[head]) + "…" + path.substr(cuts[cuts.size() - tail]);
            if (text_w(f, s.c_str()) <= width)
                break;
            head--, tail = tail > 1 ? tail - 1 : 1;
        }
        if (hovered_rect(P(x, y), P(x + width, y + f->FontSize / S)))
            ImGui::SetTooltip("%s", path.c_str());
    }
    text(f, x, y, C_TEXT, s.c_str());
    if (drawn_w)
        *drawn_w = text_w(f, s.c_str());
}

static bool button(const char *id, const char *label, float x, float y, float w, float h, Style st, bool enabled = true) {
    bool hov, down;
    bool clicked = hit(id, x, y, w, h, &hov, &down, enabled);
    ImU32 bg, fg = C_TEXT;
    switch (st) {
    case LIGHT: bg = down ? C_LIGHT_DOWN : hov ? C_LIGHT_HOVER : C_LIGHT; fg = IM_COL32(17, 17, 17, 255); break;
    case PRIMARY: bg = down ? C_GREEN_DOWN : hov ? C_GREEN_HOVER : C_GREEN; break;
    default: bg = down ? C_DARK_DOWN : hov ? C_DARK_HOVER : C_DARK; break;
    }
    if (!enabled)
        bg = C_DARK, fg = C_DIM;
    DL->AddRectFilled(P(x, y), P(x + w, y + h), bg, 8 * S);
    ImFont *f = g_media.medium;
    ImVec2 ts = text_size(f, label);
    DL->AddText(f, f->FontSize, ImVec2(P(x, y).x + (w * S - ts.x) / 2, P(x, y).y + (h * S - ts.y) / 2), fg, label);
    return clicked;
}

static void icon(const Icon &i, float x, float y, float size = 0) {
    if (!i.tex)
        return;
    float w = size ? size : i.w, h = size ? size : i.h;
    DL->AddImage((ImTextureID)(intptr_t)i.tex, P(x, y), P(x + w, y + h));
}

// A paragraph of runs, wrapped at word breaks within width; runs with a url are links (underlined on hover, open the
// url). Returns its height in design pixels.
struct Run { const char *text; ImU32 col; const char *url = nullptr; };

static float rich(ImFont *f, float x, float y, float width, std::initializer_list<Run> runs, float line_h = 0) {
    float lh = line_h ? line_h : f->FontSize / S * 1.45f;
    float cx = 0, cy = 0, space = text_w(f, " ");
    bool trailing = false;   // the run before ended with a space
    for (const Run &r : runs) {
        struct Word { float x, y, w; std::string s; };
        std::vector<Word> words;
        const char *p = r.text;
        bool lead_space = *p == ' ' || trailing;
        size_t len = SDL_strlen(p);
        trailing = len && p[len - 1] == ' ';
        while (*p) {
            while (*p == ' ')
                p++;
            if (!*p)
                break;
            const char *e = p;
            while (*e && *e != ' ')
                e++;
            std::string w(p, e);
            float ww = text_w(f, w.c_str());
            bool first = words.empty();
            float gap = (cx > 0 && (!first || lead_space)) ? space : 0;
            if (cx > 0 && cx + gap + ww > width) {
                cx = 0, cy += lh, gap = 0;
            }
            words.push_back({cx + gap, cy, ww, w});
            cx += gap + ww;
            p = e;
        }
        bool hov = false;
        if (r.url)
            for (const Word &w : words)
                if (hovered_rect(P(x + w.x, y + w.y), P(x + w.x + w.w, y + w.y + lh)))
                    hov = true;
        for (const Word &w : words) {
            text(f, x + w.x, y + w.y, r.col, w.s.c_str());
            if (r.url && hov)
                DL->AddLine(P(x + w.x, y + w.y + f->FontSize / S + 1), P(x + w.x + w.w, y + w.y + f->FontSize / S + 1), r.col, S);
        }
        if (r.url && hov) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                SDL_OpenURL(r.url);
        }
    }
    return cy + lh;
}

static void ring(float cx, float cy, float r, ImU32 col) { DL->AddCircle(P(cx, cy), r * S, col, 32, 1.6f * S); }
static void dot(float cx, float cy, float r, ImU32 col) { DL->AddCircleFilled(P(cx, cy), r * S, col, 32); }

static void globe(float x, float y, ImU32 col) {   // 14x14: a circle, the equator, a meridian ellipse
    ImVec2 c = P(x + 7, y + 7);
    float r = 6.2f * S, t = 1.2f * S;
    DL->AddCircle(c, r, col, 32, t);
    DL->AddLine(ImVec2(c.x - r, c.y), ImVec2(c.x + r, c.y), col, t);
    DL->AddEllipse(c, ImVec2(r * 0.45f, r), col, 0, 32, t);
}

// ---------------------------------------------------------------- state
enum Screen { SCR_DUMP, SCR_BUILD, SCR_HW, SCR_PLAY, SCR_UPDATE, SCR_BROKEN };
enum DumpState { D_NONE, D_CHECKING, D_BAD, D_OK };
enum RowState { R_PENDING, R_RUN, R_DONE, R_WARN, R_ERROR };

struct Row { const char *id, *name; RowState st = R_PENDING; double secs = -1; bool skipped = false; std::string err; };

static struct {
    Screen screen = SCR_DUMP;
    Settings set;
    std::string home;          // the package the launcher was started from (extracted from the zip)
    std::string install;       // where the game is built: home, or a folder the player chose (the package copied there)
    std::string root;          // the package that builds and updates: install once the copy is there, else home
    std::string dump;          // the folder with exefs and romfs (settings: dump)
    std::string version;       // of the package (VERSION.txt)
    // the dump folder screen
    DumpState dump_st = D_NONE;
    std::string dump_code;
    Json dump_args;
    SetupProc *check = nullptr;
    std::string checking;
    // the build
    SetupProc *build = nullptr;
    SetupProc *copy = nullptr;   // tools/install_to.py: the package into the install folder, before the build
    Row rows[6] = {{"check", "step_check"}, {"romfs", "step_romfs"}, {"lift", "step_lift"},
                   {"compile", "step_compile"}, {"shaders", "step_shaders"}, {"textures", "step_textures"}};
    double weight[8] = {};
    const char *weight_id[8] = {"tools", "check", "romfs", "lift", "compile", "shaders", "textures", "finish"};
    double frac[8] = {};      // each setup step's share done, 0..1
    double eta = -1;          // of the running step, from its progress events
    bool failed = false, done = false, cancel_q = false, close_after_cancel = false;
    std::string fail_text;
    Json hw;                  // the hardware cases
    bool hw_asked = false;
    // folder dialogs answer on another thread
    SDL_Mutex *lock = nullptr;
    std::string picked;
    int picked_for = 0;       // 1 the dump, 2 the install folder
    bool stopping = false;    // Stop pressed: the build is being killed
    // updates
    UpdateInfo upd;
    bool has_upd = false;
    int upd_st = 0;           // 0 asked to update? 1 downloading, 2 putting in place, 3 failed
    std::string upd_err;
    SetupProc *apply = nullptr;
    double apply_frac = 0;
} U;

static std::string log_path() { return path_join(U.install, "ultratears-setup.log"); }
static bool installed() { return path_is_file(path_join(U.install, "UltraTears.exe")) && path_is_file(path_join(U.install, "ultratears.cfg")); }
static bool started_before() { return path_is_file(path_join(U.install, "data/setup.json")) && !installed(); }
// built by an older package (an update came since): the next build redoes only what changed
static bool stale() { return installed() && !U.version.empty() && U.set.get("built") != U.version; }

static bool same_path(std::string a, std::string b) {
    for (std::string *s : {&a, &b}) {
        for (char &c : *s)
            c = c == '/' ? '\\' : (char)SDL_tolower((unsigned char)c);
        while (s->size() > 3 && s->back() == '\\')
            s->pop_back();
    }
    return a == b;
}

static bool has_package(const std::string &folder) { return path_is_file(path_join(folder, "tools/setup.py")); }

// the install folder has no package yet, or an older one than the launcher's own: tools/install_to.py first
static bool needs_copy() {
    if (same_path(U.install, U.home))
        return false;
    return !has_package(U.install) || version_newer(package_version(U.home), package_version(U.install));
}

static void use_install(const std::string &folder) {
    U.install = folder;
    U.root = has_package(folder) ? folder : U.home;
    U.version = package_version(U.root);
}

static SDL_EnumerationResult SDLCALL count_entry(void *n, const char *, const char *) {
    ++*(int *)n;
    return SDL_ENUM_SUCCESS;   // one is enough
}

static bool folder_empty(const std::string &folder) {
    int n = 0;
    SDL_EnumerateDirectory(folder.c_str(), count_entry, &n);
    return n == 0;
}

static void save() {
    U.set.set("lang", lang_code(g_lang));
    U.set.set("dump", U.dump);
    settings_save(U.set);
}

static void check_dump(const std::string &path) {
    if (U.check) {
        setup_free(U.check);
        U.check = nullptr;
    }
    std::string py = python_path(U.root);
    U.checking = path;
    U.dump_st = D_CHECKING;
    if (py.empty()) {
        U.dump_st = D_BAD, U.dump_code = "no_python", U.dump_args = Json();
        return;
    }
    U.check = setup_start({py, path_join(U.root, "tools/setup.py"), "--check-dump", path, "--json"}, U.root, "");
}

static void SDLCALL folder_picked(void *which, const char *const *files, int) {
    if (!files || !files[0])
        return;
    SDL_LockMutex(U.lock);
    U.picked = files[0];
    U.picked_for = (int)(intptr_t)which;
    SDL_UnlockMutex(U.lock);
}

static void pick_folder(int which, const std::string &start) {
    SDL_ShowOpenFolderDialog(folder_picked, (void *)(intptr_t)which, g_win, start.empty() ? nullptr : start.c_str(), false);
}

// ---------------------------------------------------------------- the build
static int setup_index(const std::string &id) {
    for (int i = 0; i < 8; i++)
        if (id == U.weight_id[i])
            return i;
    return -1;
}

static Row *row_of(const std::string &id) {   // tools and check share the first row; finish belongs to the last
    if (id == "tools" || id == "check")
        return &U.rows[0];
    if (id == "finish")
        return &U.rows[5];
    for (Row &r : U.rows)
        if (id == r.id)
            return &r;
    return nullptr;
}

static double est_total_secs() {   // a clean build: 34 min on 8 logical CPUs (2026-10-30); hyperthreads count half
    int n = logical_cpus();
    double eff = n <= 8 ? n : 8 + (n - 8) * 0.5;
    return 34 * 60 * 8 / SDL_max(eff, 1.0);
}

static void start_build_state() {
    for (Row &r : U.rows)
        r.st = R_PENDING, r.secs = -1, r.skipped = false, r.err.clear();
    static const double defaults[8] = {2, 1, 7, 4, 47, 9, 30, 0};   // setup.py's STEP_WEIGHTS, until its plan arrives
    for (int i = 0; i < 8; i++)
        U.weight[i] = defaults[i], U.frac[i] = 0;
    U.eta = -1, U.failed = U.done = U.cancel_q = U.hw_asked = U.stopping = false;
    U.fail_text.clear();
    U.hw = Json();
}

static void run_setup() {
    std::string py = python_path(U.root);
    if (py.empty()) {
        U.screen = SCR_BUILD;
        U.failed = true;
        U.fail_text = T("no_python");
        return;
    }
    const char *cpus = SDL_getenv("ULTRS_LAUNCHER_CPUS");   // development: test builds leave the PC half free
    std::vector<std::string> args = {py, path_join(U.root, "tools/setup.py"), "--json",
                                     "--cpus", cpus && *cpus ? cpus : std::to_string(logical_cpus()), "--dump", U.dump};
    U.build = setup_start(args, U.root, log_path());
    U.screen = SCR_BUILD;
}

static void start_build() {
    start_build_state();
    SDL_CreateDirectory(U.install.c_str());
    U.screen = SCR_BUILD;
    if (!needs_copy()) {
        run_setup();
        return;
    }
    U.rows[0].st = R_RUN;   // the copy counts as the first step's "tools" part
    U.copy = setup_start({python_path(U.home), path_join(U.home, "tools/install_to.py"), U.install}, U.home, log_path());
}

static void on_event(const Json &e) {
    std::string ev = e["ev"].str();
    if (ev == "plan") {
        const Json &steps = e["steps"];
        for (size_t i = 0; i < steps.size(); i++) {
            int k = setup_index(steps[i]["id"].str());
            if (k >= 0)
                U.weight[k] = steps[i]["weight"].num();
        }
    } else if (ev == "step") {
        std::string id = e["id"].str(), st = e["state"].str();
        int k = setup_index(id);
        Row *r = row_of(id);
        if (st == "run") {
            U.eta = -1;
            if (r && r->st == R_PENDING)
                r->st = R_RUN;
        } else {
            if (k >= 0)
                U.frac[k] = 1;
            if (r) {   // tools and finish only add their time: the row's own step (check, textures) says how it went
                r->secs = (r->secs > 0 ? r->secs : 0) + e["secs"].num();
                if (id != "tools" && id != "finish") {
                    r->st = r->st == R_WARN ? R_WARN : R_DONE;
                    r->skipped = st == "skip";
                }
            }
        }
    } else if (ev == "progress") {
        for (int k = 0; k < 8; k++) {
            Row *r = row_of(U.weight_id[k]);
            if (r && r->st == R_RUN && U.frac[k] < 1 && (k != 0 || U.rows[0].st != R_DONE)) {
                double total = e["total"].num();
                if (total > 0) {
                    // the running setup step: the first one not yet done whose row runs
                    U.frac[k] = SDL_min(0.999, e["done"].num() / total);
                    U.eta = e["eta"].kind == Json::Num ? e["eta"].num() : -1;
                }
                break;
            }
        }
    } else if (ev == "hw") {
        U.hw = e["cases"];
        for (size_t i = 0; i < U.hw.size(); i++)
            if (U.hw[i]["sev"].str() != "info")
                U.rows[0].st = R_WARN;
    } else if (ev == "ask" && e["id"].str() == "hw") {
        U.hw_asked = true;
        U.screen = SCR_HW;
    } else if (ev == "error") {
        U.failed = true;
        std::string code = e["code"].str();
        Row *r = row_of(e["id"].str());
        std::string t = has_text(code.c_str()) ? Tf(code.c_str(), e["args"]) : T("error_generic");
        if (r) {
            r->st = R_ERROR;
            r->err = t;
        }
        if (code == "hw_declined") {   // the player said no at the hardware question: back to the start
            U.failed = false;
            U.screen = SCR_DUMP;
        }
    } else if (ev == "done") {
        U.done = true;
        U.set.set("built", U.version);   // the game matches this package: Play, until the next update
        save();
        for (Row &r : U.rows)
            if (r.st == R_PENDING || r.st == R_RUN)
                r.st = R_DONE;
        for (double &f : U.frac)
            f = 1;
    }
}

static void poll_update() {
    UpdateInfo found;
    if (update_check_poll(&found) == 1 && found.version != U.version) {
        U.upd = found;
        U.has_upd = true;
        U.set.set("update_version", found.version), U.set.set("update_name", found.name);
        U.set.set("update_url", found.url), U.set.set("update_sha256", found.sha256);
        U.set.set("update_size", std::to_string(found.size)), U.set.set("update_page", found.page);
        save();
        if (SDL_getenv("ULTRS_LAUNCHER_DEMO") && SDL_strcmp(SDL_getenv("ULTRS_LAUNCHER_DEMO"), "update") == 0) {
            U.screen = SCR_UPDATE, U.upd_st = 1;   // development: the whole update without a click
            update_download_start(U.upd, path_join(U.root, "update"));
        }
    }
    if (U.upd_st == 1) {
        std::string res;
        int st = update_download_poll(nullptr, nullptr, &res);
        if (st == 3) {
            U.upd_st = 3, U.upd_err = res;
        } else if (st == 2) {   // checked: put it in place with the package's Python, the launcher restarts after
            U.upd_st = 2, U.apply_frac = 0;
            U.apply = setup_start({python_path(U.root), path_join(U.root, "tools/apply_update.py"), res, "--root", U.root},
                                  U.root, path_join(U.root, "update/update.log"));
        }
    }
    if (U.apply) {
        std::string line;
        bool ok = false;
        while (setup_line(U.apply, line)) {
            Json e;
            if (line.empty() || line[0] != '{' || !json_parse(line, e))
                continue;
            std::string ev = e["ev"].str();
            if (ev == "progress" && e["total"].num() > 0)
                U.apply_frac = e["done"].num() / e["total"].num();
            else if (ev == "error")
                U.upd_err = e["message"].str();
            else if (ev == "done")
                ok = true;
        }
        static bool applied = false;
        applied = applied || ok;
        int code;
        if (setup_finished(U.apply, &code)) {
            setup_free(U.apply);
            U.apply = nullptr;
            if (applied && code == 0 && restart_self()) {
                SDL_Event q = {};
                q.type = SDL_EVENT_QUIT;
                SDL_PushEvent(&q);
            } else {
                U.upd_st = 3;
                if (U.upd_err.empty())
                    U.upd_err = applied ? T("upd_restart") : "apply_update.py: exit code " + std::to_string(code);
            }
            applied = false;
        }
    }
}

static void poll() {
    poll_update();
    SDL_LockMutex(U.lock);
    std::string picked = U.picked;
    int which = U.picked_for;
    U.picked.clear();
    U.picked_for = 0;
    SDL_UnlockMutex(U.lock);
    if (which == 1)
        check_dump(picked);
    if (which == 2) {   // a folder with other things in it: the game goes into UltraTears inside it
        std::string f = picked;
        if (!same_path(f, U.home) && !has_package(f) && !folder_empty(f))
            f = path_join(f, "UltraTears");
        use_install(same_path(f, U.home) ? U.home : f);
        U.set.set("install", same_path(f, U.home) ? "" : f);
        save();
    }

    if (U.copy) {
        std::string line;
        while (setup_line(U.copy, line)) {
            Json e;
            if (line.empty() || line[0] != '{' || !json_parse(line, e))
                continue;
            std::string ev = e["ev"].str();
            if (ev == "progress" && e["total"].num() > 0)
                U.frac[0] = SDL_min(0.999, e["done"].num() / e["total"].num());
            else if (ev == "error")
                on_event(e);
        }
        int code;
        if (setup_finished(U.copy, &code)) {
            setup_free(U.copy);
            U.copy = nullptr;
            if (code == 0 && !U.failed && has_package(U.install)) {
                use_install(U.install);   // from now on the copy builds, updates and is played
                run_setup();
            } else if (!U.stopping) {
                U.failed = true;
                U.rows[0].st = R_ERROR;
                U.rows[0].err = Tf("install_copy", {{"path", U.install}});
            }
        }
    }

    if (U.check) {
        std::string line;
        while (setup_line(U.check, line)) {
            Json e;
            if (json_parse(line, e) && e["ev"].str() == "dump") {
                if (e["ok"].truth()) {
                    U.dump_st = D_OK;
                    U.dump = e["path"].str();
                    save();
                } else {
                    U.dump_st = D_BAD;
                    U.dump_code = e["code"].str();
                    U.dump_args = e["args"];
                    U.checking = e["path"].str(U.checking.c_str());
                }
            }
        }
        int code;
        if (setup_finished(U.check, &code)) {
            if (U.dump_st == D_CHECKING) {   // no answer at all: Python missing or broken
                U.dump_st = D_BAD;
                U.dump_code = "no_python";
                U.dump_args = Json();
            }
            setup_free(U.check);
            U.check = nullptr;
        }
    }
    if (U.build) {
        std::string line;
        while (setup_line(U.build, line)) {
            Json e;
            if (!line.empty() && line[0] == '{' && json_parse(line, e))
                on_event(e);
        }
        int code;
        if (setup_finished(U.build, &code)) {
            if (!U.done && !U.failed && U.screen != SCR_DUMP) {
                U.failed = true;
                U.fail_text = Tf("setup_crash", {{"code", std::to_string(code)}});
                for (Row &r : U.rows)
                    if (r.st == R_RUN) {
                        r.st = R_ERROR;
                        r.err = T("error_generic");
                    }
            }
            setup_free(U.build);
            U.build = nullptr;
        }
    }
}

static double overall() {
    double sum = 0, done = 0;
    for (int k = 0; k < 8; k++)
        sum += U.weight[k], done += U.weight[k] * U.frac[k];
    return sum > 0 ? done / sum : 0;
}

// ---------------------------------------------------------------- screens
static void update_banner(float right);

static void header(bool lang) {
    icon(g_media.emblem, 36, 20);
    ImFont *b = g_media.brand;
    text(b, 86, 40 - b->FontSize / S / 2, C_TEXT, "UltraTears");
    if (!lang)
        return;
    const char *name = lang_name(g_lang);
    float w = 26 + 14 + 6 + text_w(g_media.medium, name) + 4 + 14;
    float x = 764 - w;
    update_banner(x);
    bool hov, down;
    if (hit("lang", x, 24, w, 32, &hov, &down))
        ImGui::OpenPopup("langs");
    bool open = ImGui::IsPopupOpen("langs");
    DL->AddRectFilled(P(x, 24), P(764, 56), down ? C_DARK_DOWN : hov || open ? C_DARK_HOVER : C_DARK, 6 * S);
    globe(x + 13, 33, C_TEXT);
    text(g_media.medium, x + 13 + 14 + 6, 40 - g_media.medium->FontSize / S / 2, C_TEXT, name);
    icon(g_media.chevron, 764 - 13 - 14, 33, 14);
    ImGui::SetNextWindowPos(P(x, 60));
    ImGui::SetNextWindowSize(ImVec2((764 - x) * S, (8 + 30 * LANG_COUNT) * S));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    if (ImGui::BeginPopup("langs", ImGuiWindowFlags_NoScrollbar)) {
        ImDrawList *keep = DL;
        ImVec2 keep_o = O;
        DL = ImGui::GetWindowDrawList();
        O = ImVec2(0, 0);
        for (int l = 0; l < LANG_COUNT; l++) {
            float ly = 64 + 30 * l;
            bool lh, ld;
            if (hit(lang_code((Lang)l), x + 4, ly, 764 - x - 8, 30, &lh, &ld)) {
                g_lang = (Lang)l;
                save();
                ImGui::CloseCurrentPopup();
            }
            if (lh || l == g_lang)
                DL->AddRectFilled(P(x + 4, ly), P(760, ly + 30), lh ? C_DARK_HOVER : C_DARK_DOWN, 4 * S);
            text(g_media.medium, x + 13, ly + 15 - g_media.medium->FontSize / S / 2, C_TEXT, lang_name((Lang)l));
        }
        DL = keep, O = keep_o;
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();
}

static float title(float y, const char *t, const char *sub) {
    text(g_media.title, 36, y, C_TEXT, t);
    float h = g_media.title->FontSize / S + 10;
    if (sub)
        h += rich(g_media.regular, 36, y + h, 680, {{sub, C_SUB}});
    return y + h;
}

static std::string gb(double v) { return std::to_string((long)(v + 0.5)); }

static void screen_dump() {
    float y = title(88, T("dump_title"), T("dump_sub")) + 24;
    bool hov, down;
    if (U.dump_st == D_NONE || U.dump_st == D_CHECKING) {
        bool busy = U.dump_st == D_CHECKING;
        if (hit("pick", 36, y, 728, 88, &hov, &down, !busy))
            pick_folder(1, U.dump);
        DL->AddRectFilled(P(36, y), P(764, y + 88), down ? C_CARD : hov ? C_CARD_HOVER : C_CARD, 10 * S);
        const char *label = busy ? T("checking") : T("choose_folder");
        if (!busy)
            icon(g_media.folder, 400 - 12, y + 18);
        float tw = text_w(g_media.medium, label);
        text(g_media.medium, 400 - tw / 2, busy ? y + 35 : y + 50, busy ? C_SUB : C_TEXT, label);
        float hw = text_w(g_media.small, T("drop_hint"));
        text(g_media.small, 400 - hw / 2, y + 102, C_DIM, T("drop_hint"));
        return;
    }
    if (U.dump_st == D_BAD) {
        std::string head = has_text(U.dump_code.c_str()) ? T(U.dump_code.c_str()) : T("error_generic");
        std::string detail = U.dump_code + ".d";
        float bw = text_w(g_media.medium, T("choose_other")) + 32;
        float top = y;
        y += 18;
        path_text(g_media.regular, 52, y + 10, 728 - bw - 48, U.checking);
        if (button("other", T("choose_other"), 748 - bw, y, bw, 40, LIGHT))
            pick_folder(1, U.checking);
        y += 56;
        icon(g_media.error, 52, y + 2);
        y += rich(g_media.regular, 76, y, 660, {{head.c_str(), C_TEXT}}) + 2;
        if (has_text(detail.c_str())) {
            std::string d = Tf(detail.c_str(), U.dump_args);
            y += rich(g_media.regular, 76, y, 660, {{d.c_str(), C_SUB}});
        }
        DL->ChannelsSetCurrent(0);
        DL->AddRectFilled(P(36, top), P(764, y + 14), C_CARD, 10 * S);
        DL->ChannelsSetCurrent(1);
        return;
    }
    // D_OK: the dump and the install folder
    float top = y;
    text(g_media.small, 52, y + 14, C_SUB, T("dump_label"));
    float cw = text_w(g_media.medium, T("change")) + 32, pw = 0;
    path_text(g_media.regular, 52, y + 38, 748 - cw - 52 - 40, U.dump, &pw);
    icon(g_media.check, 52 + pw + 8, y + 39);
    if (button("dump_change", T("change"), 748 - cw, y + 14, cw, 40, DARK))
        pick_folder(1, U.dump);
    DL->ChannelsSetCurrent(0);
    DL->AddRectFilled(P(36, top), P(764, top + 68), C_CARD, 10 * S);
    DL->ChannelsSetCurrent(1);

    y = top + 76;
    top = y;
    text(g_media.small, 52, y + 14, C_TEXT, T("install_label"));
    path_text(g_media.regular, 52, y + 38, 748 - cw - 52 - 16, U.install);
    if (button("inst_change", T("change"), 748 - cw, y + 12, cw, 40, DARK))
        pick_folder(2, U.install);
    double need = needs_copy() ? 41 : 40, free = disk_free_gb(U.install);
    double est = est_total_secs() / 60;
    long lo = (long)(est * 0.85 / 5) * 5, hi = ((long)(est * 1.15) + 4) / 5 * 5;
    std::string need_s = Tf("need_free", {{"need", gb(need)}, {"free", free < 0 ? "?" : gb(free)}});
    std::string wait_s = Tf("wait_time", {{"lo", std::to_string(SDL_max(lo, 5L))}, {"hi", std::to_string(SDL_max(hi, 10L))}});
    bool space = free < 0 || free >= need || installed() || started_before();
    y += 64;
    if (space)
        y += rich(g_media.small, 52, y, 700, {{need_s.c_str(), C_SUB}, {(" " + wait_s).c_str(), C_TEXT}});
    else {
        std::string ns = Tf("no_space", {{"drive", drive_of(U.install)}});
        y += rich(g_media.small, 52, y, 700, {{need_s.c_str(), C_ERR}, {(" " + ns).c_str(), C_ERR}});
    }
    std::string del = Tf("can_delete", {{"dump", U.dump}});
    y += rich(g_media.small, 52, y, 700, {{del.c_str(), C_SUB}});
    if (!same_path(U.install, U.home)) {
        std::string cp = Tf("copy_note", {{"home", U.home}});
        y += rich(g_media.small, 52, y, 700, {{cp.c_str(), C_SUB}});
    }
    DL->ChannelsSetCurrent(0);
    DL->AddRectFilled(P(36, top), P(764, y + 14), C_CARD, 10 * S);
    DL->ChannelsSetCurrent(1);
}

static void screen_dump_footer() {
    if (U.dump_st != D_OK)
        return;
    bool resume = started_before(), rebuild = stale();
    const char *label = rebuild ? T("rebuild") : resume ? T("resume") : T("prepare");
    float w = SDL_max(196.0f, text_w(g_media.medium, label) + 48);
    double free = disk_free_gb(U.install);
    bool space = free < 0 || free >= (needs_copy() ? 41 : 40) || installed() || resume;
    if (resume || rebuild)
        rich(g_media.small, 36, 418, 764 - w - 36 - 24, {{T(rebuild ? "rebuild_hint" : "resume_hint"), C_SUB}}, 17);
    if (button("prepare", label, 764 - w, 408, w, 48, PRIMARY, space))
        start_build();
}

static void step_rows(float y) {
    double est = est_total_secs();
    for (int i = 0; i < 6; i++) {
        Row &r = U.rows[i];
        float ry = y + 26 * i, cx = 44, cy = ry + 8;
        ImU32 name_col = C_TEXT;
        switch (r.st) {
        case R_DONE: icon(g_media.check, 36, ry); break;
        case R_WARN: icon(g_media.warn, 36, ry); break;
        case R_RUN: ring(cx, cy, 6.5f, C_OK); break;
        case R_ERROR: ring(cx, cy, 6.5f, C_ERR); name_col = C_ERR; break;
        default: dot(cx, cy, 7, C_PENDING); name_col = C_DIM;
        }
        const char *name = T(r.name);
        text(g_media.regular, 58, ry - 1, name_col, name);
        float nx = 58 + text_w(g_media.regular, name) + 8;
        std::string note;
        ImU32 note_col = C_DIM;
        ImFont *nf = g_media.small;
        if (r.st == R_ERROR) {
            note = r.err, note_col = C_ERR, nf = g_media.regular;
        } else if (r.st == R_WARN && i == 0) {
            note = T("hw_note");
        } else if (r.st == R_DONE && r.skipped) {
            note = T("was_done");
        } else if (r.st == R_DONE && r.secs >= 1) {
            note = Tf("took", {{"t", fmt_duration(r.secs)}});
        } else if (r.st == R_RUN && U.eta > 0) {
            note = Tf("left", {{"t", fmt_minutes(U.eta)}});
        } else if (r.st == R_PENDING && (i == 3 || i == 5)) {   // the long ones, as the design shows
            note = Tf("may_take", {{"t", fmt_minutes(est * U.weight[i + 1] / 100)}});
        }
        if (!note.empty())
            text(nf, nx, ry + (nf == g_media.small ? 1 : -1), note_col, note.c_str());
        if (i == 0 && U.hw.size() && hovered_rect(P(36, ry - 2), P(760, ry + 20))) {   // the cases, on hover
            ImGui::BeginTooltip();
            for (size_t k = 0; k < U.hw.size(); k++) {
                std::string code = U.hw[k]["code"].str();
                ImGui::TextUnformatted(Tf(code.c_str(), U.hw[k]["args"]).c_str());
                std::string d = code + ".d";
                if (has_text(d.c_str())) {
                    ImGui::PushStyleColor(ImGuiCol_Text, C_SUB);
                    ImGui::TextUnformatted(Tf(d.c_str(), U.hw[k]["args"]).c_str());
                    ImGui::PopStyleColor();
                }
            }
            ImGui::EndTooltip();
        }
    }
}

static void screen_build() {
    float y;
    if (U.done)
        y = title(88, T("done_title"), nullptr) + 28;
    else
        y = title(88, T("prep_title"), T("prep_sub")) + 18;
    double f = U.done ? 1 : overall();
    char pct[16];
    SDL_snprintf(pct, sizeof pct, "%d%%", (int)(f * 100));
    if (!U.done) {
        const char *label = U.failed ? T("failed_title") : U.copy ? T("copying") : T("processing");
        text(g_media.regular, 36, y, C_TEXT, label);
    }
    text(g_media.regular, 764 - text_w(g_media.regular, pct), y, C_TEXT, pct);
    y += 26;
    DL->AddRectFilled(P(36, y), P(764, y + 8), C_BAR, 4 * S);
    if (f > 0)
        DL->AddRectFilled(P(36, y), P(36 + 728 * (float)f, y + 8), U.failed ? C_BAR_FAIL : C_BAR_FILL, 4 * S);
    y += 34;
    step_rows(y);
    if (U.failed && !U.fail_text.empty())
        rich(g_media.small, 36, y + 26 * 6 + 4, 728, {{U.fail_text.c_str(), C_ERR}});
}

static void screen_build_footer() {
    if (U.done) {
        if (button("done", T("done_btn"), 649, 408, 115, 48, PRIMARY))
            U.screen = SCR_PLAY;
        return;
    }
    if (U.failed) {
        std::string issues = std::string(ULTRS_REPO) + "/issues/new";
        rich(g_media.small, 36, 416, 440, {{T("report1"), C_SUB}, {T("report_link"), C_TEXT, issues.c_str()}, {T("report2"), C_SUB}}, 17);
        float w = text_w(g_media.medium, T("open_log")) + 64;
        if (button("open_log", T("open_log"), 764 - w, 408, w, 48, DARK))
            reveal_in_folder(log_path());
        return;
    }
    if (U.cancel_q) {
        rich(g_media.small, 36, 412, 420, {{T("cancel_q"), C_TEXT}}, 17);
        float w1 = text_w(g_media.medium, T("stop")) + 40, w2 = text_w(g_media.medium, T("go_on")) + 40;
        if (button("stop", T("stop"), 764 - w2 - 12 - w1, 408, w1, 48, DARK)) {
            if (U.build)
                setup_kill(U.build);
            if (U.copy)
                setup_kill(U.copy);
            U.stopping = true;
            U.cancel_q = false;
            U.screen = SCR_DUMP;
            if (U.close_after_cancel) {
                SDL_Event q = {};
                q.type = SDL_EVENT_QUIT;
                SDL_PushEvent(&q);
            }
        }
        if (button("go_on", T("go_on"), 764 - w2, 408, w2, 48, PRIMARY))
            U.cancel_q = U.close_after_cancel = false;
        return;
    }
    float w = text_w(g_media.medium, T("cancel")) + 40;
    if (button("cancel", T("cancel"), 36, 416, w, 40, DARK))
        U.cancel_q = true;
    float hw = text_w(g_media.small, T("bg_hint"));
    text(g_media.small, 764 - hw, 430, C_SUB, T("bg_hint"));
}

static void screen_hw() {
    float y = title(88, T("hw_title"), nullptr) + 6;
    std::string req = std::string(ULTRS_REPO) + (g_lang == LANG_RU ? "/blob/HEAD/README.ru.md" : "#requirements-for-either-method");
    y += rich(g_media.regular, 36, y, 700, {{T("hw_text1"), C_SUB}, {T("hw_link"), C_TEXT, req.c_str()}, {T("hw_text2"), C_SUB}});
    y += 22;
    bool hov, down;
    float w1 = text_w(g_media.medium, T("hw_go")) + 32, w2 = text_w(g_media.medium, T("cancel")) + 32;
    if (button("hw_go", T("hw_go"), 36, y, w1, 40, PRIMARY)) {
        setup_answer(U.build, "yes");
        U.screen = SCR_BUILD;
    }
    if (button("hw_no", T("cancel"), 36 + w1 + 8, y, w2, 40, DARK)) {
        setup_answer(U.build, "no");
        U.screen = SCR_DUMP;
    }
    (void)hov, (void)down;
    y += 64;
    for (size_t k = 0; k < U.hw.size(); k++) {   // what exactly: the graphics card first (stop), then the rest
        const Json &c = U.hw[k];
        std::string code = c["code"].str(), sev = c["sev"].str();
        icon(sev == "stop" ? g_media.error : g_media.warn, 36, y + 2);
        std::string head = Tf(code.c_str(), c["args"]);
        y += rich(g_media.medium, 60, y, 700, {{head.c_str(), C_TEXT}});
        std::string d = code + (code == "no_shader_object" && c["args"]["layer"].truth() ? ".layer" : ".d");
        if (has_text(d.c_str())) {
            std::string dt = Tf(d.c_str(), c["args"]);
            y += rich(g_media.regular, 60, y, 700, {{dt.c_str(), C_SUB}});
        }
        y += 10;
    }
    ImGui::SetCursorScreenPos(P(0, y));
    ImGui::Dummy(ImVec2(1, 1));
}

static void screen_broken() {   // the exe without the rest of the zip: nothing to choose, only what to do
    float y = title(88, T("broken_title"), nullptr) + 4;
    rich(g_media.regular, 36, y, 700, {{T("broken_text"), C_SUB}});
}

static void screen_play() {
    float y = title(88, T("play_title"), nullptr) + 4;
    std::string sub = Tf("play_sub", {{"path", U.install}});
    rich(g_media.regular, 36, y, 700, {{sub.c_str(), C_SUB}});
}

static void screen_update() {
    float y = title(88, Tf("upd_title", {{"v", U.upd.version}}).c_str(), nullptr) + 6;
    std::string from = Tf("upd_from", {{"old", U.version}, {"v", U.upd.version}});
    y += rich(g_media.regular, 36, y, 700, {{from.c_str(), C_SUB}, {" ", C_SUB}, {T("upd_notes"), C_TEXT, U.upd.page.c_str()}});
    y += 22;
    if (U.upd_st == 1 || U.upd_st == 2) {
        uint64_t got = 0, total = 0;
        update_download_poll(&got, &total, nullptr);
        double f = U.upd_st == 1 ? (total ? (double)got / total : 0) : U.apply_frac;
        char amount[64];
        if (U.upd_st == 1)
            SDL_snprintf(amount, sizeof amount, "%.0f / %.0f MB", got / 1048576.0, total / 1048576.0);
        else
            SDL_snprintf(amount, sizeof amount, "%d%%", (int)(f * 100));
        text(g_media.regular, 36, y, C_TEXT, T(U.upd_st == 1 ? "upd_download" : "upd_apply"));
        text(g_media.regular, 764 - text_w(g_media.regular, amount), y, C_TEXT, amount);
        y += 26;
        DL->AddRectFilled(P(36, y), P(764, y + 8), C_BAR, 4 * S);
        if (f > 0)
            DL->AddRectFilled(P(36, y), P(36 + 728 * (float)f, y + 8), C_BAR_FILL, 4 * S);
        y += 30;
        rich(g_media.small, 36, y, 728, {{T("upd_keep"), C_SUB}});
    } else if (U.upd_st == 3) {
        icon(g_media.error, 36, y + 2);
        text(g_media.regular, 60, y, C_TEXT, T("upd_failed"));
        y += 24;
        rich(g_media.regular, 60, y, 700, {{U.upd_err.c_str(), C_SUB}});
    } else {
        rich(g_media.regular, 36, y, 700, {{T("upd_what"), C_SUB}});
    }
}

static void screen_update_footer() {
    if (U.upd_st == 2)
        return;   // files are being replaced: no way back halfway (a second try would finish it)
    if (U.upd_st == 1) {
        float w = text_w(g_media.medium, T("cancel")) + 40;
        if (button("upd_cancel", T("cancel"), 36, 416, w, 40, DARK)) {
            update_download_cancel();
            U.upd_st = 0;
            U.screen = installed() && !stale() ? SCR_PLAY : SCR_DUMP;
        }
        return;
    }
    float w1 = SDL_max(160.0f, text_w(g_media.medium, T("upd_go")) + 48), w2 = text_w(g_media.medium, T("upd_later")) + 40;
    if (button("upd_go", T(U.upd_st == 3 ? "upd_retry" : "upd_go"), 764 - w1, 408, w1, 48, PRIMARY)) {
        U.upd_err.clear();
        U.upd_st = 1;
        update_download_start(U.upd, path_join(U.root, "update"));
    }
    if (button("upd_later", T("upd_later"), 764 - w1 - 12 - w2, 408, w2, 48, DARK)) {
        U.upd_st = 0;
        U.screen = installed() && !stale() ? SCR_PLAY : SCR_DUMP;
    }
}

// "Version X is out — Update" next to the language button, while nothing is being built
static void update_banner(float right) {
    if (!U.has_upd || (U.screen != SCR_DUMP && U.screen != SCR_PLAY))
        return;
    std::string t = Tf("upd_banner", {{"v", U.upd.version}});
    const char *go = T("upd_open");
    float bw = text_w(g_media.medium, go) + 28, x = right - 12 - bw;
    text(g_media.small, x - 10 - text_w(g_media.small, t.c_str()), 40 - g_media.small->FontSize / S / 2, C_SUB, t.c_str());
    if (button("upd_banner", go, x, 24, bw, 32, PRIMARY))
        U.screen = SCR_UPDATE;
}

static bool g_play_failed = false;
static void screen_play_footer() {
    if (g_play_failed)
        text(g_media.small, 36, 425, C_ERR, T("play_failed"));
    if (button("play", T("play"), 604, 408, 160, 48, PRIMARY))
        g_play_failed = !start_game(U.install);
}

// ---------------------------------------------------------------- frame
static void background(ImDrawList *bg, ImVec2 size) {
    bg->AddRectFilled(ImVec2(0, 0), size, C_BG);
    const Icon &p = g_media.pattern;
    if (!p.tex)
        return;
    float t = p.w * S;
    for (float y = 0; y < size.y; y += t)
        for (float x = 0; x < size.x; x += t)
            bg->AddImage((ImTextureID)(intptr_t)p.tex, ImVec2(x, y), ImVec2(x + t, y + t));
}

void ui_init(SDL_Window *w) {
    g_win = w;
    U.lock = SDL_CreateMutex();
    U.set = settings_load();
    std::string lang = U.set.get("lang");
    g_lang = lang.empty() ? system_lang() : lang_from_code(lang.c_str());
    U.dump = U.set.get("dump");
    // The builder package the launcher lies in: the game is built there (setup.py in place). ULTRS_ROOT: development,
    // a package or a folder made by setup.py --output elsewhere. A source checkout has no launcher: setup.py there.
    U.home = exe_dir();
    const char *root = SDL_getenv("ULTRS_ROOT");
    if (root && *root)
        U.home = root;
#ifdef _WIN32
    for (char &c : U.home)   // shown as Windows writes paths
        if (c == '/')
            c = '\\';
#endif
    std::string inst = U.set.get("install");
    use_install(inst.empty() ? U.home : inst);
    update_cleanup(U.root);
    // updates: one question to GitHub a day at most; a release found stays offered until it is installed
    U.upd.version = U.set.get("update_version");
    if (!U.upd.version.empty() && U.upd.version != U.version && !U.version.empty()) {
        U.upd.name = U.set.get("update_name"), U.upd.url = U.set.get("update_url");
        U.upd.sha256 = U.set.get("update_sha256"), U.upd.page = U.set.get("update_page");
        U.upd.size = SDL_strtoull(U.set.get("update_size").c_str(), nullptr, 10);
        U.has_upd = !U.upd.url.empty() && !U.upd.sha256.empty();
    }
    long long checked = SDL_strtoll(U.set.get("update_checked").c_str(), nullptr, 10);
    SDL_Time now = 0;
    SDL_GetCurrentTime(&now);
    long long secs = (long long)(now / SDL_NS_PER_SECOND);
    if (U.set.get("updates") != "off" && !U.version.empty() && (secs - checked > 24 * 3600 || secs < checked)) {
        U.set.set("update_checked", std::to_string(secs));
        settings_save(U.set);
        update_check_start(U.version);
    }
    ImGuiStyle &st = ImGui::GetStyle();
    st.WindowPadding = ImVec2(0, 0);
    st.WindowBorderSize = 0;
    st.ScrollbarSize = 6;
    st.ScrollbarRounding = 3;
    st.Colors[ImGuiCol_WindowBg] = ImVec4(0, 0, 0, 0);
    st.Colors[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
    st.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
    st.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(1, 1, 1, 0.18f);
    st.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(1, 1, 1, 0.3f);
    st.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(1, 1, 1, 0.4f);
    st.Colors[ImGuiCol_PopupBg] = ImVec4(0.14f, 0.14f, 0.14f, 1);
    st.Colors[ImGuiCol_NavCursor] = ImVec4(0, 0, 0, 0);   // the focus frame is drawn by hit()
    st.PopupRounding = 6;
    st.WindowRounding = 6;
    if (python_path(U.root).empty())
        U.screen = SCR_BROKEN;
    else if (installed() && !U.dump.empty() && !stale())
        U.screen = SCR_PLAY;
    else if (!U.dump.empty())
        check_dump(U.dump);
}

void ui_frame(float scale) {
    S = scale;
    poll();
    ImGuiIO &io = ImGui::GetIO();
    background(ImGui::GetBackgroundDrawList(), io.DisplaySize);
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("launcher", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                 ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    DL = ImGui::GetWindowDrawList();
    O = ImVec2(0, 0);
    bool lang = U.screen == SCR_DUMP || U.screen == SCR_PLAY || U.screen == SCR_BROKEN;

    // the body scrolls when it does not fit (long texts, a small screen); the header and the buttons stay
    ImGui::SetCursorScreenPos(P(0, 72));
    ImGui::BeginChild("body", ImVec2(800 * S, 328 * S), ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground);
    DL = ImGui::GetWindowDrawList();
    DL->ChannelsSplit(2);   // 0: the cards, drawn after what is on them once their height is known
    DL->ChannelsSetCurrent(1);
    O = ImVec2(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y - ImGui::GetScrollY() - 72 * S);
    switch (U.screen) {
    case SCR_DUMP: screen_dump(); break;
    case SCR_BUILD: screen_build(); break;
    case SCR_HW: screen_hw(); break;
    case SCR_PLAY: screen_play(); break;
    case SCR_UPDATE: screen_update(); break;
    case SCR_BROKEN: screen_broken(); break;
    }
    DL->ChannelsMerge();
    ImGui::EndChild();

    DL = ImGui::GetWindowDrawList();
    O = ImVec2(0, 0);
    switch (U.screen) {
    case SCR_DUMP: screen_dump_footer(); break;
    case SCR_BUILD: screen_build_footer(); break;
    case SCR_PLAY: screen_play_footer(); break;
    case SCR_UPDATE: screen_update_footer(); break;
    default: break;
    }
    header(lang);   // last: the language list lies over the body
    ImGui::End();
}

// Development: a screen with sample data, for screenshots (ULTRS_LAUNCHER_DEMO=<name>, main.cpp): dump, dump_bad,
// dump_ok, build, hw, failed, done, play.
void ui_demo(const char *name) {
    std::string n = name;
    auto events = [](std::initializer_list<const char *> lines) {
        for (const char *l : lines) {
            Json e;
            if (json_parse(l, e))
                on_event(e);
        }
    };
    if (U.check) {
        setup_free(U.check);
        U.check = nullptr;
    }
    U.screen = SCR_DUMP;
    U.dump_st = D_NONE;
    if (U.dump.empty() && n != "update" && n != "run")   // sample screens only: never saved over a real choice
        U.dump = "D:\\Games\\Original";
    if (n == "update")
        return;   // poll_update starts it once GitHub answers
    if (n == "update_ask") {
        U.upd.version = "0.3.0", U.upd.page = ULTRS_REPO "/releases";
        U.version = U.version.empty() ? "0.2.0" : U.version;
        U.has_upd = true, U.upd_st = 0, U.screen = SCR_UPDATE;
        return;
    }
    if (n == "dump_bad") {
        U.dump_st = D_BAD, U.dump_code = "dump_version", U.checking = "D:\\Games\\Original";
    } else if (n == "dump_ok") {
        U.dump_st = D_OK;
    } else if (n == "build" || n == "hw" || n == "failed" || n == "done") {
        start_build_state();
        U.screen = SCR_BUILD;
        events({R"({"ev":"step","id":"tools","state":"done","secs":3})",
                R"({"ev":"hw","cases":[{"code":"no_shader_object","sev":"stop","args":{"gpu":"AMD Radeon RX 570","layer":false}},{"code":"ram_low","sev":"warn","args":{"gb":8}}]})",
                R"({"ev":"step","id":"check","state":"done","secs":2})",
                R"({"ev":"step","id":"romfs","state":"done","secs":220})",
                R"({"ev":"step","id":"lift","state":"run"})",
                R"({"ev":"progress","label":"lifting","done":40,"total":100,"eta":250})"});
        if (n == "hw")
            U.screen = SCR_HW;
        if (n == "failed")
            events({R"({"ev":"error","id":"lift","code":"failed","args":{},"message":"x"})"});
        if (n == "done")
            events({R"({"ev":"step","id":"lift","state":"done","secs":80})", R"({"ev":"step","id":"compile","state":"done","secs":940})",
                    R"({"ev":"step","id":"shaders","state":"skip","secs":1})", R"({"ev":"step","id":"textures","state":"done","secs":600})",
                    R"({"ev":"done","secs":1900})"});
    } else if (n == "run") {   // a real build at once, as if Prepare was pressed
        U.dump_st = D_OK;
        start_build();
    } else if (n == "play") {
        U.screen = SCR_PLAY;
    }
}

void ui_drop(const char *path) {
    if (U.screen == SCR_DUMP || U.screen == SCR_PLAY) {
        U.screen = SCR_DUMP;
        check_dump(path);
    }
}

bool ui_close_request() {
    if (U.copy && !U.stopping) {   // the copy resumes too: the same question as for the build
        U.screen = SCR_BUILD;
        U.cancel_q = U.close_after_cancel = true;
        return false;
    }
    if (U.apply)
        return false;   // files of the package are being replaced: a few seconds
    if (!U.build || U.done || U.failed || U.stopping)
        return true;
    U.screen = SCR_BUILD;
    U.cancel_q = true;
    U.close_after_cancel = true;
    return false;
}

void ui_quit() {
    if (U.check)
        setup_free(U.check);
    if (U.build)
        setup_free(U.build);   // a build stopped here resumes next time
    if (U.copy)
        setup_free(U.copy);
}
