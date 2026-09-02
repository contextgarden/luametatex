/*
    See license.txt in the root of this project.
*/

/*
    This is work in progress. We already have some parallel processing features but these use the
    regular \LUA\ popen feature. We don't want to use some library that adds too much to the
    binary so after asking around a bit for the basics we ended up here. It kind of set up like
    the serial module, with a userdatum and such.

    The interfaces should be the same for windows and posix and because posix wants the arguments
    split form the command, that is what gets in. We also want to be abel to discard the output
    because \TEX\ also produces a log file. The reason for thisa module is that I want to process
    the tets suite faster. The (in 2026) about 2200 files took 800 seconds on my 2018 laptop with
    the regular |popen| feature but some 450 with the non-blocking approach.

*/

/*
local process_1 = process.open("context",{ "oeps-1.tex" })
local process_2 = process.open("context",{ "oeps-2.tex" })

local active = {
    [process_1] = true,
    [process_2] = true,
}

local c = os.clock()
while true do
    local list = { }
    for k, v in next, active do
        if v then
            list[#list+1] = k
        end
    end
    if #list > 0 then
        local ready = process.poll(list, 1000)
        for i=1,#ready do
            local p = ready[i]
            local chunk, eof = process.read(p)
            if chunk then
             -- print((string.gsub(chunk,"[\n]","")))
                print("busy ", i)
            elseif eof then
                local exitcode = process.close(p)
                print("exit ", i, exitcode)
                active[p] = false
            end
        end
    else
        break
    end
end
print(os.clock()-c)

*/

# include "luametatex.h"

/*tex See |lmtinterface.h| for |PROCESS_METATABLE_INSTANCE|. */

# ifdef _WIN32

    # include <windows.h>

    typedef struct process_data {
        HANDLE read;
        HANDLE process;
        int    closed;
        int    nulled;
    } process_data;

# else

    # include <unistd.h>
    # include <fcntl.h>
    # include <poll.h>
    # include <sys/wait.h>
    # include <errno.h>

    typedef pid_t ProcID;

    typedef struct process_data {
        int   read;
        pid_t process;
        int   closed;
        int   nulled;
    } process_data;

# endif

/*
    We can share this one, because it doesn't set any values in the data structure.
*/

static inline process_data * processlib_aux_valid(lua_State *L, int i)
{
    process_data * process = lua_touserdata(L, i);
    if (process && lua_getmetatable(L, i)) {
        lua_get_metatablelua(process_instance);
        if (! lua_rawequal(L, -1, -2)) {
            process = NULL;
        }
        lua_pop(L, 2);
    }
    return process;
}

// open(command, { argument, argument, ... }, nulled) : process

# ifdef _WIN32

    // nice example: https://learn.microsoft.com/en-us/windows/win32/procthread/creating-a-child-process-with-redirected-input-and-output

    static int processlib_open(lua_State *L)
    {
        const char *cmd       = luaL_checkstring(L, 1);
        int         arguments = lua_type(L, 2) == LUA_TTABLE ? (int) lua_rawlen(L, 2) : 0;
        int         nulled    = lua_toboolean(L, 3);

        if (arguments) {
            luaL_Buffer b;
            luaL_buffinit(L, &b);
            luaL_addstring(&b, cmd);
            for (int i = 1; i <= arguments; i++) {
                luaL_addstring(&b, " ");
                lua_rawgeti(L, 2, i);
                luaL_addstring(&b, luaL_checkstring(L, -1));
                lua_pop(L, 1);
            }
            luaL_pushresult(&b);
        } else {
            /* We have to make sure that the string is at slot -1 in case of no arguments. */
            lua_pushvalue(L, 1);
        }

        HANDLE hRead  = NULL;
        HANDLE hWrite = NULL;
        HANDLE hNull  = NULL;

        SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };

        if (nulled) {
            hNull = CreateFileA(
                "NUL",
                GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                &sa,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                NULL
            );
            if (hNull == INVALID_HANDLE_VALUE) {
                tex_formatted_warning("process lib", "opening nul device failed");
                return 0;
            }
        } else {
            if (! CreatePipe(&hRead, &hWrite, &sa, 0)) {
                tex_formatted_warning("process lib", "creating pipe failed");
                return 0;
            }
            SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);
        }

        STARTUPINFOA        si = { 0 };
        PROCESS_INFORMATION pi = { 0 };
        si.cb          = sizeof(STARTUPINFOA);
        si.hStdOutput  = nulled ? hNull : hWrite;
        si.hStdError   = nulled ? hNull : hWrite;
        si.dwFlags    |= STARTF_USESTDHANDLES;

        size_t len;
        const char *cmd_constant = lua_tolstring(L, -1, &len);
        char       *cmd_mutable  = lmt_memory_malloc(len + 1);

        if (! cmd_mutable) {
            CloseHandle(hRead);
            CloseHandle(hWrite);
            tex_formatted_error("process lib", "out of memory");
            return 0;
        } else {
            memcpy(cmd_mutable, cmd_constant, len + 1);
        }

        BOOL result = CreateProcessA(NULL, cmd_mutable, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi);

        lmt_memory_free(cmd_mutable);

        if (! result) {
            if (hRead)  CloseHandle(hRead);
            if (hWrite) CloseHandle(hWrite);
            if (hNull)  CloseHandle(hNull);
            tex_formatted_warning("process lib", "creating process failed");
            return 0;
        }

        if (hWrite)     CloseHandle(hWrite);
        if (hNull)      CloseHandle(hNull);
        if (pi.hThread) CloseHandle(pi.hThread);

        process_data *process = lua_newuserdatauv(L, sizeof(process_data), 0);
        lua_get_metatablelua(process_instance);
        lua_setmetatable(L, -2);
        process->read    = hRead;
        process->process = pi.hProcess;
        process->closed  = 0;
        process->nulled  = nulled;
        return 1;
    }

# else

    static int processlib_open(lua_State *L)
    {
        const char  *cmd    = luaL_checkstring(L, 1);
        int          argc   = (lua_type(L, 2) == LUA_TTABLE) ? (int) lua_rawlen(L, 2) : 0;
        int          nulled = lua_toboolean(L, 3);
        char       **argv   = lmt_memory_malloc(sizeof(char*) * (argc + 2));
        if (! argv) {
            tex_formatted_error("process lib", "out of memory");
            return 0;
        }
        argv[0] = lmt_memory_strdup(cmd);
        for (int i = 1; i <= argc; i++) {
            lua_rawgeti(L, 2, i);
            argv[i] = lmt_memory_strdup(luaL_checkstring(L, -1));
            lua_pop(L, 1);
        }
        argv[argc + 1] = NULL;

        int pipefd[2] = { -1, -1 };
        if (! nulled) {
            if (pipe(pipefd) == -1) {
                for (int i = 0; i <= argc; i++) lmt_memory_free(argv[i]);
                lmt_memory_free(argv);
                tex_formatted_warning("process lib", "creating pipe failed");
                return 0;
            }
        }

        pid_t pid = fork();
        if (pid == -1) {
            /* fork failed */
            if (! nulled) {
                close(pipefd[0]);
                close(pipefd[1]);
            }
            for (int i = 0; i <= argc; i++) lmt_memory_free(argv[i]);
            lmt_memory_free(argv);
            tex_formatted_warning("process lib", "fork failed");
            return 0;
        }

        /* child process */
        if (pid == 0) {
            if (nulled) {
                int devnull = open("/dev/null", O_WRONLY);
                if (devnull != -1) {
                    dup2(devnull, STDOUT_FILENO);
                    dup2(devnull, STDERR_FILENO);
                    close(devnull);
                }
            } else {
                close(pipefd[0]);                /* close unused read end */
                dup2(pipefd[1], STDOUT_FILENO);  /* redirect standard output */
                dup2(pipefd[1], STDERR_FILENO);  /* redirect standard error */
                close(pipefd[1]);                /* close duplicate handle */
            }
            execvp(cmd, argv);                   /* replace process image */

            /* cleanup on execvp error */
            for (int i = 0; i <= argc; i++) {
                lmt_memory_free(argv[i]);
            }
            lmt_memory_free(argv);
            _exit(127);
        }

        /* parent process cleanup */
        for (int i = 0; i <= argc; i++) {
            lmt_memory_free(argv[i]);
        }
        lmt_memory_free(argv);

        if (! nulled) {
            close(pipefd[1]);
            /* set non-blocking mode */
            int flags = fcntl(pipefd[0], F_GETFL, 0);
            fcntl(pipefd[0], F_SETFL, flags | O_NONBLOCK);
        }

        /* wrap state in userdata */
        process_data *process = lua_newuserdatauv(L, sizeof(process_data), 0);
        lua_get_metatablelua(process_instance);
        lua_setmetatable(L, -2);
        process->read    = nulled ? -1 : pipefd[0];
        process->process = pid;
        process->closed  = 0;
        process->nulled  = nulled;
        return 1;
    }

# endif

// read(process) -> string | nil, eof

# define buffersize 1024

# ifdef _WIN32

    static int processlib_read(lua_State *L)
    {
        process_data *process = processlib_aux_valid(L, 1);
        if (! process || process->closed || process->nulled) {
            lua_pushnil(L);
            lua_pushboolean(L, 1);
            return 2;
        }
        char  buf[buffersize];
        DWORD avail   = 0;
        DWORD bytes   = 0;
        BOOL  peek_ok = PeekNamedPipe(process->read, NULL, 0, NULL, &avail, NULL);
        if (! peek_ok) {
            DWORD err = GetLastError();
            if (err == ERROR_BROKEN_PIPE || err == ERROR_PIPE_NOT_CONNECTED) {
                lua_pushnil(L);
                lua_pushboolean(L, 1);
                return 2;
            }
            lua_pushnil(L);
            return 1;
        }
        if (avail > 0) {
            if (ReadFile(process->read, buf, sizeof(buf), &bytes, NULL) && bytes > 0) {
                lua_pushlstring(L, buf, bytes);
                return 1;
            }
        }
        /* Check exit status ONLY if there was no readable data */
        DWORD exit_code;
        if (GetExitCodeProcess(process->process, &exit_code) && exit_code != STILL_ACTIVE) {
            /* Double-check avail in case data arrived right before process termination */
            PeekNamedPipe(process->read, NULL, 0, NULL, &avail, NULL);
            if (avail == 0) {
                lua_pushnil(L);
                lua_pushboolean(L, 1);
                return 2;
            }
        }
        lua_pushnil(L);
        return 1;
    }

# else

    static int processlib_read(lua_State *L)
    {
        process_data *process = processlib_aux_valid(L, 1);
        if (! process || process->closed || process->nulled || process->read < 0) {
            lua_pushnil(L);
            lua_pushboolean(L, 1); /* return EOF directly */
            return 2;
        }
        char    buf[buffersize];
        ssize_t n;
        do {
            n = read(process->read, buf, sizeof(buf));
        } while (n == -1 && errno == EINTR);
        if (n > 0) {
            lua_pushlstring(L, buf, n);
            return 1;
        } else if (n == 0) {
            /* EOF reached: child closed write end of pipe */
            lua_pushnil(L);
            lua_pushboolean(L, 1);
            return 2;
        } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
            /* No data available right now */
            lua_pushnil(L);
            return 1;
        } else {
            /* read error */
            lua_pushnil(L);
            lua_pushboolean(L, 1);
            return 2;
        }
    }

# endif

// poll({ process, process, ... }, timeout) : ready_table

# ifdef _WIN32

    static int processlib_poll(lua_State *L)
    {
        luaL_checktype(L, 1, LUA_TTABLE);
        ULONGLONG timeout = (ULONGLONG) luaL_optinteger(L, 2, -1);
        ULONGLONG start   = GetTickCount64();
        int       count   = (int) lua_rawlen(L, 1);
        int       index   = 1;
        lua_newtable(L);
        while (1) {
            int ready = 0;
            for (int i = 1; i <= count; i++) {
                lua_rawgeti(L, 1, i);
                process_data *process = processlib_aux_valid(L, -1);
                lua_pop(L, 1);
                if (process && ! process->closed) {
                    if (process->nulled) {
                        /* Nulled processes have no pipe; check if the process terminated */
                        if (WaitForSingleObject(process->process, 0) == WAIT_OBJECT_0) {
                            lua_pushinteger(L, i);
                            lua_rawseti(L, -2, index++);
                            ready++;
                        }
                        continue;
                    }
                    DWORD avail   = 0;
                    BOOL  peek_ok = PeekNamedPipe(process->read, NULL, 0, NULL, &avail, NULL);
                    /* Pipe has data to read */
                    if (peek_ok && avail > 0) {
                        lua_pushinteger(L, i);
                        lua_rawseti(L, -2, index++);
                        ready++;
                        continue;
                    }
                    /* Check if pipe is broken (EOF) or process has terminated */
                    DWORD err    = GetLastError();
                    BOOL  broken = ! peek_ok && (err == ERROR_BROKEN_PIPE || err == ERROR_PIPE_NOT_CONNECTED);
                    BOOL  dead   = (WaitForSingleObject(process->process, 0) == WAIT_OBJECT_0);
                    if (broken || dead) {
                        /* Mark as ready so caller knows to read (which will hit EOF) or close */
                        lua_pushinteger(L, i);
                        lua_rawseti(L, -2, index++);
                        ready++;
                    }
                }
            }
            if (ready > 0 || timeout == 0) {
                break;
            }
            if (timeout != (ULONGLONG)-1 && (GetTickCount64() - start) >= timeout) {
                break;
            }
            Sleep(5); // 5 ms yield
        }
        return 1;
    }

# else

    static int processlib_poll(lua_State *L)
    {
        luaL_checktype(L, 1, LUA_TTABLE);
        int timeout = (int) luaL_optinteger(L, 2, -1);
        int count   = (int) lua_rawlen(L, 1);
        int index   = 1;
        int ready   = 0;

        lua_newtable(L);
        if (count == 0) {
            return 1;
        }

        struct pollfd *fds = lmt_memory_malloc(sizeof(struct pollfd) * count);
        if (! fds) {
            tex_formatted_error("process lib", "out of memory");
            return 0;
        }

        for (int i = 1; i <= count; i++) {
            lua_rawgeti(L, 1, i);
            process_data *process = processlib_aux_valid(L, -1);
            lua_pop(L, 1);
            if (process && ! process->closed) {
                if (process->nulled || process->read < 0) {
                    /* Nulled process: check process exit state directly */
                    int status;
                    pid_t res = waitpid(process->process, &status, WNOHANG);
                    if (res != 0) { /* Process terminated or error */
                        lua_pushinteger(L, i);
                        lua_rawseti(L, -2, index++);
                    }
                } else {
                    fds[ready].fd     = process->read;
                    fds[ready].events = POLLIN | POLLHUP | POLLERR;
                    ready++;
                }
            }
        }

        if (ready == 0) {
            lmt_memory_free(fds);
            return 1;
        }

        int ret;
        do {
            ret = poll(fds, ready, timeout);
        } while (ret == -1 && errno == EINTR);

        if (ret > 0) {
            int fd_idx = 0;
            for (int i = 1; i <= count; i++) {
                lua_rawgeti(L, 1, i);
                process_data *process = processlib_aux_valid(L, -1);
                lua_pop(L, 1);
                if (process && ! process->closed && ! process->nulled && process->read >= 0) {
                    /* Check if readable, hung up (EOF), or errored */
                    if (fds[fd_idx].revents & (POLLIN | POLLHUP | POLLERR)) {
                        lua_pushinteger(L, i);
                        lua_rawseti(L, -2, index++);
                    }
                    fd_idx++;
                }
            }
        }
        lmt_memory_free(fds);
        return 1;
    }

# endif
// close(process) : exit_code

# ifdef _WIN32

    static int processlib_close(lua_State *L)
    {
        process_data *process = processlib_aux_valid(L, 1);
        if (process && ! process->closed) {
            DWORD exitcode = 0;
            CloseHandle(process->read);
            WaitForSingleObject(process->process, INFINITE);
            GetExitCodeProcess(process->process, &exitcode);
            CloseHandle(process->process);
            lua_pushinteger(L, exitcode);
            process->closed = 1;
        } else {
            lua_pushnil(L);
        }
        return 1;
    }

# else

    static int processlib_close(lua_State *L)
    {
        process_data *process = processlib_aux_valid(L, 1);
        if (process && ! process->closed) {
            int   status;
            pid_t p;
            if (process->read >= 0) {
                close(process->read);
                process->read = -1;
            }
            process->closed = 1;
            do {
                p = waitpid(process->process, &status, 0);
            } while (p == -1 && errno == EINTR);
            if (p > 0 && WIFEXITED(status)) {
                lua_pushinteger(L, WEXITSTATUS(status));
            } else if (p > 0 && WIFSIGNALED(status)) {
                lua_pushinteger(L, -WTERMSIG(status));
            } else {
                lua_pushinteger(L, -1);
            }
        } else {
            lua_pushnil(L);
        }
        return 1;
    }

# endif
static int processlib_tostring(lua_State *L)
{
    process_data * process = processlib_aux_valid(L, 1);
    if (process) { // && ! process->closed) {
        lua_pushfstring(L, "<process %p>", process);
        return 1;
    } else {
        return 0;
    }
}

static int processlib_gc(lua_State *L)
{
    return processlib_close(L);
}

static const struct luaL_Reg processlib_function_list[] = {
    /* management */
    { "open",     processlib_open     },
    { "read",     processlib_read     },
    { "poll",     processlib_poll     },
    { "close",    processlib_close    },
    /* */
    { "tostring", processlib_tostring },
    /* */
    { "__gc",     processlib_gc       },
    /* */
    { NULL,       NULL                },
};

int luaopen_process(lua_State *L)
{
    luaL_newmetatable(L, PROCESS_METATABLE_INSTANCE);
    luaL_setfuncs(L, processlib_function_list, 0);
    lua_pushliteral(L, "__index");
    lua_pushvalue(L, -2);
    lua_settable(L, -3);
    lua_pushliteral(L, "__tostring");
    lua_pushliteral(L, "tostring");
    lua_gettable(L, -3);
    lua_settable(L, -3);
    lua_pushliteral(L, "__name");
    lua_pushliteral(L, "process");
    lua_settable(L, -3);
    return 1;
}
