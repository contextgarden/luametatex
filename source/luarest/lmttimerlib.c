/*
    See license.txt in the root of this project.
*/

/*tex

    In |lmtlualib| we introduced more precise timer support but there we stored the data in \LUA's
    64 bit integers. Here we use userdata. Again we delay division; see the other code for similar
    trickery. I will test this and when it's not more convenient (or used) we will not use it.
*/

# include <luametatex.h>
# include <stdint.h>
# include <stdbool.h>

typedef struct timer {
    uint64_t total;
    uint64_t initial;
    bool     running;
} timer;

# if defined(_WIN32) || defined(_WIN64)

    # include <windows.h>

    inline static uint64_t get_ticks(void)
    {
        LARGE_INTEGER counter;
        QueryPerformanceCounter(&counter);
        return (uint64_t) counter.QuadPart;
    }

    inline static double ticks_to_ms(uint64_t ticks)
    {
        LARGE_INTEGER freq;
        QueryPerformanceFrequency(&freq);
        return ((double) ticks * 1000.0) / (double) freq.QuadPart;
    }

    inline static double ticks_to_us(uint64_t ticks)
    {
        LARGE_INTEGER freq;
        QueryPerformanceFrequency(&freq);
        return ((double) ticks * 1000000.0) / (double) freq.QuadPart;
    }

# elif defined(__APPLE__)

    # include <mach/mach_time.h>

    inline static uint64_t get_ticks(void)
    {
        return mach_absolute_time();
    }

    inline static double ticks_to_ms(uint64_t ticks)
    {
        static mach_timebase_info_data_t timebase;
        if (timebase.denom == 0) {
            mach_timebase_info(&timebase);
        }
        return ((double) ticks * timebase.numer / timebase.denom) / 1000000.0;
    }

    inline static double ticks_to_us(uint64_t ticks)
    {
        static mach_timebase_info_data_t timebase;
        if (timebase.denom == 0) {
            mach_timebase_info(&timebase);
        }
        return ((double) ticks * timebase.numer / timebase.denom) / 1000.0;
    }

# else /* POSIX */

    # include <time.h>

    inline static uint64_t get_ticks(void)
    {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return ((uint64_t) ts.tv_sec * 1000000000ULL) + (uint64_t) ts.tv_nsec;
    }

    inline static double ticks_to_ms(uint64_t ticks)
    {
        return (double) ticks / 1000000.0;
    }

    inline static double ticks_to_us(uint64_t ticks)
    {
        return (double) ticks / 1000.0;
    }

# endif

static inline timer * timerlib_aux_valid(lua_State *L, int i)
{
    timer * t = lua_touserdata(L, i);
    if (t && lua_getmetatable(L, i)) {
        lua_get_metatablelua(timer_instance);
        if (! lua_rawequal(L, -1, -2)) {
            t = NULL;
        }
        lua_pop(L, 2);
    }
    return t;
}

static int timerlib_new(lua_State *L)
{
    timer *t = lua_newuserdatauv(L, sizeof(timer), 0);
    if (t) {
        lua_get_metatablelua(timer_instance);
        lua_setmetatable(L, -2);
        t->total   = 0;
        t->initial = 0;
        t->running = false;
    } else {
        tex_formatted_error("process lib", "out of memory");
        lua_pushnil(L); /* never seen as we abort */
    }
    return 1;
}

static int timerlib_start(lua_State *L)
{
    timer *t = timerlib_aux_valid(L, 1);
    if (t && ! t->running) {
        t->initial = get_ticks();
        t->running = true;
    }
    return 0;
}

static int timerlib_stop(lua_State *L)
{
    timer *t = timerlib_aux_valid(L, 1);
    if (t && t->running) {
        uint64_t now = get_ticks();
        t->total += (now - t->initial);
        t->running = false;
    }
    return 0;
}

static int timerlib_reset(lua_State *L)
{
    timer *t = timerlib_aux_valid(L, 1);
    if (t) {
        t->total   = 0;
        t->initial = 0;
        t->running = false;
    }
    return 0;
}

static int timerlib_elapsed(lua_State *L)
{
    timer *t = timerlib_aux_valid(L, 1);
    if (t) {
        uint64_t total = t->total;
        if (t->running) {
            total += (get_ticks() - t->initial);
        }
        lua_pushnumber(L, lua_toboolean(L, 2) ? ticks_to_us(total) : ticks_to_ms(total));
    } else {
        lua_pushnil(L);
    }
    return 1;
}

static int timerlib_tostring(lua_State *L)
{
    timer *t = timerlib_aux_valid(L, 1);
    if (t) {
        lua_pushfstring(L, "<timer %p>", t);
        return 1;
    } else {
        return 0;
    }
}

/*tex
    The normal sleep function rounds and/or snap so on windows we only have some 15 ms
    accuracy. Each operating system has its interferences unless one goes for the more
    high resolution timers.
*/

# if defined(_WIN32) || defined(_WIN64)

    # include <windows.h>

    inline static void timerlib_aux_sleep(uint32_t ms)
    {
        HANDLE hTimer = CreateWaitableTimerExW(
            NULL,
            NULL,
            TIMER_ALL_ACCESS | CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
            TIMER_ALL_ACCESS
        );
        if (hTimer) {
            /* a negative number is a relative time in 100-ns intervals */
            LARGE_INTEGER liDueTime;
            liDueTime.QuadPart = -(int64_t) ms * 10000LL;
            if (SetWaitableTimerEx(hTimer, &liDueTime, 0, NULL, NULL, NULL, 0)) {
                WaitForSingleObject(hTimer, INFINITE);
            } else {
                Sleep(ms);
            }
            CloseHandle(hTimer);
        } else {
            /* older windows, not that we use them */
            Sleep(ms);
        }
    }

# elif defined(__linux__)

    # include <time.h>
    # include <sys/prctl.h>

    inline static void timerlib_aux_sleep(uint32_t ms)
    {
        # ifdef PR_SET_TIMERSLACK
            prctl(PR_SET_TIMERSLACK, 1UL, 0, 0, 0); /* no auto-rounding */
        # endif
        struct timespec ts;
        ts.tv_sec  = ms / 1000;
        ts.tv_nsec = (ms % 1000) * 1000000UL;
        clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, NULL); /* avoid NTP interference */
    }

# elif defined(__APPLE__)

    # include <time.h>

    inline static void timerlib_aux_sleep(uint32_t ms)
    {
        struct timespec ts;
        ts.tv_sec  = ms / 1000;
        ts.tv_nsec = (ms % 1000) * 1000000UL;
        nanosleep(&ts, NULL);
    }

# else /* POSIX */

    # include <unistd.h>

    inline static void timerlib_aux_sleep(uint32_t ms)
    {
        usleep(ms * 1000);
    }

# endif

static int timerlib_sleepms(lua_State *L)
{
    timerlib_aux_sleep((uint32_t) lua_tointeger(L, 1));
    return 0;
}

static const struct luaL_Reg timerlib_function_list[] = {
    /* management */
    { "new",      timerlib_new      },
    { "start",    timerlib_start    },
    { "stop",     timerlib_stop     },
    { "reset",    timerlib_reset    },
    { "elapsed",  timerlib_elapsed  },
    { "tostring", timerlib_tostring },
    /* */
    { "sleepms",  timerlib_sleepms  },
    /* */
    { NULL,       NULL              },
};

int luaopen_timer(lua_State *L)
{
    luaL_newmetatable(L, TIMER_METATABLE_INSTANCE);
    luaL_setfuncs(L, timerlib_function_list, 0);
    lua_pushliteral(L, "__index");
    lua_pushvalue(L, -2);
    lua_settable(L, -3);
    lua_pushliteral(L, "__tostring");
    lua_pushliteral(L, "tostring");
    lua_gettable(L, -3);
    lua_settable(L, -3);
    lua_pushliteral(L, "__name");
    lua_pushliteral(L, "timer");
    lua_settable(L, -3);
    return 1;
}