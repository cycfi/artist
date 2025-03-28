#ifndef XDGSHELL_H
#define XDGSHELL_H

//#include <functional>

#include "display.h"
#include "xdg-shell-client-protocol.h"
#include "xdg-decoration-unstable-v1-client-protocol.h"

namespace WL {

enum XDGState
{
    resizing  = 1 << XDG_TOPLEVEL_STATE_RESIZING,
    maximized = 1 << XDG_TOPLEVEL_STATE_MAXIMIZED,
    activated = 1 << XDG_TOPLEVEL_STATE_ACTIVATED,
    fullscreen = 1 << XDG_TOPLEVEL_STATE_FULLSCREEN
};

class XDGWmBase final: public Protocol<xdg_wm_base,
                                      &xdg_wm_base_interface,
                                      XDG_WM_BASE_DESTROY>
{
public:
    void bind(wl_registry *reg, uint32_t name, uint32_t version) noexcept override;
};

//--------Decoration--------////////////////////////////////////////////

using XDGDecorateManager = Protocol<zxdg_decoration_manager_v1,
                                  &zxdg_decoration_manager_v1_interface,
                                  ZXDG_DECORATION_MANAGER_V1_DESTROY>;

class NativeToplevel
{
public:
    NativeToplevel(const NativeSurface &surface,
             uint32_t width,
             uint32_t height);

    void setTitle(const char* title)
    {xdg_toplevel_set_title(m_top.c_ptr(), title);}

    void setAppID(const char* id)
    {xdg_toplevel_set_app_id(m_top.c_ptr(), id);}

private:
    Proxy<xdg_surface,  XDG_SURFACE_DESTROY>  m_xdg_surfase;
    Proxy<xdg_toplevel, XDG_TOPLEVEL_DESTROY> m_top;
    Proxy<zxdg_toplevel_decoration_v1, 
          ZXDG_TOPLEVEL_DECORATION_V1_DESTROY> m_decor;

    template<typename>
    friend class Toplevel;

protected:
    uint32_t m_width ;
    uint32_t m_height;
    uint32_t m_state ;

    virtual void closed() = 0;
};

template <typename CTX>
class Toplevel: public Surface<CTX>, public NativeToplevel
{
public:
    Toplevel(uint32_t width, uint32_t height):
        NativeToplevel(*this, width, height)
    {
        static const xdg_surface_listener surface_lsr{
            .configure = [](void* data, xdg_surface *xdg_surf, uint32_t serial){

            auto inst = static_cast<Toplevel*>(data);
            xdg_surface_ack_configure(xdg_surf, serial);

            if (!inst->mapped()) {

                if (!inst->makeBuffer(inst->m_width, inst->m_height)) {
                    std::cerr << "Can't create window frame" << std::endl;
                    return;
                }

                if (inst->configure(inst->m_width, inst->m_height, XDGState::resizing)){
                    inst->draw(inst->m_scale);
                    inst->context().flush(*inst->m_buffer.get());
                }

                return;
            }

            if (inst->m_state & XDGState::resizing)
                inst->m_buffer->resize(inst->m_width, inst->m_height);

            if (inst->configure(inst->m_width, inst->m_height, inst->m_state))
                inst->refresh();
        }};

        xdg_surface_add_listener (this->m_xdg_surfase.c_ptr(), &surface_lsr, this);
        this->commit();
    }

protected:

    /**
     * @brief configure вызывается композитором при изменении состояния (s)
     * или размеров окна (w, h)
     * @param w
     * @param h
     * @param s
     * @return должна возвращать true если требуется перерисовка
     */
    virtual bool configure(unsigned w, unsigned h, unsigned s) = 0;
};


// struct Positioner: Proxy<xdg_positioner, &xdg_positioner_destroy>
// {
//     //const WlApplication &global;

//     // explicit Positioner(const XDGWmBase &shell):
//     //     Proxy<xdg_positioner, &xdg_positioner_destroy>(
//     //           xdg_wm_base_create_positioner(shell.c_ptr()),
//     //           "Can't create positioner")//,
//     // //global(app)
//     // {}
// };

// class Popup: public Proxy<xdg_popup, &xdg_popup_destroy>
// {
// public:
//     Popup(const Positioner &pos,
//           const XDGSurface &parent);

//     //std::function<void()> onClosed;

//     int32_t x, y;
//     int32_t width, height;
// };

}

#endif // XDGSHELL_H
