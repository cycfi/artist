#include "xdgshell.h"

#include <iostream>

using namespace WL;

void XDGWmBase::bind(wl_registry *reg, uint32_t name, uint32_t version) noexcept
{
    registry(reg, name, version);

    static const xdg_wm_base_listener listener = {
        .ping = [](void*, xdg_wm_base *wm_base, uint32_t serial){
            xdg_wm_base_pong(wm_base, serial);
        }
    };

    xdg_wm_base_add_listener (c_ptr(), &listener, nullptr);
}

NativeToplevel::NativeToplevel(const NativeSurface &surface, uint32_t width, uint32_t height)
    : m_xdg_surfase(xdg_wm_base_get_xdg_surface(
                        Display::instance().ensureProtocol<XDGWmBase>().c_ptr(),
                        surface.c_ptr()),
                    "Can't create xdg surface")
    , m_top(xdg_surface_get_toplevel(m_xdg_surfase.c_ptr()),
            "Can't create toplevel role")
    , m_decor(Display::instance().protocol<XDGDecorateManager>() ?
                  zxdg_decoration_manager_v1_get_toplevel_decoration(
                    Display::instance().protocol<XDGDecorateManager>()->c_ptr(), m_top.c_ptr())
                                                                 :nullptr)
    , m_width(width)
    , m_height(height)
    , m_state(0)
{
    static const xdg_toplevel_listener toplevel_lsr{
        .configure = [](void* data, xdg_toplevel *tt,
                        int32_t width, int32_t height, wl_array *states){

            auto win = static_cast<NativeToplevel*>(data);

            xdg_toplevel_state *pos;
            win->m_state = 0;
            std::cout<<"states "<<states->size<<" W "<<width<<" H "<<height<<std::endl;

            for (pos = (xdg_toplevel_state*)states->data;
                (const char *) pos < ((const char *) states->data + states->size);
                (pos)++){

                    std::cout<<"--- "<<*pos<<std::endl;

                    win->m_state |= (1 << *pos);
            }

            if (width && height){
                win->m_width  = width;
                win->m_height = height;
            }
        },
        .close = [](void *data, xdg_toplevel*){
            auto win = static_cast<NativeToplevel*>(data);
            win->closed();
        },
        .configure_bounds = [](void* data, xdg_toplevel*, int32_t width, int32_t height){

            std::cout<<"configure_bound: "<< width<<" "<< height<<std::endl;

        },
        .wm_capabilities = [](void *data,
                                  xdg_toplevel *xdg_toplevel,
                                  wl_array *capabilities){}
    };

    xdg_toplevel_add_listener(m_top.c_ptr(), &toplevel_lsr, this);

    if (m_decor)
        zxdg_toplevel_decoration_v1_set_mode(m_decor.c_ptr(),
                                             ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
}
