#include "display.h"

#include <unistd.h>

using namespace WL;

std::unique_ptr<Display> Display::s_instance;

Display::Display(const char *dpy_name, std::unique_ptr<ProtocolContainer> &&protocols)
    : m_display(wl_display_connect(dpy_name))
    , m_compositor(nullptr)
    , m_surfaces(nullptr)
    , isRuning(false)
{
    if (!m_display)
        throw std::runtime_error("display with name 'dpy_name' does not exist");

    static const wl_registry_listener lsr = {
        .global = [](void *data,
                    wl_registry *registry,
                    uint32_t name,
                    const char *interface,
                    uint32_t version)
        {
            auto dpy = static_cast<Display*>(data);
           // std::cerr<<"available interface "<<interface<<" "<<version<<std::endl;
            if (strcmp(interface, wl_compositor_interface.name) == 0)
                dpy->m_compositor =
                    (wl_compositor*)wl_registry_bind(registry, name, &wl_compositor_interface, version);
            else
                if (auto item = dpy->m_protocols->find(interface))
                    item->bind(registry, name, version);

        },
        .global_remove = [](void*, wl_registry*, uint32_t){
            //To do global remove
        }
    };

    auto registry = wl_display_get_registry(m_display);

    if (!registry || wl_registry_add_listener(registry, &lsr, this) < 0){
        wl_display_disconnect(m_display);
        throw std::runtime_error("add registry listener failed");
    }

    m_protocols = std::move(protocols);

    wl_display_roundtrip(m_display);
    wl_registry_destroy(registry);

    if (!m_compositor) {
        m_protocols.reset(nullptr);
        wl_display_disconnect(m_display);
        throw std::runtime_error("wayland compositor not found");
    }

    m_fds[EventT::wayland].fd = wl_display_get_fd(m_display);
    m_fds[EventT::wayland].events = POLLIN;
    m_fds[EventT::system].fd = -1;
}

Display::~Display()
{
    if (m_compositor)
        wl_proxy_destroy((wl_proxy*) m_compositor);

    m_protocols.reset(nullptr);
    wl_display_disconnect(m_display);
}

void Display::start_event()
{
    if (isRuning) return;

    isRuning = true;

    auto seat = protocol<Seat>();

    if (seat){
        m_fds[EventT::system].fd = seat->key_repeat_fd;
        m_fds[EventT::system].events = POLLIN;
        m_fds[EventT::system].revents = 0;
    }

    while (isRuning) {

        while (wl_display_prepare_read(m_display) != 0) {
            if (wl_display_dispatch_pending(m_display) < 0)
                throw std::runtime_error("failed to dispatch pending Wayland events");
        }

        int ret;
        
        while ((ret = wl_display_flush(m_display)) < 0) {
            if (errno == EINTR) {
                continue;
            }

            if (errno == EAGAIN) {
                struct pollfd fds[1];
                fds[0].fd = m_fds[EventT::wayland].fd;
                fds[0].events = POLLOUT;

                do {
                    ret = poll(fds, 1, -1);
                } while (ret < 0 && errno == EINTR);
            }
        }

        if (ret < 0) {
            wl_display_cancel_read(m_display);
            throw std::runtime_error("failed to display flush");
        }

        do {
            ret = poll(m_fds, EventT::count, -1);
        } while (ret < 0 && errno == EINTR);

        if (ret < 0) {
            if (m_fds[0].revents & POLLHUP)
            throw std::runtime_error("disconnected from wayland");

            wl_display_cancel_read(m_display);
            throw std::runtime_error("failed to poll():");
        }

        if ((m_fds[0].revents & POLLIN)) {

            if (wl_display_read_events(m_display) < 0)
                throw std::runtime_error("failed to read Wayland events");
        }
        else
            wl_display_cancel_read(m_display);

        if (wl_display_dispatch_pending(m_display) < 0)
            throw std::runtime_error("failed to dispatch pending Wayland events");

        if (m_fds[1].revents & POLLIN) {
            uint64_t repeats;
            if (read(m_fds[1].fd, &repeats, sizeof(repeats)) == 8) {
                for (uint64_t i = 0; i < repeats; i++)
                    seat->emitKeypress();
            }
        }

        auto node = m_surfaces;
        while (node){
            if (node->draw_f){
                auto cb = std::move(node->draw_f);
                cb();
            }

            node = node->next;
        }
    }
}

void Display::add(Node &node)
{
    if(!m_surfaces){
        m_surfaces = &node;
        m_surfaces->prev = m_surfaces;
    }
    else {
        auto last = m_surfaces->prev;
        m_surfaces->prev = &node;
        node.prev = last;
        last->next = &node;
    }
}
