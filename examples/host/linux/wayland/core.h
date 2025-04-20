#pragma once

#include <ctime>
//#include <iostream>

#include <functional>
#include <stdexcept>
#include <sys/timerfd.h>
#include <wayland-client-protocol.h>

#include "fractional-scale-v1-client-protocol.h"
#include "viewporter-client-protocol.h"

namespace WL
{

template <int DCode>
class ProxyBase
{
public:
    ~ProxyBase()
    {
        if (m_ptr)
            wl_proxy_marshal_flags(m_ptr, DCode, NULL,
                                   version(), WL_MARSHAL_FLAG_DESTROY);
    }

    inline operator bool() const noexcept
    {return m_ptr != nullptr;}

    inline uint32_t version() const noexcept
    {return wl_proxy_get_version((wl_proxy*)m_ptr);}

protected:
    ProxyBase():m_ptr(nullptr){}

    template <typename T>
    ProxyBase(T* ptr):m_ptr((wl_proxy*)ptr){}

    ProxyBase(ProxyBase const&) = delete;
    ProxyBase& operator=(ProxyBase const&) = delete;

    ProxyBase(ProxyBase&& other):
        m_ptr(other.m_ptr)
    {
        other.m_ptr = nullptr;
    }

    wl_proxy* m_ptr;
};

class ProtocolBase
{
public:
    virtual void bind(wl_registry *reg, uint32_t name, uint32_t version) noexcept = 0;
};

template <typename T, const wl_interface* I, int Destroy_code>
class Protocol: public ProtocolBase, public ProxyBase<Destroy_code>
{
public:
    constexpr static const wl_interface* iface(){return I;}

    Protocol() = default;

    Protocol(Protocol&& other) = default;
    Protocol& operator=(Protocol&&) = delete;

    void bind(wl_registry *reg, uint32_t name, uint32_t version) noexcept override
    { registry(reg, name, version);}

    inline T* c_ptr() const noexcept
    { return (T*) this->m_ptr; }

protected:
    void registry(wl_registry *reg, uint32_t name, uint32_t version) noexcept
    { this->m_ptr = static_cast<wl_proxy*>(wl_registry_bind(reg, name, I, version)); }
};

template <typename T, int Destroy_code>
class Proxy: public ProxyBase<Destroy_code>
{
public:
    Proxy() = default;
    explicit Proxy(T* p):ProxyBase<Destroy_code>(p){}

    Proxy(T* p, const char* msg): ProxyBase<Destroy_code>(p)
    { if (!p) throw std::runtime_error(msg); }

    Proxy(Proxy&& other) = default;
    Proxy& operator=(Proxy&& other)
    {
        ~Proxy();
        this->m_ptr = other.m_ptr;
        other.m_ptr = nullptr;

        return *this;
    }

    inline void reset(T* ptr) noexcept
    {
        ~Proxy();
        this->m_ptr = (wl_proxy*)ptr;
    }

    inline T* c_ptr() const noexcept
    {return (T*) this->m_ptr;}

protected:
    using Base = Proxy;
};

class Keyboard: public Proxy<wl_keyboard, WL_KEYBOARD_RELEASE>
{
public:
   ~Keyboard();

   enum ModSet {locked, effective, consumed};

   unsigned modifiers(ModSet group) const;
   const char *utf8() const;
   uint32_t symbol() const;

private:
   struct KeyMapper;
   KeyMapper* m_mapper{nullptr};

   friend class Seat;
};

class Pointer: public Proxy<wl_pointer, WL_POINTER_RELEASE>
{
public:
    std::pair<int, int> toInt() const
    {
        return {wl_fixed_to_int(m_x),
                wl_fixed_to_int(m_y)};
    }

    std::pair<double, double> toDouble() const
    {
        return {wl_fixed_to_double(m_x),
                wl_fixed_to_double(m_y)};
    }

    bool inBound(int sx, int sy, int sw, int sh) const
    {
        auto [x, y] = toInt();
        return sx > x && sw < x && sy > y && sh < y;
    }

    //from input-event-codes.h
    //зависит от платформы, хотя на Linux и FreeBSD одинаковые
    enum BTN {
        LEFT    =   0x110,
        RIGHT   =   0x111,
        MIDDLE	=	0x112,
        SIDE	=	0x113,
        EXTRA	=	0x114,
        FORWARD	=	0x115,
        BACK	=	0x116,
        ASK		=   0x117
    };

private:
    wl_fixed_t m_x{0}, m_y{0};

    const Pointer& set(wl_fixed_t x, wl_fixed_t y)
    {m_x = x; m_y = y; return *this;}

    friend class Seat;
};

class SeatListener
{
public:
    std::function<void(bool)> onFocused;
    std::function<void(Keyboard const &)> onKey;

    //virtual void keyUnfocused() = 0;
    //virtual void key(Keyboard const&) = 0;

    enum PointerState { enter, leave, motion };

    std::function<void(PointerState, unsigned, Pointer const &)> onPoint;
    std::function<void(Pointer::BTN /*button*/,
                       bool /*pressed*/,
                       int /*count*/,
                       unsigned /*key_mod*/,
                       Pointer const &)>  onClick;
    std::function<void(int /*time*/,
                       int /*axis*/,
                       double /*value*/)> onScroll;

   // virtual void enter(uint32_t /*serial*/, Pointer const&) = 0;
   // virtual void leave() = 0;
   // virtual void motion(uint32_t /*time*/, Pointer const&) = 0;
   // virtual void click(Pointer::BTN /*button*/ ,
   //                    bool         /*pressed*/,
   //                    int          /*count*/,
   //                    unsigned     /*key_mod*/,
   //                    Pointer const&) = 0;
   // virtual void scroll(int time, int axis, double value) = 0;

// protected:
//    virtual void keyFocused(bool) = 0;
};

class Seat final: public Protocol<wl_seat, &wl_seat_interface, WL_SEAT_RELEASE>
{
public:
    ~Seat();
    void bind(wl_registry *reg, uint32_t name, uint32_t version) noexcept override;

private:
    // Поддерживает только XKB на Linux и FreeBSD платформах
    // возможно в дальнейшем этот класс нужно будет сделать более гибким

    Keyboard m_keyboard;
    SeatListener *m_focused_surf = nullptr;
    static const wl_keyboard_listener keyboard_listener;
    int key_repeat_fd{timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK)};
    uint32_t delay  {0};
    uint32_t rate   {0};
    void emitKeypress() const;

    friend class Display;

    Pointer  m_pointer;
    SeatListener *m_hovered_surf = nullptr;
    static const wl_pointer_listener pointer_listener;
    //для вычисления нескольких нажатий одной и той же кнопки мыши
    uint32_t m_last_time = 0;
    std::time_t m_stamp;
    uint32_t m_last_released_button = 0;
    int m_count_click = 0;
};

using FractionalScaleManager = Protocol<wp_fractional_scale_manager_v1,
                                      &wp_fractional_scale_manager_v1_interface,
                                      WP_FRACTIONAL_SCALE_MANAGER_V1_DESTROY>;

class Surface: public Proxy<wl_surface, WL_SUBSURFACE_DESTROY>,
               public SeatListener
{
public:
    explicit Surface(wl_compositor *compositor,
                     const FractionalScaleManager *manager);

    inline void commit()
    { wl_surface_commit(c_ptr()); }

protected:
    void setAreaOpaque(wl_compositor *compositor, int32_t w, int32_t h) const;

    float m_scale{1};

    virtual void draw(float /*scale*/) = 0;
    virtual void buffer_scale(float /*scale*/) = 0;

private:
    Proxy<wp_fractional_scale_v1,
          WP_FRACTIONAL_SCALE_V1_DESTROY> m_fscale;
};

using Viewporter = Protocol<wp_viewporter,
                            &wp_viewporter_interface,
                            WP_VIEWPORTER_DESTROY>;

class Viewport final: public Proxy<wp_viewport, WP_VIEWPORT_DESTROY>
{
public:
    Viewport(const WL::Viewporter *vpr, Surface const &surface);
    Viewport() = default;

    void setDestination(int32_t width, int32_t height)
    {wp_viewport_set_destination(c_ptr(), width, height);}
};

}


