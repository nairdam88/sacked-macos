#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0501

#include <windows.h>
#include <windowsx.h>
#include <psapi.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HOTKEY_TOGGLE 1
#define HOTKEY_QUIT 2
#define HOTKEY_OSD 3
#define HOTKEY_MENU 4

#define WM_RELOCK_CURSOR (WM_APP + 111)
#define WM_TOGGLE_FULLSCREEN (WM_APP + 117)
#define WM_MENU_REQUEST (WM_APP + 118)
#define WM_SACKED_MENU (WM_APP + 120)

#define MENU_DLL_NAME "sacked-menu.dll"

#define OSD_CLASS "SackedOsdWindow"
#define OSD_TIMER 1
#define OSD_TIMEOUT_SECONDS 10
#define OSD_WIDTH 220
#define OSD_HEIGHT 222
#define OSD_ALPHA 238

static char g_log_path[MAX_PATH];
static HWND g_osd;
static HWND g_game;
static HHOOK g_keyboard_hook;
static HANDLE g_menu_ready;
static HANDLE g_menu_done;
static int g_menu_injected;
static int g_menu_hook_failed;
static DWORD g_last_escape;
static DWORD g_last_main_menu;
static DWORD g_game_pid;
static int g_language_hu;
static int g_large_window;
static int g_hide_seconds = OSD_TIMEOUT_SECONDS;
static char g_status[128];
static int g_hover;
static DWORD g_relock_at;
static int g_relock_left;
static int g_was_fullscreen;

#define OSD_MARGIN 12
#define OSD_BUTTON_W 196
#define OSD_BUTTON_H 32
static const RECT g_button_full = {
    OSD_MARGIN, 54, OSD_MARGIN + OSD_BUTTON_W, 54 + OSD_BUTTON_H
};
static const RECT g_button_large = {
    OSD_MARGIN, 92, OSD_MARGIN + OSD_BUTTON_W, 92 + OSD_BUTTON_H
};
static const RECT g_button_language = {
    OSD_MARGIN, 130, OSD_MARGIN + OSD_BUTTON_W, 130 + OSD_BUTTON_H
};
static const RECT g_button_quit = {
    OSD_MARGIN, 168, OSD_MARGIN + OSD_BUTTON_W, 168 + OSD_BUTTON_H
};

static void hide_osd(void);
static void show_osd(void);
static void request_main_menu(void);
static void read_window_mode(void);
static void write_window_mode(void);
static void write_restart_file(void);
static void toggle_fullscreen(HWND game);
static void schedule_cursor_relock(void);
static void run_cursor_relock(void);
static void log_window_state(const char *tag);
static void quit_game(HWND game, DWORD pid);
static void write_language_files(void);
static LRESULT CALLBACK low_level_keyboard_proc(int code, WPARAM wparam,
                                                 LPARAM lparam);
static HWND find_game_window(DWORD *pid);
static int inject_menu_dll(DWORD pid, const char *dll_path);
static int install_menu_hook(HWND game, DWORD pid);
static void remove_menu_hook(void);
static int send_menu_request(HWND game);

static void log_init(void)
{
    char path[MAX_PATH];
    char *slash;

    if (!GetModuleFileNameA(NULL, path, sizeof(path)))
        return;

    slash = strrchr(path, '\\');
    if (slash)
        slash[1] = '\0';
    else
        path[0] = '\0';

    snprintf(g_log_path, sizeof(g_log_path), "%ssacked-hotkeys.log", path);
}

static void log_msg(const char *fmt, ...)
{
    SYSTEMTIME st;
    char buf[1024];
    int used;
    va_list ap;
    DWORD written;
    HANDLE file;

    if (!g_log_path[0])
        return;

    file = CreateFileA(g_log_path, FILE_APPEND_DATA,
                       FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                       OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE)
        return;

    GetLocalTime(&st);
    used = snprintf(buf, sizeof(buf), "[%04u-%02u-%02u %02u:%02u:%02u] ",
                    st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    if (used < 0)
        used = 0;
    if (used > (int)sizeof(buf) - 3)
        used = (int)sizeof(buf) - 3;

    va_start(ap, fmt);
    vsnprintf(buf + used, sizeof(buf) - used - 2, fmt, ap);
    va_end(ap);

    strncat(buf, "\r\n", sizeof(buf) - strlen(buf) - 1);
    WriteFile(file, buf, (DWORD)strlen(buf), &written, NULL);
    CloseHandle(file);
}

static int contains_ci(const char *haystack, const char *needle)
{
    size_t length = strlen(needle);

    for (; *haystack; ++haystack)
    {
        if (!_strnicmp(haystack, needle, length))
            return 1;
    }

    return 0;
}

typedef struct
{
    HWND hwnd;
    DWORD pid;
} find_context;

static BOOL CALLBACK find_window_proc(HWND hwnd, LPARAM lparam)
{
    find_context *ctx = (find_context *)lparam;
    DWORD pid = 0;
    BOOL is_game = FALSE;
    HANDLE process;
    char exe[MAX_PATH];
    char title[256];
    char *base;

    if (hwnd == g_osd)
        return TRUE;

    {
        char class_name[64];
        class_name[0] = '\0';
        GetClassNameA(hwnd, class_name, sizeof(class_name));
        if (!_stricmp(class_name, OSD_CLASS))
            return TRUE;
    }

    GetWindowThreadProcessId(hwnd, &pid);
    if (!pid)
        return TRUE;

    process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (process)
    {
        if (GetModuleFileNameExA(process, NULL, exe, sizeof(exe)))
        {
            base = strrchr(exe, '\\');
            base = base ? base + 1 : exe;
            if (!_stricmp(base, "sacked.exe"))
                is_game = TRUE;
        }
        CloseHandle(process);
    }

    if (!is_game)
    {
        title[0] = '\0';
        GetWindowTextA(hwnd, title, sizeof(title));
        if (contains_ci(title, "Sacked") || contains_ci(title, "Gefeuert") ||
            contains_ci(title, "Last Day"))
            is_game = TRUE;
    }

    if (!is_game)
        return TRUE;

    if (IsWindowVisible(hwnd))
    {
        ctx->hwnd = hwnd;
        ctx->pid = pid;
        return FALSE;
    }

    if (!ctx->hwnd)
    {
        ctx->hwnd = hwnd;
        ctx->pid = pid;
    }

    return TRUE;
}

static HWND find_game_window(DWORD *pid)
{
    find_context ctx;

    ctx.hwnd = NULL;
    ctx.pid = 0;
    EnumWindows(find_window_proc, (LPARAM)&ctx);
    if (pid)
        *pid = ctx.pid;
    return ctx.hwnd;
}

static void read_language(void)
{
    char buf[32] = { 0 };
    DWORD read = 0;
    HANDLE file = CreateFileA("language", GENERIC_READ,
                              FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if (file != INVALID_HANDLE_VALUE)
    {
        ReadFile(file, buf, sizeof(buf) - 1, &read, NULL);
        CloseHandle(file);
    }

    g_language_hu = strstr(buf, "hu") != NULL || strstr(buf, "HU") != NULL;
}

static void read_window_mode(void)
{
    char buf[32] = { 0 };
    DWORD read = 0;
    HANDLE file = CreateFileA("window-mode", GENERIC_READ,
                              FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if (file != INVALID_HANDLE_VALUE)
    {
        ReadFile(file, buf, sizeof(buf) - 1, &read, NULL);
        CloseHandle(file);
    }

    g_large_window = strstr(buf, "large") != NULL;
    log_msg("window mode: %s", g_large_window ? "1200x900" : "800x600");
}

static void write_window_mode(void)
{
    DWORD written;
    HANDLE file = CreateFileA("window-mode", GENERIC_WRITE, FILE_SHARE_READ,
                              NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    if (file != INVALID_HANDLE_VALUE)
    {
        const char *text = g_large_window ? "large\n" : "normal\n";
        WriteFile(file, text, (DWORD)strlen(text), &written, NULL);
        CloseHandle(file);
    }

    write_restart_file();
    log_msg("window mode set to %s", g_large_window ? "large" : "normal");
}

static void write_restart_file(void)
{
    DWORD written;
    HANDLE file = CreateFileA("restart", GENERIC_WRITE, FILE_SHARE_READ, NULL,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    if (file != INVALID_HANDLE_VALUE)
    {
        const char *text = "restart\n";
        WriteFile(file, text, (DWORD)strlen(text), &written, NULL);
        CloseHandle(file);
    }

    log_msg("restart requested");
}

static void write_language_files(void)
{
    DWORD written;
    HANDLE file = CreateFileA("language", GENERIC_WRITE, FILE_SHARE_READ, NULL,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    if (file != INVALID_HANDLE_VALUE)
    {
        const char *text = g_language_hu ? "hu\n" : "en\n";
        WriteFile(file, text, (DWORD)strlen(text), &written, NULL);
        CloseHandle(file);
    }

    write_restart_file();
    log_msg("language set to %s", g_language_hu ? "hu" : "en");
}

static void request_main_menu(void)
{
    DWORD now = GetTickCount();

    if (now - g_last_main_menu < 500)
        return;

    if (!g_game || !IsWindow(g_game))
        g_game = find_game_window(&g_game_pid);

    if (!g_game || !g_game_pid)
        return;

    g_last_main_menu = now;
    hide_osd();

    if (!g_menu_injected && !install_menu_hook(g_game, g_game_pid))
    {
        snprintf(g_status, sizeof(g_status), "Main-menu hook unavailable");
        show_osd();
        return;
    }

    if (!send_menu_request(g_game))
    {
        snprintf(g_status, sizeof(g_status), "Could not open the main menu");
        show_osd();
    }
}

static LRESULT CALLBACK low_level_keyboard_proc(int code, WPARAM wparam,
                                                 LPARAM lparam)
{
    if (code == HC_ACTION &&
        (wparam == WM_KEYDOWN || wparam == WM_SYSKEYDOWN))
    {
        KBDLLHOOKSTRUCT *key = (KBDLLHOOKSTRUCT *)lparam;

        if (key->vkCode == VK_ESCAPE && !(key->flags & LLKHF_INJECTED))
        {
            DWORD now = GetTickCount();

            if (now - g_last_escape > 500)
            {
                g_last_escape = now;
                if (g_osd)
                    PostMessageA(g_osd, WM_MENU_REQUEST, 0, 0);
            }
            return 1;
        }
    }

    return CallNextHookEx(g_keyboard_hook, code, wparam, lparam);
}

static void toggle_fullscreen(HWND game)
{
    if (!game || !IsWindow(game))
        return;

    log_msg("toggle fullscreen");
    PostMessageA(game, WM_TOGGLE_FULLSCREEN, 0, 0);
    schedule_cursor_relock();
}

static void schedule_cursor_relock(void)
{
    g_relock_at = GetTickCount() + 700;
    g_relock_left = 2;
    log_msg("scheduled cursor relock");
}

static void log_window_state(const char *tag)
{
    RECT window_rect = { 0 };
    RECT client_rect = { 0 };
    RECT clip_rect = { 0 };
    POINT cursor = { 0 };
    HWND foreground = GetForegroundWindow();
    BOOL have_clip = FALSE;

    if (g_game && IsWindow(g_game))
    {
        GetWindowRect(g_game, &window_rect);
        GetClientRect(g_game, &client_rect);
    }
    GetCursorPos(&cursor);
    have_clip = GetClipCursor(&clip_rect);

    log_msg(
        "%s: window=%ld,%ld %ldx%ld client=%ldx%ld clip=%s %ld,%ld %ldx%ld "
        "cursor=%ld,%ld foreground=%p game=%p",
        tag,
        (long)window_rect.left, (long)window_rect.top,
        (long)(window_rect.right - window_rect.left),
        (long)(window_rect.bottom - window_rect.top),
        (long)client_rect.right, (long)client_rect.bottom,
        have_clip ? "yes" : "no",
        (long)clip_rect.left, (long)clip_rect.top,
        (long)(clip_rect.right - clip_rect.left),
        (long)(clip_rect.bottom - clip_rect.top),
        (long)cursor.x, (long)cursor.y,
        (void *)foreground, (void *)g_game);
}

static int is_fullscreen_window(void)
{
    RECT window_rect;
    RECT monitor_rect;
    MONITORINFO monitor_info;
    HMONITOR monitor;

    if (!g_game || !IsWindow(g_game))
        return 0;
    if (!GetWindowRect(g_game, &window_rect))
        return 0;

    monitor = MonitorFromWindow(g_game, MONITOR_DEFAULTTONEAREST);
    memset(&monitor_info, 0, sizeof(monitor_info));
    monitor_info.cbSize = sizeof(monitor_info);
    if (!GetMonitorInfoA(monitor, &monitor_info))
        return 0;

    monitor_rect = monitor_info.rcMonitor;
    return window_rect.left <= monitor_rect.left + 2 &&
           window_rect.top <= monitor_rect.top + 2 &&
           window_rect.right >= monitor_rect.right - 2 &&
           window_rect.bottom >= monitor_rect.bottom - 2;
}

static void run_cursor_relock(void)
{
    DWORD now;

    if (!g_relock_left || !g_game || !IsWindow(g_game))
        return;

    now = GetTickCount();
    if ((LONG)(now - g_relock_at) < 0)
        return;
    if (!is_fullscreen_window())
    {
        if (now - g_relock_at > 3000)
            g_relock_left = 0;
        return;
    }

    log_window_state("before cursor relock");
    PostMessageA(g_game, WM_RELOCK_CURSOR, 0, 0);
    --g_relock_left;
    g_relock_at = GetTickCount() + 900;
}

static void quit_game(HWND game, DWORD pid)
{
    HANDLE process;

    if (!game || !IsWindow(game))
        return;

    log_msg("quit requested");
    PostMessageA(game, WM_CLOSE, 0, 0);

    process = OpenProcess(SYNCHRONIZE | PROCESS_TERMINATE, FALSE, pid);
    if (!process)
    {
        log_msg("could not open game process (%lu)", GetLastError());
        return;
    }

    if (WaitForSingleObject(process, 5000) == WAIT_TIMEOUT)
    {
        log_msg("game did not close; terminating");
        TerminateProcess(process, 1);
    }

    CloseHandle(process);
}

static int inject_menu_dll(DWORD pid, const char *dll_path)
{
    HANDLE process;
    HANDLE thread;
    HMODULE kernel;
    FARPROC load_library;
    void *remote_path;
    DWORD path_size;
    DWORD thread_id = 0;
    DWORD exit_code = 0;
    int ok = 0;

    process = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
                          PROCESS_VM_OPERATION | PROCESS_VM_WRITE |
                          PROCESS_VM_READ,
                          FALSE, pid);
    if (!process)
    {
        log_msg("menu hook: OpenProcess failed (%lu)", GetLastError());
        return 0;
    }

    path_size = (DWORD)strlen(dll_path) + 1;
    remote_path = VirtualAllocEx(process, NULL, path_size,
                                 MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote_path)
    {
        log_msg("menu hook: VirtualAllocEx failed (%lu)", GetLastError());
        CloseHandle(process);
        return 0;
    }

    if (!WriteProcessMemory(process, remote_path, dll_path, path_size, NULL))
    {
        log_msg("menu hook: WriteProcessMemory failed (%lu)", GetLastError());
        VirtualFreeEx(process, remote_path, 0, MEM_RELEASE);
        CloseHandle(process);
        return 0;
    }

    kernel = GetModuleHandleA("kernel32.dll");
    load_library = kernel ? GetProcAddress(kernel, "LoadLibraryA") : NULL;
    if (!load_library)
    {
        log_msg("menu hook: could not resolve LoadLibraryA (%lu)",
                GetLastError());
        VirtualFreeEx(process, remote_path, 0, MEM_RELEASE);
        CloseHandle(process);
        return 0;
    }

    thread = CreateRemoteThread(process, NULL, 0,
                                (LPTHREAD_START_ROUTINE)load_library,
                                remote_path, 0, &thread_id);
    if (!thread)
    {
        log_msg("menu hook: CreateRemoteThread failed (%lu)", GetLastError());
        VirtualFreeEx(process, remote_path, 0, MEM_RELEASE);
        CloseHandle(process);
        return 0;
    }

    if (WaitForSingleObject(thread, 10000) != WAIT_OBJECT_0)
    {
        log_msg("menu hook: LoadLibrary did not finish (thread %lu)",
                (unsigned long)thread_id);
    }
    else if (GetExitCodeThread(thread, &exit_code) && exit_code)
    {
        ok = 1;
        log_msg("menu hook: DLL loaded in pid %lu (module %lu)",
                (unsigned long)pid, (unsigned long)exit_code);
    }
    else
    {
        log_msg("menu hook: LoadLibrary failed in game process (%lu)",
                GetLastError());
    }

    CloseHandle(thread);
    if (ok)
        VirtualFreeEx(process, remote_path, 0, MEM_RELEASE);
    CloseHandle(process);
    return ok;
}

static int install_menu_hook(HWND game, DWORD pid)
{
    char dll_path[MAX_PATH];
    char event_name[64];
    char *slash;

    if (g_menu_injected)
        return 1;
    if (g_menu_hook_failed)
        return 0;
    if (!game || !IsWindow(game) || !pid)
        return 0;

    if (!GetModuleFileNameA(NULL, dll_path, sizeof(dll_path)))
    {
        log_msg("menu hook: GetModuleFileName failed (%lu)", GetLastError());
        g_menu_hook_failed = 1;
        return 0;
    }

    slash = strrchr(dll_path, '\\');
    if (!slash || strlen(dll_path) + strlen(MENU_DLL_NAME) >= sizeof(dll_path))
    {
        log_msg("menu hook: invalid helper path");
        g_menu_hook_failed = 1;
        return 0;
    }
    strcpy(slash + 1, MENU_DLL_NAME);

    snprintf(event_name, sizeof(event_name),
             "Local\\SackedMenuReady_%lu", (unsigned long)pid);
    g_menu_ready = CreateEventA(NULL, TRUE, FALSE, event_name);
    if (!g_menu_ready)
    {
        log_msg("menu hook: CreateEvent ready failed (%lu)", GetLastError());
        g_menu_hook_failed = 1;
        return 0;
    }

    snprintf(event_name, sizeof(event_name),
             "Local\\SackedMenuDone_%lu", (unsigned long)pid);
    g_menu_done = CreateEventA(NULL, TRUE, FALSE, event_name);
    if (!g_menu_done)
    {
        log_msg("menu hook: CreateEvent done failed (%lu)", GetLastError());
        CloseHandle(g_menu_ready);
        g_menu_ready = NULL;
        g_menu_hook_failed = 1;
        return 0;
    }

    if (!inject_menu_dll(pid, dll_path))
    {
        CloseHandle(g_menu_ready);
        CloseHandle(g_menu_done);
        g_menu_ready = NULL;
        g_menu_done = NULL;
        g_menu_hook_failed = 1;
        return 0;
    }

    if (WaitForSingleObject(g_menu_ready, 12000) != WAIT_OBJECT_0)
    {
        log_msg("menu hook: DLL did not install the window subclass");
        CloseHandle(g_menu_ready);
        CloseHandle(g_menu_done);
        g_menu_ready = NULL;
        g_menu_done = NULL;
        g_menu_hook_failed = 1;
        return 0;
    }

    g_menu_injected = 1;
    log_msg("menu hook installed (pid %lu, hwnd %p, dll %s)",
            (unsigned long)pid, (void *)game, dll_path);
    return 1;
}

static void remove_menu_hook(void)
{
    if (g_menu_ready)
    {
        CloseHandle(g_menu_ready);
        g_menu_ready = NULL;
    }
    if (g_menu_done)
    {
        CloseHandle(g_menu_done);
        g_menu_done = NULL;
    }
    g_menu_injected = 0;
    g_menu_hook_failed = 0;
}

static int send_menu_request(HWND game)
{
    if (!game || !g_menu_injected || !g_menu_done)
        return 0;

    ResetEvent(g_menu_done);
    log_msg("menu request: posting to hwnd %p", (void *)game);
    if (!PostMessageA(game, WM_SACKED_MENU, 0, 0))
    {
        log_msg("menu request: PostMessage failed (%lu)", GetLastError());
        return 0;
    }

    if (WaitForSingleObject(g_menu_done, 5000) != WAIT_OBJECT_0)
    {
        log_msg("menu request: DLL did not acknowledge");
        return 0;
    }

    log_msg("menu request handled on the game thread");
    return 1;
}

static void draw_button(HDC dc, const RECT *rect, const char *text,
                       int hover, int danger)
{
    COLORREF fill_color = danger ? RGB(69, 35, 44) : RGB(30, 41, 59);
    COLORREF border_color = danger ? RGB(190, 80, 90) : RGB(71, 85, 105);
    COLORREF text_color = RGB(248, 250, 252);
    HBRUSH fill;
    HPEN border;
    HFONT font;
    HGDIOBJ old_brush;
    HGDIOBJ old_pen;
    HGDIOBJ old_font;
    RECT draw = *rect;

    if (hover)
    {
        fill_color = danger ? RGB(111, 45, 55) : RGB(38, 52, 73);
        border_color = danger ? RGB(248, 113, 113) : RGB(56, 189, 248);
    }

    fill = CreateSolidBrush(fill_color);
    border = CreatePen(PS_SOLID, 1, border_color);
    font = CreateFontA(-11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                       DEFAULT_PITCH | FF_DONTCARE, "Arial");

    old_brush = SelectObject(dc, fill);
    old_pen = SelectObject(dc, border);
    RoundRect(dc, rect->left, rect->top, rect->right, rect->bottom, 10, 10);
    SelectObject(dc, old_brush);
    SelectObject(dc, old_pen);
    DeleteObject(fill);
    DeleteObject(border);

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, text_color);
    old_font = SelectObject(dc, font);
    DrawTextA(dc, text, -1, &draw, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(dc, old_font);
    DeleteObject(font);
}

static int hit_button(POINT point)
{
    if (PtInRect(&g_button_full, point))
        return 1;
    if (PtInRect(&g_button_large, point))
        return 2;
    if (PtInRect(&g_button_language, point))
        return 3;
    if (PtInRect(&g_button_quit, point))
        return 4;
    return 0;
}

static void draw_osd(HWND hwnd)
{
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hwnd, &ps);
    RECT countdown_rect = { 144, 7, 208, 27 };
    RECT hint_rect = { OSD_MARGIN, 204, OSD_WIDTH - OSD_MARGIN, 218 };
    char countdown[64];
    const char *hint = "Esc  Menu     Ctrl+Cmd+H  OSD";
    HBRUSH background = CreateSolidBrush(RGB(15, 20, 30));
    HPEN border = CreatePen(PS_SOLID, 1, RGB(51, 65, 85));
    HPEN accent = CreatePen(PS_SOLID, 2, RGB(56, 189, 248));
    HPEN divider = CreatePen(PS_SOLID, 1, RGB(30, 41, 59));
    HBRUSH pill_fill = CreateSolidBrush(RGB(17, 24, 39));
    HPEN pill_border = CreatePen(PS_SOLID, 1, RGB(51, 65, 85));
    HFONT title_font = CreateFontA(-18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                   DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                   CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                   DEFAULT_PITCH | FF_DONTCARE, "Arial");
    HFONT status_font = CreateFontA(-9, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                    CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                    DEFAULT_PITCH | FF_DONTCARE, "Arial");
    HFONT pill_font = CreateFontA(-8, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                  CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                  DEFAULT_PITCH | FF_DONTCARE, "Arial");
    HFONT hint_font = CreateFontA(-8, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                  CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                  DEFAULT_PITCH | FF_DONTCARE, "Arial");
    HGDIOBJ old_brush;
    HGDIOBJ old_pen;
    HGDIOBJ old_font;

    snprintf(countdown, sizeof(countdown), "Hide in %ds", g_hide_seconds);

    old_brush = SelectObject(dc, background);
    old_pen = SelectObject(dc, border);
    RoundRect(dc, 1, 1, OSD_WIDTH - 1, OSD_HEIGHT - 1, 12, 12);
    SelectObject(dc, old_brush);
    SelectObject(dc, old_pen);
    DeleteObject(background);
    DeleteObject(border);

    old_pen = SelectObject(dc, accent);
    MoveToEx(dc, 10, 2, NULL);
    LineTo(dc, OSD_WIDTH - 10, 2);
    SelectObject(dc, old_pen);
    DeleteObject(accent);

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(248, 250, 252));
    old_font = SelectObject(dc, title_font);
    TextOutA(dc, OSD_MARGIN, 7, "SACKED", 6);
    SelectObject(dc, old_font);
    DeleteObject(title_font);

    SetTextColor(dc, RGB(148, 163, 184));
    old_font = SelectObject(dc, status_font);
    TextOutA(dc, OSD_MARGIN, 28, g_status, (int)strlen(g_status));
    SelectObject(dc, old_font);
    DeleteObject(status_font);

    old_brush = SelectObject(dc, pill_fill);
    old_pen = SelectObject(dc, pill_border);
    RoundRect(dc, countdown_rect.left, countdown_rect.top,
              countdown_rect.right, countdown_rect.bottom, 10, 10);
    SelectObject(dc, old_brush);
    SelectObject(dc, old_pen);
    DeleteObject(pill_fill);
    DeleteObject(pill_border);

    old_font = SelectObject(dc, pill_font);
    SetTextColor(dc, g_hide_seconds <= 3 ? RGB(248, 113, 113)
                                         : RGB(148, 163, 184));
    DrawTextA(dc, countdown, -1, &countdown_rect,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(dc, old_font);
    DeleteObject(pill_font);

    old_pen = SelectObject(dc, divider);
    MoveToEx(dc, OSD_MARGIN, 44, NULL);
    LineTo(dc, OSD_WIDTH - OSD_MARGIN, 44);
    SelectObject(dc, old_pen);
    DeleteObject(divider);

    draw_button(dc, &g_button_full, "Toggle Fullscreen", g_hover == 1, 0);
    draw_button(dc, &g_button_large,
                g_large_window ? "Window 1.5x: On" : "Window 1.5x",
                g_hover == 2, 0);
    draw_button(dc, &g_button_language,
                g_language_hu ? "Language: English" : "Language: Magyar",
                g_hover == 3, 0);
    draw_button(dc, &g_button_quit, "Quit", g_hover == 4, 1);

    old_font = SelectObject(dc, hint_font);
    SetTextColor(dc, RGB(148, 163, 184));
    DrawTextA(dc, hint, -1, &hint_rect,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SelectObject(dc, old_font);
    DeleteObject(hint_font);

    EndPaint(hwnd, &ps);
}

static void position_osd(void)
{
    int x;
    int y;

    if (!g_osd)
        return;

    if (g_game && IsWindow(g_game))
    {
        RECT client;
        POINT origin = { 0, 0 };

        GetClientRect(g_game, &client);
        ClientToScreen(g_game, &origin);
        x = origin.x + 12;
        y = origin.y + 12;
        if (x < origin.x)
            x = origin.x;
        if (y < origin.y)
            y = origin.y;
    }
    else
    {
        x = 12;
        y = 12;
    }

    SetWindowPos(g_osd, HWND_TOPMOST, x, y, OSD_WIDTH, OSD_HEIGHT,
                 SWP_NOACTIVATE);
}

static void hide_osd(void)
{
    if (!g_osd)
        return;

    KillTimer(g_osd, OSD_TIMER);
    g_hover = 0;
    ShowWindow(g_osd, SW_HIDE);
}

static void show_osd(void)
{
    if (!g_osd)
        return;

    position_osd();
    g_hide_seconds = OSD_TIMEOUT_SECONDS;
    g_hover = 0;
    InvalidateRect(g_osd, NULL, FALSE);
    ShowWindow(g_osd, SW_SHOWNOACTIVATE);
    SetWindowPos(g_osd, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    SetTimer(g_osd, OSD_TIMER, 1000, NULL);
}

static void toggle_osd(void)
{
    if (IsWindowVisible(g_osd))
        hide_osd();
    else
        show_osd();
}

static LRESULT CALLBACK osd_window_proc(HWND hwnd, UINT message,
                                        WPARAM wparam, LPARAM lparam)
{
    switch (message)
    {
        case WM_ERASEBKGND:
            return 1;

        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;

        case WM_MOUSEMOVE:
        {
            POINT point = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
            int hover = hit_button(point);
            TRACKMOUSEEVENT track;

            if (hover != g_hover)
            {
                g_hover = hover;
                InvalidateRect(hwnd, NULL, FALSE);
            }

            memset(&track, 0, sizeof(track));
            track.cbSize = sizeof(track);
            track.dwFlags = TME_LEAVE;
            track.hwndTrack = hwnd;
            TrackMouseEvent(&track);
            return 0;
        }

        case WM_MOUSELEAVE:
            if (g_hover)
            {
                g_hover = 0;
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;

        case WM_PAINT:
            draw_osd(hwnd);
            return 0;

        case WM_TIMER:
            if (wparam == OSD_TIMER)
            {
                if (g_hide_seconds > 0)
                    --g_hide_seconds;
                if (g_hide_seconds == 0)
                    hide_osd();
                else
                    InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;

        case WM_MENU_REQUEST:
            request_main_menu();
            return 0;

        case WM_HOTKEY:
            if (wparam == HOTKEY_OSD)
            {
                toggle_osd();
                return 0;
            }

            if (!g_game || !IsWindow(g_game))
                g_game = find_game_window(&g_game_pid);

            if (wparam == HOTKEY_MENU)
            {
                request_main_menu();
                return 0;
            }

            if (wparam == HOTKEY_TOGGLE && g_game)
            {
                toggle_fullscreen(g_game);
                show_osd();
            }
            else if (wparam == HOTKEY_QUIT && g_game)
            {
                quit_game(g_game, g_game_pid);
            }
            return 0;

        case WM_LBUTTONUP:
        {
            POINT point = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
            int button = hit_button(point);

            if (!g_game || !IsWindow(g_game))
                g_game = find_game_window(&g_game_pid);

            if (button == 1)
            {
                if (g_game)
                {
                    toggle_fullscreen(g_game);
                    show_osd();
                }
            }
            else if (button == 2)
            {
                g_large_window = !g_large_window;
                write_window_mode();
                snprintf(g_status, sizeof(g_status), "Restarting...");
                InvalidateRect(hwnd, NULL, FALSE);
                show_osd();
                if (g_game)
                    quit_game(g_game, g_game_pid);
            }
            else if (button == 3)
            {
                g_language_hu = !g_language_hu;
                write_language_files();
                snprintf(g_status, sizeof(g_status), "Restarting...");
                InvalidateRect(hwnd, NULL, FALSE);
                show_osd();
                if (g_game)
                    quit_game(g_game, g_game_pid);
            }
            else if (button == 4)
            {
                if (g_game)
                    quit_game(g_game, g_game_pid);
            }
            return 0;
        }

        default:
            break;
    }

    return DefWindowProcA(hwnd, message, wparam, lparam);
}

static int create_osd(HINSTANCE instance)
{
    WNDCLASSEXA window_class;
    HINSTANCE module = instance ? instance : GetModuleHandleA(NULL);

    memset(&window_class, 0, sizeof(window_class));
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = osd_window_proc;
    window_class.hInstance = module;
    window_class.hCursor = LoadCursor(NULL, IDC_ARROW);
    window_class.lpszClassName = OSD_CLASS;

    if (!RegisterClassExA(&window_class) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        return 0;

    g_osd = CreateWindowExA(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE |
                            WS_EX_LAYERED,
                            OSD_CLASS, "Sacked OSD", WS_POPUP,
                            0, 0, OSD_WIDTH, OSD_HEIGHT,
                            NULL, NULL, module, NULL);
    if (!g_osd)
        return 0;

    SetLayeredWindowAttributes(g_osd, 0, OSD_ALPHA, LWA_ALPHA);
    return 1;
}

static void register_hotkeys(void)
{
    if (!RegisterHotKey(g_osd, HOTKEY_TOGGLE, MOD_CONTROL | MOD_ALT, 'F'))
        log_msg("failed to register Control+Command+F (error %lu)", GetLastError());
    else
        log_msg("registered Control+Command+F: toggle fullscreen");

    if (!RegisterHotKey(g_osd, HOTKEY_QUIT, MOD_CONTROL | MOD_ALT, 'Q'))
        log_msg("failed to register Control+Command+Q (error %lu)", GetLastError());
    else
        log_msg("registered Control+Command+Q: quit");

    if (!RegisterHotKey(g_osd, HOTKEY_OSD, MOD_CONTROL | MOD_ALT, 'H'))
        log_msg("failed to register Control+Command+H (error %lu)", GetLastError());
    else
        log_msg("registered Control+Command+H: show/hide OSD");

    if (!RegisterHotKey(g_osd, HOTKEY_MENU, 0, VK_ESCAPE))
        log_msg("failed to register Esc (error %lu)", GetLastError());
    else
        log_msg("registered Esc: return to main menu");
}

static char *trim_command(char *text)
{
    char *end;

    while (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n')
        ++text;

    end = text + strlen(text);
    while (end > text && (end[-1] == ' ' || end[-1] == '\t' ||
                          end[-1] == '\r' || end[-1] == '\n'))
        *--end = '\0';

    return text;
}

static HWND wait_for_game(DWORD *pid, DWORD timeout)
{
    DWORD start = GetTickCount();
    HWND game;

    for (;;)
    {
        game = find_game_window(pid);
        if (game)
            return game;
        if (GetTickCount() - start > timeout)
            return NULL;
        Sleep(100);
    }
}

static int run_cli_action(const char *action)
{
    DWORD pid = 0;
    HWND game = wait_for_game(&pid, 5000);

    if (!game)
    {
        log_msg("action %s: game window not found", action);
        return 1;
    }

    if (!_stricmp(action, "toggle"))
    {
        toggle_fullscreen(game);
        Sleep(300);
        return 0;
    }

    if (!_stricmp(action, "quit"))
    {
        quit_game(game, pid);
        Sleep(300);
        return 0;
    }

    if (!_stricmp(action, "menu"))
    {
        int ok = install_menu_hook(game, pid) && send_menu_request(game);
        Sleep(300);
        return ok ? 0 : 1;
    }

    log_msg("unknown action: %s", action);
    return 1;
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR command_line, int show)
{
    DWORD start_tick;
    DWORD lost_tick = 0;
    BOOL had_game = FALSE;
    BOOL osd_started = FALSE;
    MSG message;

    (void)previous;
    (void)show;

    log_init();
    log_msg("start");
    log_msg("helper build: game-thread menu hook");

    command_line = trim_command(command_line);
    if (*command_line)
    {
        log_msg("cli action: %s", command_line);
        return run_cli_action(command_line);
    }

    read_language();
    read_window_mode();
    snprintf(g_status, sizeof(g_status), "In-game settings");

    if (!create_osd(instance))
    {
        log_msg("could not create OSD window (%lu)", GetLastError());
        return 1;
    }

    register_hotkeys();
    g_keyboard_hook = SetWindowsHookExA(WH_KEYBOARD_LL, low_level_keyboard_proc,
                                        instance, 0);
    if (!g_keyboard_hook)
        log_msg("failed to install low-level keyboard hook (error %lu)",
                GetLastError());
    else
        log_msg("installed low-level keyboard hook for Esc");
    start_tick = GetTickCount();

    for (;;)
    {
        while (PeekMessageA(&message, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&message);
            DispatchMessageA(&message);
        }

        if (!g_game || !IsWindow(g_game))
        {
            if (g_game)
            {
                log_msg("game window disappeared");
                remove_menu_hook();
                g_game = NULL;
                g_game_pid = 0;
                g_relock_left = 0;
                g_was_fullscreen = 0;
                lost_tick = GetTickCount();
                hide_osd();
            }

            g_game = find_game_window(&g_game_pid);
            if (g_game)
            {
                had_game = TRUE;
                lost_tick = 0;
                log_msg("game window found (pid %lu)", g_game_pid);
                log_window_state("game window found");
                if (!osd_started)
                {
                    show_osd();
                    osd_started = TRUE;
                }
                install_menu_hook(g_game, g_game_pid);
            }
            else if (!had_game && GetTickCount() - start_tick > 120000)
            {
                log_msg("no game window after 120 seconds; exiting");
                break;
            }
            else if (had_game && lost_tick && GetTickCount() - lost_tick > 3000)
            {
                log_msg("game exited");
                break;
            }
        }
        else if (IsWindowVisible(g_osd))
        {
            position_osd();
        }

        {
            int fullscreen = is_fullscreen_window();
            if (fullscreen && !g_was_fullscreen)
            {
                log_msg("fullscreen detected");
                schedule_cursor_relock();
            }
            g_was_fullscreen = fullscreen;
        }

        run_cursor_relock();

        {
            static int escape_down;
            int down = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;

            if (down && !escape_down)
                request_main_menu();
            escape_down = down;
        }

        Sleep(50);
    }

    remove_menu_hook();
    hide_osd();
    if (g_keyboard_hook)
    {
        UnhookWindowsHookEx(g_keyboard_hook);
        g_keyboard_hook = NULL;
    }
    UnregisterHotKey(g_osd, HOTKEY_TOGGLE);
    UnregisterHotKey(g_osd, HOTKEY_QUIT);
    UnregisterHotKey(g_osd, HOTKEY_OSD);
    UnregisterHotKey(g_osd, HOTKEY_MENU);
    DestroyWindow(g_osd);
    log_msg("exit");
    return 0;
}
