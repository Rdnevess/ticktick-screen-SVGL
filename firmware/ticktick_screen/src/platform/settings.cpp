#include "settings.h"

#include <Preferences.h>

#include <cstdio>

static Settings s_settings;

Settings &settings() { return s_settings; }

static void defaults(Settings &s) {
    s = Settings{};
    s.pollMin = DEFAULT_POLL_MIN;
    s.pomoMin = DEFAULT_POMO_MIN;
    s.langEn = false;
    s.tzMin = DEFAULT_TZ_MIN;
    s.tlsInsecure = false;
    s.briIdx = 1; // medio
    s.idleClock = true;
}

// Pin na NVS: "projectId:taskId".
static bool pin_from_str(const String &str, Pin &p) {
    const int sep = str.indexOf(':');
    if (sep <= 0 || sep >= (int)str.length() - 1) return false;
    task_set_id(p.projectId, str.substring(0, sep).c_str());
    task_set_id(p.taskId, str.substring(sep + 1).c_str());
    return true;
}

void settings_load() {
    Settings &s = s_settings;
    defaults(s);
    Preferences p;
    if (!p.begin(NVS_CFG, true)) return; // primeiro boot: namespace nao existe

    s.pollMin = p.getInt("poll", s.pollMin);
    s.pomoMin = p.getInt("pomo", s.pomoMin);
    s.langEn = p.getBool("lang", s.langEn);
    s.tzMin = p.getInt("tz", s.tzMin);
    s.tlsInsecure = p.getBool("tls", s.tlsInsecure);
    s.briIdx = p.getInt("bri", s.briIdx);
    if (s.briIdx < 0 || s.briIdx > 2) s.briIdx = 1;
    s.idleClock = p.getBool("idle", s.idleClock);

    const String lists = p.getString("lists", "");
    int start = 0;
    while (start < (int)lists.length() && s.listCount < MAX_LISTS) {
        int comma = lists.indexOf(',', start);
        if (comma < 0) comma = lists.length();
        if (comma > start)
            task_set_id(s.lists[s.listCount++], lists.substring(start, comma).c_str());
        start = comma + 1;
    }

    for (int i = 0; i < FOCUS_SLOTS; i++) {
        char key[6];
        snprintf(key, sizeof(key), "pin%d", i + 1);
        if (pin_from_str(p.getString(key, ""), s.pins.pins[s.pins.count])) s.pins.count++;
    }
    p.end();
}

void settings_save() {
    const Settings &s = s_settings;
    Preferences p;
    if (!p.begin(NVS_CFG, false)) return;

    p.putInt("poll", s.pollMin);
    p.putInt("pomo", s.pomoMin);
    p.putBool("lang", s.langEn);
    p.putInt("tz", s.tzMin);
    p.putBool("tls", s.tlsInsecure);
    p.putInt("bri", s.briIdx);
    p.putBool("idle", s.idleClock);

    String lists;
    for (int i = 0; i < s.listCount; i++) {
        if (i) lists += ',';
        lists += s.lists[i];
    }
    p.putString("lists", lists);

    for (int i = 0; i < FOCUS_SLOTS; i++) {
        char key[6];
        snprintf(key, sizeof(key), "pin%d", i + 1);
        if (i < s.pins.count) {
            char buf[2 * TASK_ID_BYTES + 2];
            snprintf(buf, sizeof(buf), "%s:%s", s.pins.pins[i].projectId, s.pins.pins[i].taskId);
            p.putString(key, buf);
        } else {
            p.remove(key);
        }
    }
    p.end();
}

void settings_reset() {
    defaults(s_settings);
    Preferences p;
    if (p.begin(NVS_CFG, false)) {
        p.clear();
        p.end();
    }
}
