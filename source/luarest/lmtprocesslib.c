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

    The rewrite to a faster process handling involved (also at the \CONTEXT) efficient support for
    handling lines (using a callback) using a (growing) buffer for pending content. Of course
    interwoven output in a visible acceptable way is something to handle at the \LUA\ end; in
    |mtx-context| we can use colors. We might add some more features here.

    The code is a bit messy because snippets come from suggestions from e.g. Gemini as well as
    searching.  We have a bit special interface wrt commands, polling, flushing but eventually we
    get there. This kind of platform specific code is not really nice to deal with.

*/

# include "luametatex.h"

/*tex See |lmtinterface.h| for |PROCESS_METATABLE_INSTANCE|. */

// open  ( command, nulled )                    : process
// poll  ( { process, process, ... }, timeout ) : ready_table
// read  ( process, [callback] )                : string (continue) | false (continue) | true (eof, exit)
// close ( process )                            : exit_code

# define buffersize 4096 // 16384

# ifdef _WIN32

    # include <windows.h>

    typedef struct process_data {
        HANDLE  read;
        HANDLE  process;
        int     closed;
        int     nulled;
        char   *partial;
        size_t  length;
        size_t  capacity;
    } process_data;

# else

    # include <unistd.h>
    # include <fcntl.h>
    # include <poll.h>
    # include <sys/wait.h>
    # include <errno.h>

    typedef struct process_data {
        int     read;
        pid_t   process;
        int     closed;
        int     nulled;
        int     exited;
        int     status;
        char   *partial;
        size_t  length;
        size_t  capacity;
    } process_data;

# endif

/*tex
    We share this one, because it doesn't set any values in the data structure.
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

/*tex
    When we enable a terminal (see \CONTEXT\ usage) it makes sense to handle lines here instead
    of splitting and appending in \LUA, so here is a helper.
*/

static void processlib_callback(
    lua_State    *L,
    process_data *process,
    const char   *data,
    size_t        len,
    int           eof
)
{
    /*tex We can have long lines, so we need to let the buffer grow on demand. */
    if (len > 0) {
        size_t needed = process->length + len;
        if (needed > process->capacity) {
            size_t capacity = process->capacity ? process->capacity * 2 : buffersize;
            if (capacity < needed) {
                capacity = needed;
            }
            char *buffer = lmt_memory_realloc(process->partial, capacity);
            if (! buffer) {
                tex_formatted_warning("process lib", "out of memory in line buffering");
                return;
            }
            process->partial  = buffer;
            process->capacity = capacity;
        }
        memcpy(process->partial + process->length, data, len);
        process->length = needed;
    }
    /*tex We need to handle both line endings. The callback is the second argument to read. */
    size_t start = 0;
    for (size_t i = 0; i < process->length; i++) {
        if (process->partial[i] == '\n') {
            size_t line_len = i - start;
            if (line_len > 0 && process->partial[start + line_len - 1] == '\r') {
                line_len--;
            }
            lua_pushvalue(L, 2);
            lua_pushlstring(L, process->partial + start, line_len);
            lua_call(L, 1, 0);
            start = i + 1;
        }
    }
    /*tex We now move the pending rest to the start. */
    if (start < process->length) {
        size_t remaining = process->length - start;
        if (eof) {
            /*tex We flush pending text when we exit (eof). */
            if (remaining > 0 && process->partial[start + remaining - 1] == '\r') {
                remaining--;
            }
            lua_pushvalue(L, 2);
            lua_pushlstring(L, process->partial + start, remaining);
            lua_call(L, 1, 0);
            process->length = 0;
        } else {
            /*tex We handle the rest next time (no blocking by waiting). */
            memmove(process->partial, process->partial + start, remaining);
            process->length = remaining;
        }
    } else {
        process->length = 0;
    }
}

# ifdef _WIN32

# if 0 /* 1 = fork like, 0 = native non-blocking */ /* there is not much difference currently */

    static BOOL process_create_pipe(PHANDLE hRead, PHANDLE hWrite, LPSECURITY_ATTRIBUTES lpSA)
    {
        if (CreatePipe(hRead, hWrite, lpSA, 0)) {
            SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);
            return TRUE;
        } else {
            return FALSE;
        }
    }

    static int processlib_read(lua_State *L)
    {
        process_data *process  = processlib_aux_valid(L, 1);
        int           callback = lua_type(L, 2) == LUA_TFUNCTION;

        if (! process || process->closed || process->nulled || ! process->read) {
            if (callback && process && process->length > 0) {
                processlib_callback(L, process, NULL, 0, 1);
            }
            lua_pushboolean(L, 1); /* we're done */
            return 1;
        }

        char buf[buffersize];
        DWORD avail   = 0;
        DWORD bytes   = 0;
        BOOL  peek_ok = PeekNamedPipe(process->read, NULL, 0, NULL, &avail, NULL);

        if (! peek_ok) {
            DWORD err = GetLastError();
            if (err == ERROR_BROKEN_PIPE || err == ERROR_PIPE_NOT_CONNECTED) {
                if (callback && process->length > 0) {
                    processlib_callback(L, process, NULL, 0, 1);
                }
                lua_pushboolean(L, 1); /* we're done */
                return 1;
            } else {
                lua_pushboolean(L, 0); /* we continue */
                return 1;
            }
        }

        if (avail > 0) {
            if (ReadFile(process->read, buf, sizeof(buf), &bytes, NULL) && bytes > 0) {
                if (callback) {
                    processlib_callback(L, process, buf, (size_t) bytes, 0);
                    lua_pushboolean(L, 0); /* we continue */
                    return 1;
                } else {
                    lua_pushlstring(L, buf, bytes); /* some data */
                    return 1;
                }
            }
        }

        DWORD exit_code;
        if (GetExitCodeProcess(process->process, &exit_code) && exit_code != STILL_ACTIVE) {
            PeekNamedPipe(process->read, NULL, 0, NULL, &avail, NULL);
            if (avail == 0) {
                if (callback && process->length > 0) {
                    processlib_callback(L, process, NULL, 0, 1);
                }
                lua_pushboolean(L, 1); /* we're done */
                return 1;
            }
        }

        lua_pushboolean(L, 0); /* we continue */
        return 1;
    }

# else

    static BOOL process_create_pipe(PHANDLE hRead, PHANDLE hWrite, LPSECURITY_ATTRIBUTES lpSA) /* non-blocking */
    {
        if (! CreatePipe(hRead, hWrite, lpSA, 0)) {
            return FALSE;
        } else {
            /* prevent child process from inheriting the read end */
            SetHandleInformation(*hRead, HANDLE_FLAG_INHERIT, 0);
            /* set pipe mode to NOWAIT (non-blocking I/O) */
            DWORD mode = PIPE_READMODE_BYTE | PIPE_NOWAIT;
            if (! SetNamedPipeHandleState(*hRead, &mode, NULL, NULL)) {
                CloseHandle(*hRead);
                CloseHandle(*hWrite);
                *hRead  = NULL;
                *hWrite = NULL;
                return FALSE;
            } else {
                return TRUE;
            }
        }
    }

    static int processlib_read(lua_State *L)
    {
        process_data *process  = processlib_aux_valid(L, 1);
        int           callback = lua_type(L, 2) == LUA_TFUNCTION;

        if (! process || process->closed || process->nulled || ! process->read) {
            if (callback && process && process->length > 0) {
                processlib_callback(L, process, NULL, 0, 1);
            }
            lua_pushboolean(L, 1); /* we're done */
            return 1;
        }

     // /* loop until pipe is empty so child process is never blocked on stdout writes */

        char  buf[buffersize];
        DWORD bytes_read = 0;

     // while (1) {
            BOOL ok = ReadFile(process->read, buf, sizeof(buf), &bytes_read, NULL);
            if (ok && bytes_read > 0) {
                if (callback) {
                    processlib_callback(L, process, buf, (size_t) bytes_read, 0);
                    lua_pushboolean(L, 0); /* we continue */
                    return 1;
                } else {
                    lua_pushlstring(L, buf, bytes_read); /* some data */
                    return 1;
                }
            } else {
                DWORD err = GetLastError();
                if (! ok && (err == ERROR_NO_DATA || err == ERROR_MORE_DATA)) {
                    lua_pushboolean(L, 1); /* we continue */
                    return 1;
                } else {
                    /* broken pipe or EOF */
                    if (callback && process->length > 0) {
                        processlib_callback(L, process, NULL, 0, 1);
                    }
                    lua_pushboolean(L, 1); /* we're done */
                    return 1;
                }
            }
     // }
    }

# endif

    /*tex
        We delegate quoting to the caller because we also have |--foo="bar bar"| like arguments.
    */

    static int processlib_open(lua_State *L)
    {
        int nulled = lua_toboolean(L, 2);

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
            if (! process_create_pipe(&hRead, &hWrite, &sa)) {
                tex_formatted_warning("process lib", "creating pipe failed");
                return 0;
            }
        }

        STARTUPINFOA        si = { 0 };
        PROCESS_INFORMATION pi = { 0 };

        si.cb          = sizeof(STARTUPINFOA);
        si.hStdOutput  = nulled ? hNull : hWrite;
        si.hStdError   = nulled ? hNull : hWrite;
        si.dwFlags    |= STARTF_USESTDHANDLES;

        size_t len;
        const char *cmd_constant = lua_tolstring(L, 1, &len);
        char       *cmd_mutable  = lmt_memory_malloc(len + 1);

        if (! cmd_mutable) {
            if (hRead)  CloseHandle(hRead);
            if (hWrite) CloseHandle(hWrite);
            if (hNull)  CloseHandle(hNull);
            tex_formatted_warning("process lib", "out of memory");
            return 0;
        } else {
            memcpy(cmd_mutable, cmd_constant, len + 1);
        }

        BOOL result = CreateProcessA(
            NULL,
            cmd_mutable,      // has to be writable
            NULL,
            NULL,
            TRUE,             // inherit handles
            CREATE_NO_WINDOW, // instead of 0
            NULL,
            NULL,
            &si,
            &pi
        );

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
        process->read     = hRead;
        process->process  = pi.hProcess;
        process->closed   = 0;
        process->nulled   = nulled;
        process->partial  = NULL;
        process->length   = 0;
        process->capacity = 0;
        return 1;
    }

    /*tex

        We played with avoiding the fetching from the table but for our use case it doesn't really
        matters much. We need to allocate and free then (or impose a maximum). The not so accurate
        timer (rounds) has also some impact but it can be neglected. We can use the one in timerlib
        if needed. We have (in the archive) a variant that uses a specific windows feature: delegate
        the checking using an array of processes. However, it means more code. This is mostly used
        for parallel \CONTEXT\ runs, like the test suite and experiments gave the impression that
        we're talking noise here.

        Todo: test again, as it might matter when we don't null the output. Another thing to play
        with is collecting more because now the 4096 can end in the middle of a line.

    */

    static int processlib_poll(lua_State *L)
    {
        luaL_checktype(L, 1, LUA_TTABLE);
        ULONGLONG timeout = (ULONGLONG) luaL_optinteger(L, 2, -1);
        ULONGLONG start   = GetTickCount64();
        int       count   = (int) lua_rawlen(L, 1);
        int       index   = 1;
        lua_newtable(L);
        if (count == 0) {
            /*tex Normally we check before we call the poller, but just in case. */
            return 1;
        }
        /*tex
            We could validate the userdat in the table once and then avoid repetitive
            checking of the userdata.
        */
        while (1) {
            int ready = 0;
            for (int i = 1; i <= count; i++) {
                lua_rawgeti(L, 1, i);
                process_data *process = processlib_aux_valid(L, -1);
                lua_pop(L, 1);
                if (process && ! process->closed) {
                    if (process->nulled) {
                        /* nulled processes have no pipe; check if the process terminated */
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
                    /* check if pipe is broken (EOF) or process has terminated */
                    DWORD err    = GetLastError();
                    BOOL  broken = ! peek_ok && (err == ERROR_BROKEN_PIPE || err == ERROR_PIPE_NOT_CONNECTED);
                    BOOL  dead   = (WaitForSingleObject(process->process, 0) == WAIT_OBJECT_0);
                    if (broken || dead) {
                        /* mark as ready so caller knows to read (which will hit EOF) or close */
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
            Sleep(5); /* rounded to 15 ms */
        }
        return 1;
    }

    static int processlib_close(lua_State *L)
    {
        process_data *process = processlib_aux_valid(L, 1);
        if (process && ! process->closed) {
            DWORD exitcode = 0;
            if (process->read) {
                CloseHandle(process->read);
                process->read = NULL;
            }
            if (process->process) {
                WaitForSingleObject(process->process, INFINITE);
                GetExitCodeProcess(process->process, &exitcode);
                CloseHandle(process->process);
                process->process = NULL;
            }
            lua_pushinteger(L, exitcode);
            if (process->partial) {
                lmt_memory_free(process->partial);
                process->partial  = NULL;
                process->length   = 0;
                process->capacity = 0;
            }
            process->closed = 1;
        } else {
            lua_pushnil(L);
        }
        return 1;
    }

# else

    # define MAX_ARGS 64

    /*tex
        Here we need to split the arguments, so

             context "foo.tex" --crap --foo="oof foo"

        has to become:

            context
            foo.tex
            --crap
            --foo="oof foo"

        but we need to check that.
    */

    static int parse_command(char *cmd, char **argv)
    {
        int   argc      = 0;
        char *p         = cmd;
        int   in_quotes = 0;
        while (*p && argc < MAX_ARGS - 1) {
            /* we normally only have spaces in context but there can be multiple */
            while (*p && (*p == ' ' || *p == '\t')) {
                p++;
            }
            if (*p == '\0') {
                break;
            }
            /* next argument */
            argv[argc++] = p;
            char *dst = p;
            /* obey quotes after = */
            int quote_is_literal = 0;
            while (*p) {
                if (*p == '"') {
                    /* quote right after '=': treat the quotes as literal characters */
                    if (! in_quotes && dst > argv[argc - 1] && *(dst - 1) == '=') {
                        quote_is_literal = 1;
                    }
                    if (quote_is_literal) {
                        /* keep the quote character in the output */
                        *dst++ = *p++;
                        in_quotes = ! in_quotes;
                        if (! in_quotes) {
                            quote_is_literal = 0; /* reset state on closing quote */
                        }
                    } else {
                        /* outer wrapper quote: strip it */
                        in_quotes = ! in_quotes;
                        p++;
                    }
                } else if ((*p == ' ' || *p == '\t') && ! in_quotes) {
                    p++;   /* skip space */
                    break; /* end current argument */
                } else {
                    *dst++ = *p++; /* copy regular character in-place */
                }
            }
            *dst = '\0'; /* null-terminate the current argument */
        }
        argv[argc] = NULL;
        return argc;
    }

    static int processlib_open(lua_State *L)
    {
        char       *argv[MAX_ARGS];
        size_t      len;
        const char *cmd_constant = lua_tolstring(L, 1, &len);
        char       *cmd_mutable  = lmt_memory_malloc(len + 1);

        if (! cmd_mutable) {
            tex_formatted_warning("process lib", "out of memory");
            return 0;
        } else {
            memcpy(cmd_mutable, cmd_constant, len + 1);
        }

        int nulled = lua_toboolean(L, 2);
        int count  = parse_command(cmd_mutable, argv);

        if (! count) {
            lmt_memory_free(cmd_mutable);
            return 0;
        }

        int pipefd[2] = { -1, -1 };

        if (! nulled) {
            if (pipe(pipefd) == -1) {
                tex_formatted_warning("process lib", "creating pipe failed");
                lmt_memory_free(cmd_mutable);
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
            tex_formatted_warning("process lib", "fork failed");
            lmt_memory_free(cmd_mutable);
            return 0;
        }

        /* child process */
        if (pid == 0) {
            if (nulled) {
                int devnull = open("/dev/null", O_RDWR);
                if (devnull == -1) {
                    _exit(127);
                }
                if (dup2(devnull, STDIN_FILENO) == -1
                    || dup2(devnull, STDOUT_FILENO) == -1
                    || dup2(devnull, STDERR_FILENO) == -1) {
                    _exit(127);
                }
                if (devnull > STDERR_FILENO) {
                    close(devnull);
                }
            } else {
                close(pipefd[0]);                /* close unused read end */
                /* redirect standard output and standard error */
                if (dup2(pipefd[1], STDOUT_FILENO) == -1
                    || dup2(pipefd[1], STDERR_FILENO) == -1) {
                    _exit(127);
                }
                close(pipefd[1]);                /* close duplicate handle */
            }

            execvp(argv[0], argv);               /* replace process image */
            _exit(127);
        }

        lmt_memory_free(cmd_mutable);

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
        process->read     = nulled ? -1 : pipefd[0];
        process->process  = pid;
        process->closed   = 0;
        process->nulled   = nulled;
        process->exited   = 0;
        process->status   = -1;
        process->partial  = NULL;
        process->length   = 0;
        process->capacity = 0;
        return 1;
    }

    static int processlib_read(lua_State *L)
    {
        process_data *process  = processlib_aux_valid(L, 1);
        int           callback = lua_type(L, 2) == LUA_TFUNCTION;

        if (! process || process->closed || process->nulled || process->read < 0) {
            if (callback && process && process->length > 0) {
                processlib_callback(L, process, NULL, 0, 1);
            }
            lua_pushboolean(L, 1); /* we're done */
            return 1;
        }

        char buf[buffersize];
        ssize_t n;
        do {
            n = read(process->read, buf, sizeof(buf));
        } while (n == -1 && errno == EINTR);

        if (n > 0) {
            if (callback) {
                processlib_callback(L, process, buf, (size_t) n, 0);
                lua_pushboolean(L, 0); /* we continue */
                return 1;
            } else {
                lua_pushlstring(L, buf, n);
                return 1;
            }
        } else if (n == 0) {
            /* EOF reached */
            if (callback && process->length > 0) {
                processlib_callback(L, process, NULL, 0, 1);
            }
            lua_pushboolean(L, 1); /* we're done */
            return 1;
        } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
            lua_pushboolean(L, 0); /* we continue */
            return 1;
        } else {
            lua_pushboolean(L, 1); /* we're done */
            return 1;
        }
    }

    static inline int processlib_aux_reap(process_data *process)
    {
        if (process->exited) {
            return 1;
        } else {
            int   status;
            pid_t result;
            do {
                result = waitpid(process->process, &status, WNOHANG);
            } while (result == -1 && errno == EINTR);
            if (result == process->process) {
                process->status = status;
                process->exited = 1;
            } else if (result == -1) {
                /* There is no useful status left if waiting failed. */
                process->status = -1;
                process->exited = 1;
            }
            return process->exited;
        }
    }

    static int processlib_poll(lua_State *L)
    {
        luaL_checktype(L, 1, LUA_TTABLE);
        int timeout  = (int) luaL_optinteger(L, 2, -1);
        int count    = (int) lua_rawlen(L, 1);
        int index    = 1;
        int ready    = 0;
        int fd_count = 0;
        int nulled   = 0;

        lua_newtable(L);
        if (count == 0) {
            return 1;
        }

        struct pollfd *fds = lmt_memory_malloc(sizeof(struct pollfd) * count);
        if (! fds) {
            tex_formatted_warning("process lib", "out of memory");
            return 0;
        }

        for (int i = 1; i <= count; i++) {
            lua_rawgeti(L, 1, i);
            process_data *process = processlib_aux_valid(L, -1);
            lua_pop(L, 1);
            if (process && ! process->closed) {
                if (process->nulled || process->read < 0) {
                    nulled++;
                } else {
                    fds[fd_count].fd     = process->read;
                    fds[fd_count].events = POLLIN | POLLHUP | POLLERR;
                    fd_count++;
                }
            }
        }

        if (fd_count == 0 && nulled == 0) {
            lmt_memory_free(fds);
            return 1;
        }

        int remaining = timeout;
        while (ready == 0) {
            if (nulled) {
                for (int i = 1; i <= count; i++) {
                    lua_rawgeti(L, 1, i);
                    process_data *process = processlib_aux_valid(L, -1);
                    lua_pop(L, 1);
                    if (process && ! process->closed && (process->nulled || process->read < 0) && processlib_aux_reap(process)) {
                        lua_pushinteger(L, i);
                        lua_rawseti(L, -2, index++);
                        ready++;
                    }
                }
                if (ready > 0) {
                    break;
                }
            }

            if (remaining == 0) {
                break;
            }

            int wait = remaining;
            if (nulled && (wait < 0 || wait > 5)) {
                /* waitpid cannot be included in poll, so check it periodically */
                wait = 5;
            }

            int ret;
            do {
                ret = poll(fds, fd_count, wait);
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
                            ready++;
                        }
                        fd_idx++;
                    }
                }
                break;
            } else if (ret < 0) {
                break;
            }

            if (remaining > 0) {
                remaining -= wait;
            }
            if (! nulled) {
                break;
            }
        }
        lmt_memory_free(fds);
        return 1;
    }

    static int processlib_close(lua_State *L)
    {
        process_data *process = processlib_aux_valid(L, 1);
        if (process && ! process->closed) {
            if (process->read >= 0) {
                close(process->read);
                process->read = -1;
            }
            if (! process->exited) {
                int   status;
                pid_t p;
                do {
                    p = waitpid(process->process, &status, 0);
                } while (p == -1 && errno == EINTR);
                if (p == process->process) {
                    process->status = status;
                } else {
                    process->status = -1;
                }
                process->exited = 1;
            }
            if (process->partial) {
                lmt_memory_free(process->partial);
                process->partial  = NULL;
                process->length   = 0;
                process->capacity = 0;
            }
            process->closed = 1;
            if (process->status >= 0 && WIFEXITED(process->status)) {
                lua_pushinteger(L, WEXITSTATUS(process->status));
            } else if (process->status >= 0 && WIFSIGNALED(process->status)) {
                lua_pushinteger(L, -WTERMSIG(process->status));
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
