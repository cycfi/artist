#ifndef DISPLAY_H
#define DISPLAY_H

#include <poll.h>
#include <tuple>
#include <stdexcept>
#include <cstring>
#include <memory>

#include <wayland-client-core.h>

#include "core.h"

namespace WL {

class ProtocolContainer
{
public:
    virtual ~ProtocolContainer() = default;
    virtual ProtocolBase* find(const char*) noexcept = 0;
    virtual const ProtocolBase* find(const wl_interface*) noexcept = 0;
};

template<typename... Global>
class ProtocolTuple final: public ProtocolContainer
{
    std::tuple<Global...> m_gb;

    template<typename F>
    void tuple_find(F &&f) noexcept
    {
        std::apply([&f](auto &...e) {
            (std::forward<F>(f)(e, e.iface()) || ...);
        }, m_gb);
    }

public:
    ProtocolBase* find(const char* str) noexcept override
    {
        ProtocolBase* result = nullptr;
        tuple_find([str, &result](ProtocolBase& gb,
                                  const wl_interface* ifc){
            if (strcmp(str, ifc->name) == 0){
                result = &gb; return true;
            }
            return false;
        });

        return result;
    }

    const ProtocolBase* find(const wl_interface* iface) noexcept override
    {
        ProtocolBase* result = nullptr;
        tuple_find([iface, &result](ProtocolBase& gb,
                                  const wl_interface* ifc){
            if (iface == ifc){
                result = &gb; return true;
            }
            return false;
        });

        return result;
    }

    virtual ~ProtocolTuple()
    {
        std::cerr<<"~GlobalTuple"<<std::endl;
    }
};

class Display
{
public:
    template <typename... G>
    static Display create_default()
    {
        return Display(nullptr, std::make_unique<ProtocolTuple<Seat,
                                                            FractionalScaleManager,
                                                            G...>>());
    }

    Display(Display const&) = delete;
    Display& operator=(Display const&) = delete;
    Display(Display&& other) = default;
    Display& operator=(Display&&) = delete;

    Display(const char *name, std::unique_ptr<ProtocolContainer> &&protocols);
    
    void event_wait();

    wl_display* c_ptr() const {return m_display.get();}
    wl_compositor* compositor() const {return m_compositor.get();}

    template <typename G>
    const G* protocol() const
    {
        if (auto search = m_protocols->find(G::iface()))
            return static_cast<const G*>(search);

        return nullptr;
    }

    template <typename G>
    const G& ensureProtocol() const
    {
        if (auto search = m_protocols->find(G::iface()))
            return *static_cast<const G*>(search);

        std::string err("protocol not found: ");
        throw std::runtime_error(err + G::iface()->name);
    }

private:

    struct Deleter
    {
        void operator()(wl_display* data) const
        {wl_display_disconnect(data);}

        void operator()(wl_compositor* data) const
        {wl_proxy_destroy((wl_proxy*) data);}
    };

    std::unique_ptr<wl_display, Deleter> m_display;
    std::unique_ptr<wl_compositor, Deleter> m_compositor;

    const Seat *m_seat;

    std::unique_ptr<ProtocolContainer> m_protocols;

    enum EventT
    {wayland, key, system, count};

    pollfd m_fds[EventT::count];
};

}

#endif // DISPLAY_H
