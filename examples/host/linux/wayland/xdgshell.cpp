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

Toplevel::Toplevel(const Display &dpy, uint32_t width, uint32_t height)
    : Surface(dpy.compositor(), dpy.protocol<FractionalScaleManager>())
    , m_xdg_surfase(xdg_wm_base_get_xdg_surface(dpy.ensureProtocol<XDGWmBase>().c_ptr(), c_ptr()),
                    "Can't create xdg surface")
    , m_top(xdg_surface_get_toplevel(m_xdg_surfase.c_ptr()),
            "Can't create toplevel role")
    , m_decor(dpy.protocol<XDGDecorateManager>() ?
                  zxdg_decoration_manager_v1_get_toplevel_decoration(
                    dpy.protocol<XDGDecorateManager>()->c_ptr(), m_top.c_ptr())
                                                                 :nullptr)
    , m_width(width)
    , m_height(height)
    , m_state(0)
{
    static const xdg_toplevel_listener toplevel_lsr{
        .configure = [](void* data, xdg_toplevel *tt,
                        int32_t width, int32_t height, wl_array *states){

            auto inst = static_cast<Toplevel*>(data);

            xdg_toplevel_state *pos;
            inst->m_state = 0;

            for (pos = (xdg_toplevel_state*)states->data;
                (const char *) pos < ((const char *) states->data + states->size);
                (pos)++){

                    //std::cout<<"--- "<<*pos<<std::endl;

                    inst->m_state |= (1 << *pos);
            }

            if (width && height){
                inst->m_width  = width;
                inst->m_height = height;
            }

            //std::cout<<"states "<<win->m_state<<" W "<<width<<" H "<<height<<std::endl;
        },
        .close = [](void *data, xdg_toplevel*){
            auto inst = static_cast<Toplevel*>(data);
            inst->closed();
        },
        .configure_bounds = [](void* data, xdg_toplevel*, int32_t width, int32_t height){

            std::cout<<"configure_bound: "<< width<<" "<< height<<std::endl;

        },
        .wm_capabilities = [](void *data,
                                  xdg_toplevel *xdg_toplevel,
                                  wl_array *capabilities){
            std::cout<<"wm_capabilities: "<<std::endl;
            // auto inst = static_cast<Toplevel*>(data);

            // inst->mapped();
        }
    };

    xdg_toplevel_add_listener(m_top.c_ptr(), &toplevel_lsr, this);

    static const xdg_surface_listener surface_lsr{
        .configure = [](void* data, xdg_surface *xdg_surf, uint32_t serial){

          auto inst = static_cast<Toplevel*>(data);
          xdg_surface_ack_configure(xdg_surf, serial);
std::cout<<"xdg_surface_listener states "<<inst->m_state<<" W "<<inst->m_width<<" H "<<inst->m_height<<std::endl;
            if (inst->m_state != 0)
                inst->configure(inst->m_width, inst->m_height, inst->m_state);
        }
    };

    xdg_surface_add_listener (this->m_xdg_surfase.c_ptr(), &surface_lsr, this);
    commit();

    if (m_decor)
        zxdg_toplevel_decoration_v1_set_mode(m_decor.c_ptr(),
                                             ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
}
