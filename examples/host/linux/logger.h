#ifndef LOG_H
#define LOG_H

#include <syslog.h>

class Logger final
{
public:
    explicit Logger(const char* app_id)
    {
        openlog(app_id, LOG_PERROR | LOG_PID, LOG_USER);
    }

    ~Logger()
    {
        closelog();
    }

    template<typename ...Arg>
    static void message(const char *msg, Arg&& ...args) noexcept
    {
        if constexpr (sizeof...(args))
            syslog(LOG_INFO, msg, args...);
        else
            syslog(LOG_INFO, "%s", msg);
    }

    template<typename ...Arg>
    static void error(const char *msg, Arg&& ...args) noexcept
    {
        if constexpr (sizeof...(args))
            syslog(LOG_ERR, msg, args...);
        else
            syslog(LOG_ERR, "%s", msg);
    }

    template<typename ...Arg>
    static void debug(const char *msg, Arg&& ...args) noexcept
    {
#ifndef NDEBUG
        if constexpr (sizeof...(args))
            syslog(LOG_DEBUG, msg, args...);
        else
            syslog(LOG_DEBUG, "%s", msg);
#endif
    }

private:
    Logger() = default;
};

#ifndef NDEBUG
#define PRINT_DBG(fmt, ...)\
fprintf(stderr, "DEBUG: %s: %d:%s(): " fmt "\n", \
    __FILE_NAME__, __LINE__, __func__, __VA_ARGS__)
#else
#define PRINT_DBG(fmt, ...) ((void)0)
#endif

#endif // LOG_H
