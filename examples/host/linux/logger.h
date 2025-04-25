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
        syslog(LOG_INFO, msg, args...);
    }

    template<typename ...Arg>
    static void error(const char *msg, Arg&& ...args) noexcept
    {
        syslog(LOG_ERR, msg, args...);
    }

    template<typename ...Arg>
    static void debug(const char *msg, Arg&& ...args) noexcept
    {
#ifndef NDEBUG
        syslog(LOG_DEBUG, msg, args...);
#endif
    }

private:
    Logger() = default;
};

#endif // LOG_H
