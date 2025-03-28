#ifndef DISPLAY_H
#define DISPLAY_H

#include <iostream>

#include <poll.h>
#include <tuple>
// #include <type_traits>

#include <cassert>
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
    virtual ProtocolBase* find(const wl_interface*) noexcept = 0;
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

    ProtocolBase* find(const wl_interface* iface) noexcept override
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
    static Display& init(const char *name = nullptr)
    {
        assert(!s_instance);

        s_instance = std::make_unique<Display>(name, 
                                               std::make_unique<ProtocolTuple<Seat,
                                                                FractionalScaleManager,
                                                                G...>>());
        return *s_instance.get();
    }

    inline static Display& instance() noexcept
    {
        assert(s_instance);
        return *s_instance.get();
    }

    Display(Display const&) = delete;
    Display& operator=(Display const&) = delete;
    Display(Display&& other) = default;
    Display& operator=(Display&&) = delete;

    ~Display();
    
    void start_event();
    void stop(){isRuning = false;}

    wl_display* native() const {return m_display;}
    //wl_compositor* compositor() const {return m_compositor;}

    template <typename G>
    const G* protocol() const
    {
        if (auto search = m_protocols->find(G::iface()))
            return static_cast<G*>(search);

        return nullptr;
    }

    template <typename G>
    const G& ensureProtocol() const
    {
        if (auto search = m_protocols->find(G::iface()))
            return *static_cast<G*>(search);

        std::string err("protocol not find: ");
        throw std::runtime_error(err + G::iface()->name);
    }

    Display(const char *name, std::unique_ptr<ProtocolContainer> &&protocols);

private:
    wl_display* m_display;
    wl_compositor* m_compositor;

    std::unique_ptr<ProtocolContainer> m_protocols;

    enum EventT
    {wayland, system, count};

    pollfd m_fds[EventT::count];

    using DrawFunc = std::function<void()>;
    struct Node
    {
        DrawFunc draw_f;
        Node *next{nullptr};
        Node *prev{nullptr};
    };

    //std::vector<DrawFunc> m_surfaces;
    Node *m_surfaces;
    void add(Node &node);

    bool isRuning;
    static std::unique_ptr<Display> s_instance;

    template<typename>
    friend class Surface;
};

template <typename CTX>
class Surface : public NativeSurface
{
    using Buffer = std::unique_ptr<typename CTX::Buffer>;

    static inline CTX *s_ctx = nullptr;
    static inline void *s_buf = nullptr;

    Display::Node m_node;

public:
    Surface():
        NativeSurface(Display::instance().m_compositor,
                      Display::instance().protocol<FractionalScaleManager>())
    {}

    ~Surface()
    {
        s_ctx->destroy(*m_buffer.get());

        //если окно последнее уничтожаем контекст
        if (m_node.prev == &m_node){
            Display::instance().m_surfaces = nullptr;
            delete s_ctx;
            s_ctx = nullptr;
        }
        else {
            m_node.prev->next = m_node.next;
        }
    }

    void refresh()
    {
        if (mapped())
            m_node.draw_f = [this](){
                if (s_buf != m_buffer.get()){
                    s_ctx->makeCurrent(*m_buffer.get());
                    s_buf = m_buffer.get();
                }
                draw(m_scale);
                s_ctx->flush(*m_buffer.get());
            };
    }

protected:
    Buffer m_buffer;

    CTX& context() const
    {
        //Context динамически создается для предсказуемого удаления т.к. он static
        if (!s_ctx) s_ctx = new CTX;
        return *s_ctx;
    }

    bool mapped() const {return m_node.prev != nullptr;}

    bool makeBuffer(unsigned &width, unsigned &height)
    {
        auto scale_width  = width * m_scale;
        auto scale_height = height * m_scale;
        auto buf = std::make_unique<typename CTX::Buffer>(
            context(), *this, scale_width, scale_height);

        if (buf->valid()){
            Display::instance().add(m_node);
            width = scale_width;
            height = scale_height;
            s_ctx->makeCurrent(*buf.get());
            s_buf = buf.get();
            m_buffer = std::move(buf);
            return true;
        }

        return false;
    }

    void setAreaOpaque(int32_t w, int32_t h) const
    {NativeSurface::setAreaOpaque(Display::instance().m_compositor, w, h);}

    virtual void draw(float) = 0;
};

}

#endif // DISPLAY_H
