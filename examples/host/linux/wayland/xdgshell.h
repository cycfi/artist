#ifndef XDGSHELL_H
#define XDGSHELL_H

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

class Toplevel: public Surface
{
public:
    Toplevel(const Display &dpy, uint32_t width, uint32_t height);

    void setTitle(const char* title)
    {xdg_toplevel_set_title(m_top.c_ptr(), title);}

    void setAppID(const char* id)
    {xdg_toplevel_set_app_id(m_top.c_ptr(), id);}

private:
    Proxy<xdg_surface,  XDG_SURFACE_DESTROY>  m_xdg_surfase;
    Proxy<xdg_toplevel, XDG_TOPLEVEL_DESTROY> m_top;
    Proxy<zxdg_toplevel_decoration_v1, 
          ZXDG_TOPLEVEL_DECORATION_V1_DESTROY> m_decor;

protected:
    uint32_t m_width ;
    uint32_t m_height;
    uint32_t m_state ;

    virtual void closed() = 0;

    /**
     * @brief configure вызывается композитором при изменении состояния (s)
     * или размеров окна (w, h)
     * @param w
     * @param h
     * @param s
     * @return должна возвращать true если требуется перерисовка
     */
    virtual void configure(unsigned w, unsigned h, unsigned s) = 0;
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
