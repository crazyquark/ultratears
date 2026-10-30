// The launcher's words in English and Russian. A new language: one more column, LANG_COUNT and lang_name / lang_code.
// The codes of setup.py --json (dump_*, hw cases of tools/hw_check.py, step errors) have their texts here: "<code>"
// is the headline, "<code>.d" the detail; {name} is filled from the event's args.
#include "launcher.h"
#include <cstring>

Lang g_lang = LANG_EN;

struct Text { const char *key, *s[LANG_COUNT]; };

static const Text TEXTS[] = {
    {"dump_title", {"Path to the original game", "Путь к оригинальной игре"}},
    {"dump_sub", {"UltraTears is built from your own copy of the game. Choose its folder: the launcher checks the version "
                  "and compatibility itself.",
                  "UltraTears собирается из вашей собственной копии игры. Выберите папку с ней — лаунчер сам проверит "
                  "версию и совместимость."}},
    {"choose_folder", {"Choose folder", "Выбрать папку"}},
    {"drop_hint", {"You can also drop the folder onto this window", "Можно перетащить в это окно"}},
    {"checking", {"Checking the folder…", "Проверяю папку…"}},
    {"choose_other", {"Choose another folder", "Выбрать другую папку"}},
    {"dump_label", {"Folder with the original game", "Папка с исходной игрой"}},
    {"install_label", {"Where the game will be installed", "Финальное место распаковки игры"}},
    {"change", {"Change", "Изменить"}},
    {"need_free", {"Needs ≈ {need} GB. Free: {free} GB.", "Нужно ≈ {need} ГБ. Свободно {free} ГБ."}},
    {"wait_time", {"≈ {lo}–{hi} minutes of waiting", "≈ время ожидания {lo}–{hi} минут"}},
    {"can_delete", {"Once the game is ready, the original files in {dump} can be deleted.",
                    "По окончании установки оригинальные файлы из {dump} можно удалить."}},
    {"no_space", {"Not enough space on {drive} — choose another folder or free some space.",
                  "Не хватает места на диске {drive} — выберите другую папку или освободите место."}},
    {"prepare", {"Prepare the game", "Подготовить игру"}},
    {"resume", {"Continue preparing", "Продолжить подготовку"}},
    {"resume_hint", {"The last preparation stopped midway: it continues where it left off.",
                     "Прошлая подготовка остановилась на середине — продолжится с того же места."}},
    {"rebuild", {"Update the game", "Обновить игру"}},
    {"rebuild_hint", {"UltraTears was updated: only what changed is rebuilt, usually a few minutes.",
                      "UltraTears обновилась — пересоберётся только изменённое, обычно несколько минут."}},
    {"upd_banner", {"Version {v} is out", "Доступна версия {v}"}},
    {"upd_open", {"Update", "Обновить"}},
    {"upd_title", {"Update to {v}", "Обновление до {v}"}},
    {"upd_from", {"Installed: {old}. New: {v}.", "Установлена {old}, новая — {v}."}},
    {"upd_notes", {"What's new", "Что нового"}},
    {"upd_what", {"The launcher downloads the new version from GitHub, checks it and puts it in place of the old one. "
                  "Your game data and saves stay; then only what changed is rebuilt.",
                  "Лаунчер скачает новую версию с GitHub, проверит её и поставит вместо старой. Данные игры и сохранения "
                  "останутся, затем пересоберётся только изменённое."}},
    {"upd_go", {"Update", "Обновить"}},
    {"upd_retry", {"Try again", "Повторить"}},
    {"upd_later", {"Not now", "Не сейчас"}},
    {"upd_download", {"Downloading", "Загрузка"}},
    {"upd_apply", {"Installing the update", "Установка обновления"}},
    {"upd_keep", {"The launcher restarts by itself when it is done.", "Когда всё будет готово, лаунчер перезапустится сам."}},
    {"upd_failed", {"The update failed", "Обновить не удалось"}},
    {"upd_restart", {"The update is in place: start the launcher again.", "Обновление установлено — запустите лаунчер заново."}},
    {"prep_title", {"Preparing the game", "Подготовка игры"}},
    {"prep_sub", {"The launcher does all the technical work itself", "Всю техническую работу лаунчер сделает сам"}},
    {"processing", {"Processing files", "Обработка файлов"}},
    {"step_check", {"Hardware check", "Проверка оборудования"}},
    {"step_romfs", {"Copying the game data", "Копирование данных игры"}},
    {"step_lift", {"Translating the game code", "Перевод кода игры"}},
    {"step_compile", {"Building the game", "Сборка игры"}},
    {"step_shaders", {"Preparing the graphics", "Подготовка графики"}},
    {"step_textures", {"Compressing textures", "Сжатие текстур"}},
    {"left", {"about {t} left", "осталось ≈ {t}"}},
    {"may_take", {"may take about {t}", "может потребоваться около {t}"}},
    {"took", {"took {t}", "заняло {t}"}},
    {"was_done", {"already done", "уже готово"}},
    {"cancel", {"Cancel", "Отмена"}},
    {"bg_hint", {"You can minimize the window: preparing goes on in the background.",
                 "Можно свернуть окно — подготовка продолжится в фоне."}},
    {"cancel_q", {"Stop preparing? Next time it continues where it stopped.",
                  "Остановить подготовку? В следующий раз она продолжится с того же места."}},
    {"stop", {"Stop", "Остановить"}},
    {"go_on", {"Go on", "Продолжить"}},
    {"failed_title", {"Preparing the game failed", "Подготовка игры завершилась неудачей"}},
    {"report1", {"If you think this is a bug, please open an ", "Если считаете это ошибкой, пожалуйста, создайте "}},
    {"report_link", {"issue on GitHub", "issue на GitHub"}},
    {"report2", {" and attach ultratears-setup.log.", " и приложите ultratears-setup.log."}},
    {"open_log", {"Open the log folder", "Открыть папку с логом"}},
    {"done_title", {"The game is ready", "Подготовка игры завершена"}},
    {"done_btn", {"Done", "Готово"}},
    {"hw_note", {"below the recommended hardware", "оборудование не соответствует рекомендуемому"}},
    {"hw_title", {"Hardware check", "Проверка оборудования"}},
    {"hw_text1", {"Your graphics card does not meet the minimum requirements. The game may not work. See the ",
                  "Ваша видеокарта не соответствует минимальным характеристикам. Работоспособность не может быть "
                  "гарантирована. Ознакомьтесь с "}},
    {"hw_link", {"system requirements", "техническими требованиями"}},
    {"hw_text2", {" of the game.", " игры."}},
    {"hw_go", {"Continue anyway", "Всё равно продолжить"}},
    {"play_title", {"UltraTears is ready", "UltraTears готова"}},
    {"play_sub", {"The game is prepared from your copy and installed in {path}.", "Игра подготовлена из вашей копии и установлена в {path}."}},
    {"play", {"Play", "Играть"}},
    {"play_failed", {"UltraTears.exe did not start.", "Не удалось запустить UltraTears.exe."}},
    // the exe alone, without the rest of the zip: run from inside the zip, or copied somewhere by itself
    {"no_python", {"The launcher's files are missing: extract the whole archive and start the launcher from the extracted folder.",
                   "Не найдены файлы лаунчера: распакуйте архив целиком и запускайте лаунчер из распакованной папки."}},
    {"setup_crash", {"The builder stopped unexpectedly (exit code {code}).", "Сборщик неожиданно остановился (код {code})."}},
    {"error_generic", {"error: the details are in the log", "ошибка — подробности в логе"}},
    {"min", {"{m} min", "{m} мин"}},
    {"ms", {"{m}m {s}s", "{m}м {s}с"}},
    {"s", {"{s}s", "{s}с"}},
    {"hm", {"{h}h {m}m", "{h}ч {m}м"}},

    // the dump (setup.py --check-dump)
    {"dump_missing", {"The folder does not exist", "Папка не найдена"}},
    {"dump_missing.d", {"Choose the folder with your extracted game.", "Выберите папку с извлечённой игрой."}},
    {"dump_encrypted", {"This is an encrypted game package", "Это зашифрованный файл игры"}},
    {"dump_encrypted.d", {"{file}: the extracted exefs and romfs folders are needed. UltraTears decrypts nothing.",
                          "{file}: нужны уже извлечённые папки exefs и romfs. UltraTears ничего не расшифровывает."}},
    {"dump_no_folders", {"There is no game in this folder", "В папке нет игры"}},
    {"dump_no_folders.d", {"Choose the folder that holds exefs and romfs (extracted, as plain files).",
                           "Нужна папка, в которой лежат exefs и romfs (извлечённые, обычными файлами)."}},
    {"dump_incomplete", {"The game is not extracted completely", "Игра извлечена не полностью"}},
    {"dump_incomplete.d", {"{file} is missing. Extract the game again.", "Не хватает {file}. Извлеките игру заново."}},
    {"dump_version", {"This version of the game does not fit", "Эта версия игры не подходит"}},
    {"dump_version.d", {"Version 1.4.3 is needed. Update the original game to 1.4.3 or choose another folder.",
                        "Нужна версия 1.4.3. Обновите оригинальную игру до 1.4.3 или укажите другой путь."}},
    {"dump_not_totk", {"This is not Tears of the Kingdom", "Это не Tears of the Kingdom"}},
    {"dump_not_totk.d", {"The romfs folder has none of this game's files.", "В папке romfs нет файлов этой игры."}},
    {"romfs_version", {"the RomFS is not of version 1.4.3", "RomFS не от версии 1.4.3"}},
    {"tools", {"the build tools could not be prepared", "не удалось подготовить инструменты сборки"}},
    {"disk_space", {"not enough disk space: {need_gb} GB needed on {drive}, {free_gb} GB free",
                    "не хватает места: нужно {need_gb} ГБ на {drive}, свободно {free_gb} ГБ"}},

    // the hardware (tools/hw_check.py)
    {"no_vulkan", {"No Vulkan graphics driver was found", "Не найден графический драйвер Vulkan"}},
    {"no_vulkan.d", {"Install the current driver for your NVIDIA, AMD or Intel card.",
                     "Установите свежий драйвер видеокарты NVIDIA, AMD или Intel."}},
    {"vulkan_old", {"The {gpu} driver offers Vulkan {version}; 1.3 is needed", "Драйвер {gpu} даёт Vulkan {version}, нужен 1.3"}},
    {"vulkan_old.d", {"Updating the graphics driver usually helps.", "Обычно помогает обновление драйвера видеокарты."}},
    {"no_shader_object", {"{gpu} lacks VK_EXT_shader_object", "{gpu} не поддерживает VK_EXT_shader_object"}},
    {"no_shader_object.d", {"Usual for AMD RX 400/500, Vega and Intel HD: such cards are not supported yet.",
                            "Так бывает у AMD RX 400/500, Vega и Intel HD — такие видеокарты пока не поддерживаются."}},
    {"no_shader_object.layer", {"It can run through the installed Khronos shader object layer: slower and with glitches.",
                                "Игра может работать через установленный слой Khronos shader object — медленнее и с ошибками "
                                "изображения."}},
    {"no_ext_mem_host", {"The {gpu} driver lacks VK_EXT_external_memory_host", "Драйвер {gpu} не поддерживает VK_EXT_external_memory_host"}},
    {"no_ext_mem_host.d", {"Update the graphics driver.", "Обновите драйвер видеокарты."}},
    {"no_swapchain", {"{gpu} cannot show pictures", "{gpu} не умеет выводить изображение"}},
    {"no_swapchain.d", {"It is a compute device without a display output.", "Это вычислительное устройство без вывода на экран."}},
    {"vram_low", {"{gpu}: {gb} GB of video memory", "{gpu}: {gb} ГБ видеопамяти"}},
    {"vram_low.d", {"4 GB or more is recommended: the game may stutter.", "Рекомендуется 4 ГБ и больше — возможны подтормаживания."}},
    {"ram_low", {"{gb} GB of RAM", "Оперативной памяти {gb} ГБ"}},
    {"ram_low.d", {"16 GB or more is recommended.", "Рекомендуется 16 ГБ и больше."}},
    {"cpu_no_avx2", {"The CPU lacks AVX2", "Процессор без AVX2"}},
    {"cpu_no_avx2.d", {"The game is built for SSE4.2 and runs a little slower.", "Игра соберётся под SSE4.2 и будет работать немного медленнее."}},
    {"amd_untested", {"{gpu}: AMD cards are not tested yet", "{gpu}: видеокарты AMD ещё не проверены"}},
    {"amd_untested.d", {"Please tell how the game runs: it helps.", "Расскажите, как идёт игра, — это поможет."}},
};

static const Text *find(const char *key) {
    for (const Text &t : TEXTS)
        if (std::strcmp(t.key, key) == 0)
            return &t;
    return nullptr;
}

bool has_text(const char *key) { return find(key) != nullptr; }

const char *T(const char *key) {
    const Text *t = find(key);
    if (!t)
        return key;
    return t->s[g_lang] ? t->s[g_lang] : t->s[LANG_EN];
}

static std::string fill(const char *text, const std::vector<std::pair<std::string, std::string>> &args) {
    std::string out;
    for (const char *p = text; *p;) {
        if (*p == '{') {
            const char *e = std::strchr(p, '}');
            if (e) {
                std::string name(p + 1, e);
                bool found = false;
                for (auto &a : args)
                    if (a.first == name) {
                        out += a.second;
                        found = true;
                        break;
                    }
                if (found) {
                    p = e + 1;
                    continue;
                }
            }
        }
        out += *p++;
    }
    return out;
}

std::string Tf(const char *key, std::initializer_list<std::pair<const char *, std::string>> args) {
    std::vector<std::pair<std::string, std::string>> v;
    for (auto &a : args)
        v.emplace_back(a.first, a.second);
    return fill(T(key), v);
}

std::string Tf(const char *key, const Json &args) {
    std::vector<std::pair<std::string, std::string>> v;
    for (size_t i = 0; i < args.keys.size(); i++)
        v.emplace_back(args.keys[i], args.items[i].str());
    return fill(T(key), v);
}

const char *lang_name(Lang l) { return l == LANG_RU ? "Русский" : "English"; }
const char *lang_code(Lang l) { return l == LANG_RU ? "ru" : "en"; }
Lang lang_from_code(const char *code) { return code && SDL_strncasecmp(code, "ru", 2) == 0 ? LANG_RU : LANG_EN; }

std::string fmt_duration(double secs) {
    long s = (long)(secs + 0.5);
    if (s < 60)
        return Tf("s", {{"s", std::to_string(s)}});
    if (s < 3600 && s % 60 == 0)
        return Tf("min", {{"m", std::to_string(s / 60)}});
    if (s < 3600)
        return Tf("ms", {{"m", std::to_string(s / 60)}, {"s", std::to_string(s % 60)}});
    return Tf("hm", {{"h", std::to_string(s / 3600)}, {"m", std::to_string(s / 60 % 60)}});
}

std::string fmt_minutes(double secs) {
    long m = (long)(secs / 60 + 0.5);
    return Tf("min", {{"m", std::to_string(m < 1 ? 1 : m)}});
}
