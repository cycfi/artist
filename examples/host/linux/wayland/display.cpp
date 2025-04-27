#include "display.h"

#include <unistd.h>

using namespace WL;

Display::Display(const char *dpy_name, std::unique_ptr<ProtocolContainer> &&protocols)
    : m_display(wl_display_connect(dpy_name))
    , m_compositor(nullptr)
    , m_protocols(std::move(protocols))
{
    if (!m_display)
        throw std::runtime_error("display does not exist");

    static const wl_registry_listener lsr = {
        .global = [](void *data,
                    wl_registry *registry,
                    uint32_t name,
                    const char *interface,
                    uint32_t version)
        {
            auto dpy = static_cast<Display*>(data);

            if (strcmp(interface, wl_compositor_interface.name) == 0)
                dpy->m_compositor.reset(
                    (wl_compositor*)wl_registry_bind(registry, name, &wl_compositor_interface, version));
            else
                if (auto item = dpy->m_protocols->find(interface))
                    item->bind(registry, name, version);

        },
        .global_remove = [](void*, wl_registry*, uint32_t){
            //To do global remove
        }
    };

    auto registry = wl_display_get_registry(m_display.get());

    if (!registry || wl_registry_add_listener(registry, &lsr, this) < 0)
        throw std::runtime_error("add registry listener failed");


    wl_display_roundtrip(m_display.get());
    wl_registry_destroy(registry);

    if (!m_compositor)
        throw std::runtime_error("wayland compositor not found");

    m_fds[EventT::wayland].fd = wl_display_get_fd(m_display.get());
    m_fds[EventT::wayland].events = POLLIN;
    m_fds[EventT::system].fd = -1; //To do add system interrupts

    m_seat = protocol<Seat>();
    m_fds[EventT::key].fd = m_seat ? m_seat->key_repeat_fd : -1;
    m_fds[EventT::key].events = POLLIN;
}

void Display::event_wait()
{
    auto display = c_ptr();

    while (wl_display_prepare_read(display) != 0) {
        if (wl_display_dispatch_pending(display) < 0)
            throw std::runtime_error("failed to dispatch pending Wayland events");
    }

    int ret;

    while ((ret = wl_display_flush(display)) < 0) {
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
        wl_display_cancel_read(display);
        throw std::runtime_error("failed to display flush");
    }

    do {
        ret = poll(m_fds, EventT::count - 1, -1); //To do add system interrupts
    } while (ret < 0 && errno == EINTR);

    if (ret < 0) {
        if (m_fds[0].revents & POLLHUP)
            throw std::runtime_error("disconnected from wayland");

        wl_display_cancel_read(display);
        throw std::runtime_error("failed to poll():");
    }

    if ((m_fds[0].revents & POLLIN)) {

        if (wl_display_read_events(display) < 0)
            throw std::runtime_error("failed to read Wayland events");
    }
    else
        wl_display_cancel_read(display);

    if (wl_display_dispatch_pending(display) < 0)
        throw std::runtime_error("failed to dispatch pending Wayland events");

    if (m_fds[1].revents & POLLIN) {
        uint64_t repeats;
        if (read(m_fds[1].fd, &repeats, sizeof(repeats)) == 8) {
            for (uint64_t i = 0; i < repeats; i++)
                m_seat->emitKeypress();
        }
    }
}
