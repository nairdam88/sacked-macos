#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0501

#include <windows.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define WM_SACKED_MENU (WM_APP + 120)

#define GAME_CGAME_RVA 0x0007a948u
#define GAME_SET_STATE_RVA 0x000079b0u
#define GAME_STATE_RVA 0x000049dcu
#define GAME_STATE_MAIN_MENU 3

#define MENU_READY_FORMAT "Local\\SackedMenuReady_%lu"
#define MENU_DONE_FORMAT "Local\\SackedMenuDone_%lu"

static HINSTANCE g_module;
static HWND g_game_window;
static WNDPROC g_old_window_proc;
static HANDLE g_ready_event;
static HANDLE g_done_event;
static volatile LONG g_busy;

void call_set_state(void *self, void *fn, int state);

static void menu_log(const char *fmt, ...)
{
    char path[MAX_PATH];
    char buf[512];
    char *slash;
    va_list ap;
    DWORD written;
    HANDLE file;

    if (!g_module || !GetModuleFileNameA(g_module, path, sizeof(path)))
        return;

    slash = strrchr(path, '\\');
    if (!slash)
        return;
    strcpy(slash + 1, "sacked-menu.log");

    file = CreateFileA(path, FILE_APPEND_DATA,
                       FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                       OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE)
        return;

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf) - 2, fmt, ap);
    va_end(ap);
    strncat(buf, "\r\n", sizeof(buf) - strlen(buf) - 1);
    WriteFile(file, buf, (DWORD)strlen(buf), &written, NULL);
    CloseHandle(file);
}

static HANDLE create_named_event(const char *format)
{
    char name[64];

    snprintf(name, sizeof(name), format, (unsigned long)GetCurrentProcessId());
    return CreateEventA(NULL, TRUE, FALSE, name);
}

static void signal_done(void)
{
    if (g_done_event)
        SetEvent(g_done_event);
}

static void invoke_main_menu(void)
{
    HMODULE base = GetModuleHandleA(NULL);
    unsigned char *cgame;
    unsigned char *set_state;
    DWORD state;

    if (!base)
    {
        menu_log("could not get game module base");
        signal_done();
        return;
    }

    cgame = (unsigned char *)base + GAME_CGAME_RVA;
    set_state = (unsigned char *)base + GAME_SET_STATE_RVA;
    state = *(volatile DWORD *)(cgame + GAME_STATE_RVA);
    menu_log("menu: CGame=%p SetState=%p current state=%lu",
             cgame, set_state, (unsigned long)state);

    if (state == GAME_STATE_MAIN_MENU)
    {
        menu_log("menu: already at the main menu");
        signal_done();
        return;
    }

    menu_log("menu: calling SetState(%d) on the game thread",
             GAME_STATE_MAIN_MENU);
    call_set_state(cgame, set_state, GAME_STATE_MAIN_MENU);
    state = *(volatile DWORD *)(cgame + GAME_STATE_RVA);
    menu_log("menu: SetState returned, state=%lu", (unsigned long)state);
    signal_done();
}

static LRESULT CALLBACK menu_window_proc(HWND hwnd, UINT message,
                                         WPARAM wparam, LPARAM lparam)
{
    if (message == WM_SACKED_MENU)
    {
        if (InterlockedCompareExchange(&g_busy, 1, 0) == 0)
        {
            invoke_main_menu();
            InterlockedExchange(&g_busy, 0);
        }
        else
        {
            menu_log("menu: ignored reentrant request");
        }
        return 0;
    }

    return CallWindowProcA(g_old_window_proc, hwnd, message, wparam, lparam);
}

typedef struct
{
    DWORD pid;
    HWND best;
    LONG area;
} find_window_context;

static BOOL CALLBACK find_window_proc(HWND hwnd, LPARAM lparam)
{
    find_window_context *ctx = (find_window_context *)lparam;
    DWORD pid = 0;
    RECT rect;
    LONG area;

    GetWindowThreadProcessId(hwnd, &pid);
    if (pid != ctx->pid || !IsWindowVisible(hwnd))
        return TRUE;

    if (!GetWindowRect(hwnd, &rect))
        return TRUE;
    area = (rect.right - rect.left) * (rect.bottom - rect.top);
    if (area < 100 * 100)
        return TRUE;

    if (!ctx->best || area > ctx->area)
    {
        ctx->best = hwnd;
        ctx->area = area;
    }

    return TRUE;
}

static HWND find_own_window(void)
{
    find_window_context ctx;

    ctx.pid = GetCurrentProcessId();
    ctx.best = NULL;
    ctx.area = 0;
    EnumWindows(find_window_proc, (LPARAM)&ctx);
    return ctx.best;
}

static DWORD WINAPI installer_thread(LPVOID unused)
{
    DWORD start = GetTickCount();
    int announced = 0;

    (void)unused;

    g_ready_event = create_named_event(MENU_READY_FORMAT);
    g_done_event = create_named_event(MENU_DONE_FORMAT);
    if (!g_ready_event)
    {
        menu_log("could not create ready event (error %lu)", GetLastError());
        return 1;
    }
    if (!g_done_event)
    {
        menu_log("could not create done event (error %lu)", GetLastError());
        return 1;
    }

    for (;;)
    {
        HWND window = find_own_window();

        if (window && window != g_game_window)
        {
            WNDPROC old_proc = (WNDPROC)SetWindowLongPtrA(
                window, GWLP_WNDPROC, (LONG_PTR)menu_window_proc);

            if (old_proc)
            {
                g_game_window = window;
                g_old_window_proc = old_proc;
                if (!announced)
                {
                    SetEvent(g_ready_event);
                    announced = 1;
                }
                menu_log("subclass installed (hwnd=%p, old proc=%p)",
                         window, old_proc);
            }
            else
            {
                menu_log("SetWindowLongPtr failed (error %lu)", GetLastError());
            }
        }

        if (!announced && GetTickCount() - start > 30000)
        {
            menu_log("no game window found for subclassing");
            return 1;
        }

        Sleep(250);
    }
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)reserved;

    if (reason == DLL_PROCESS_ATTACH)
    {
        HANDLE thread;

        g_module = instance;
        DisableThreadLibraryCalls(instance);
        thread = CreateThread(NULL, 0, installer_thread, NULL, 0, NULL);
        if (thread)
            CloseHandle(thread);
        else
            menu_log("could not start installer thread (error %lu)",
                     GetLastError());
    }

    return TRUE;
}
