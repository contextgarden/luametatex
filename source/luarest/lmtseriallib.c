/*
    See license.txt in the root of this project.
*/

# include "luametatex.h"

/*tex See |lmtinterface.h| for |SERIAL_METATABLE_INSTANCE|. */

# ifdef _WIN32

    /* see: https://ds.opdenbrouw.nl/micprg/pdf/serial-win.pdf */
    /* see: https://learn.microsoft.com/en-us/previous-versions/ff802693(v=msdn.10)?redirectedfrom=MSDN */

    typedef struct serial_data {
        HANDLE   handle;
        int      closed;
    } serial_data;

 //  CreateFile(
 //      "\\\\.\\COM10", // address of name of the communications device: \\.\COM10
 //      fdwAccess,      // access (read-write) mode
 //      0,              // share mode
 //      NULL,           // address of security descriptor
 //      OPEN_EXISTING,  // how to create
 //      0,              // file attributes
 //      NULL            // handle of file with attributes to copy
 //  );

    static serial_data *seriallib_aux_open(lua_State *L, serial_data *existing)
    {
        int           success  = 0;
        HANDLE        handle   = INVALID_HANDLE_VALUE;
        const char   *portname = lua_type(L, 1) == LUA_TSTRING ? lua_tostring(L, 1) : NULL;
        lua_Integer   baudrate = lua_type(L, 2) == LUA_TNUMBER ? lua_tointeger(L, 2) : CBR_19200;
        DCB           dcb      = { 0 };
        COMMTIMEOUTS  timeouts = { 0 };

        if (! portname || strlen(portname) == 0) {
            goto DONE;
        }

        handle = CreateFileA(
            portname,
            GENERIC_READ | GENERIC_WRITE,
            0, /* Sharing doesn't work for serial devices. */
            NULL,
            OPEN_EXISTING,
            0,
            NULL
        );
        if (handle == INVALID_HANDLE_VALUE) {
            goto DONE;
        }

        dcb.DCBlength = sizeof(dcb);
        if (! GetCommState(handle, &dcb)) {
            goto DONE;
        }

        dcb.BaudRate = (DWORD) (baudrate ? baudrate : CBR_19200);
        dcb.ByteSize = 8;
        dcb.StopBits = ONESTOPBIT;
        dcb.Parity   = NOPARITY;
        if (! SetCommState(handle, &dcb)) {
            goto DONE;
        }

        timeouts.ReadIntervalTimeout         = 50;
        timeouts.ReadTotalTimeoutConstant    = 50;
        timeouts.ReadTotalTimeoutMultiplier  = 10;
        timeouts.WriteTotalTimeoutConstant   = 50;
        timeouts.WriteTotalTimeoutMultiplier = 10;
        if (SetCommTimeouts(handle, &timeouts) == FALSE) {
            goto DONE;
        }

        success = 1;

      DONE:
        if (success) {
            serial_data *serial = existing ? existing : (serial_data *) lua_newuserdatauv(L, sizeof(serial_data), 1);
            serial->handle = handle;
            serial->closed = 0;
            if (! existing) {
                lua_pushstring(L, portname);
                lua_setiuservalue(L, -2, 1);
            }
            return serial;
        } else {
            if (handle != INVALID_HANDLE_VALUE) {
                CloseHandle(handle);
            }
            return NULL;
        }
    }

    static void seriallib_aux_close(serial_data *serial)
    {
        if (serial->handle != INVALID_HANDLE_VALUE) {
            CloseHandle(serial->handle);
            serial->handle = INVALID_HANDLE_VALUE;
        }
    }

    static int seriallib_aux_send(serial_data *serial, const char *sequence, size_t length)
    {
        DWORD written = 0;
        if (WriteFile(serial->handle, sequence, (DWORD) length, &written, NULL)) {
            return (written == (DWORD) length) ? 1 : 0;
        }
        return 0;
    }

    static int seriallib_write(lua_State *L)
    {
        serial_data serial = {
            .handle = INVALID_HANDLE_VALUE,
            .closed = 0
        };
        int success = seriallib_aux_open(L, &serial) && (serial.handle != INVALID_HANDLE_VALUE);
        if (success) {
            size_t length = 0;
            const char *sequence = lua_tolstring(L, 3, &length);
            if (length) {
                success = seriallib_aux_send(&serial, sequence, length);
            }
            seriallib_aux_close(&serial);
        }
        lua_pushboolean(L, success);
        return 1;
    }

# else

    #include <stdio.h>
    #include <fcntl.h>
    #include <termios.h>
    #include <unistd.h>
    #include <errno.h>

	/* /dev/ttyUSBx /dev/ttySx /dev/ttyACM0 */

    typedef struct serial_data {
        int handle;
        int closed;
    } serial_data;

    static speed_t seriallib_aux_get_baud_constant(lua_Integer baud)
    {
        switch (baud) {
            case     50: return     B50;
            case     75: return     B75;
            case    110: return    B110;
            case    134: return    B134;
            case    150: return    B150;
            case    200: return    B200;
            case    300: return    B300;
            case    600: return    B600;
            case   1200: return   B1200;
            case   1800: return   B1800;
            case   2400: return   B2400;
            case   4800: return   B4800;
            case   9600: return   B9600;
            case  19200: return  B19200;
            case  38400: return  B38400;
            case  57600: return  B57600;
            case 115200: return B115200;
            case 230400: return B230400;
            default:     return  B19200;
        }
    }

    static serial_data *seriallib_aux_open(lua_State *L, serial_data *existing)
    {
        int            success  = 0;
        int            handle   = -1;
        const char    *portname = lua_type(L, 1) == LUA_TSTRING ? lua_tostring (L, 1) : NULL;
        lua_Integer    baudrate = lua_type(L, 2) == LUA_TNUMBER ? lua_tointeger(L, 2) : 19200;
        struct termios settings;

        if (! portname || strlen(portname) == 0) {
            goto DONE;
        }

        handle = open(portname, O_RDWR | O_NOCTTY | O_NDELAY);
        if (handle == -1) {
            goto DONE;
        }

        if (tcgetattr(handle, &settings) != 0) {
            goto DONE;
        }

        speed_t speed = seriallib_aux_get_baud_constant(baudrate);
        cfsetispeed(&settings, speed);
        cfsetospeed(&settings, speed);

        settings.c_cflag &= ~PARENB;
        settings.c_cflag &= ~CSTOPB;
        settings.c_cflag &= ~CSIZE;
        settings.c_cflag |=  CS8;
        settings.c_cflag &= ~CRTSCTS;
        settings.c_cflag |=  (CREAD | CLOCAL);
        settings.c_iflag &= ~(IXON | IXOFF | IXANY);
        settings.c_iflag &= ~(ICANON | ECHO | ECHOE | ISIG);
        settings.c_oflag &= ~OPOST;

        // ASYNC_LOW_LATENCY

     // settings.c_cc[VMIN]  = 0; // don't block
     // settings.c_cc[VTIME] = 2; // timeout in 1/10 sec

     // settings.c_lflag  = 0; /* no echo etc */
     // settings.c_oflag  = 0;

     // cfmakeraw(&settings);

        if (tcsetattr(handle, TCSANOW, &settings) != 0) {
            goto DONE;
        }

        /* Reset O_NDELAY flag for standard operation */
        fcntl(handle, F_SETFL, 0);

        success = 1;

      DONE:
        if (success) {
            serial_data *serial = existing ? existing : (serial_data *) lua_newuserdatauv(L, sizeof(serial_data), 1);
            if (! existing) {
                lua_pushstring(L, portname);
                lua_setiuservalue(L, -2, 1);
            }
            serial->handle = handle;
            serial->closed = 0;
            return serial;
        } else {
            if (handle >= 0) {
                close(handle);
            }
            return NULL;
        }
    }

    static void seriallib_aux_close(serial_data *serial)
    {
        if (serial->handle >= 0) {
            close(serial->handle);
            serial->handle = -1;
        }
    }

    static int seriallib_aux_send(serial_data *serial, const char *sequence, size_t length)
    {
        ssize_t res = write(serial->handle, sequence, length);
        return (res == (ssize_t) length) ? 1 : 0;
    }

    static int seriallib_write(lua_State *L)
    {
        serial_data serial = {
            .handle = -1,
            .closed = 0
        };
        int success = seriallib_aux_open(L, &serial) && serial.handle >= 0;
        if (success) {
            size_t      length   = 0;
            const char *sequence = lua_tolstring(L, 3, &length);
            if (length) {
                success = seriallib_aux_send(&serial, sequence, length);
            }
            seriallib_aux_close(&serial);
        }
        lua_pushboolean(L, success);
        return 1;
    }

# endif

static inline serial_data * seriallib_aux_valid(lua_State *L, int i)
{
    serial_data * serial = lua_touserdata(L, i);
    if (serial && lua_getmetatable(L, i)) {
        lua_get_metatablelua(serial_instance);
        if (! lua_rawequal(L, -1, -2)) {
           serial = NULL;
        }
        lua_pop(L, 2);
    }
    return serial;
}

static int seriallib_open(lua_State *L)
{
    serial_data *serial = seriallib_aux_open(L, NULL);
    if (serial) {
        lua_get_metatablelua(serial_instance);
        lua_setmetatable(L, -2);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

static int seriallib_send(lua_State *L)
{
    serial_data *serial = seriallib_aux_valid(L, 1);
    int success = 0;
    if (serial && ! serial->closed) {
        size_t      length   = 0;
        const char *sequence = lua_tolstring(L, 2, &length);
        if (sequence && length) {
            success = seriallib_aux_send(serial, sequence, length);
        }
    }
    lua_pushboolean(L, success);
    return 1;
}

static int seriallib_close(lua_State *L)
{
    serial_data * serial = seriallib_aux_valid(L, 1);
    if (serial && ! serial->closed) {
        serial->closed = 1;
        seriallib_aux_close(serial);
    }
    return 0;
}

static int seriallib_tostring(lua_State *L)
{
    serial_data * serial = seriallib_aux_valid(L, 1);
    if (serial && ! serial->closed) {
        const char *port;
        lua_getiuservalue(L, 1, 1);
        port = lua_tostring(L, -1);
        lua_pop(L, 1);
        lua_pushfstring(L, "<serial %p : %s>", serial, port ? port : "unknown");
        return 1;
    } else {
        return 0;
    }
}

static const luaL_Reg seriallib_function_list[] =
{
    /* management */
    { "open",         seriallib_open      },
    { "close",        seriallib_close     },
    { "send",         seriallib_send      },
    { "write",        seriallib_write     },
    /* */
    { "tostring",     seriallib_tostring  },
    /* */
    { "__gc",         seriallib_close     },
    /* */
    { NULL,           NULL                },
};

int luaopen_serial(lua_State *L)
{
    luaL_newmetatable(L, SERIAL_METATABLE_INSTANCE);
    luaL_setfuncs(L, seriallib_function_list, 0);
    lua_pushliteral(L, "__index");
    lua_pushvalue(L, -2);
    lua_settable(L, -3);
    lua_pushliteral(L, "__tostring");
    lua_pushliteral(L, "tostring");
    lua_gettable(L, -3);
    lua_settable(L, -3);
    lua_pushliteral(L, "__name");
    lua_pushliteral(L, "serial");
    lua_settable(L, -3);
    return 1;
}
