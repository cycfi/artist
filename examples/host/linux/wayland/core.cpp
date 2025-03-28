#include "core.h"

#include <unistd.h>
#include <xkbcommon/xkbcommon.h>
#include <sys/mman.h>
//#include <sys/timerfd.h>
#include <errno.h>
#include <cassert>

#include <iostream>

namespace WL {

////////---- Surface Impl ----///////////////////////////////////////

NativeSurface::NativeSurface(wl_compositor *compositor,
                 const FractionalScaleManager *manager):
    Base(wl_compositor_create_surface(compositor), "Can't create surface"),
    m_fscale(manager ? wp_fractional_scale_manager_v1_get_fractional_scale(manager->c_ptr(), c_ptr())
                     : nullptr)
{
    if (m_fscale) {
        static const wp_fractional_scale_v1_listener lsr{
            .preferred_scale = [](void *data, wp_fractional_scale_v1 *, uint32_t scale) {
                auto self = static_cast<NativeSurface *>(data);
                auto val = (float)scale / 120;

                self->m_scale = val;
                if (self->onScale) self->onScale(val);
            }
        };

        wp_fractional_scale_v1_add_listener(m_fscale.c_ptr(), &lsr, this);
    }
}

void NativeSurface::setAreaOpaque(wl_compositor *compositor, int32_t w, int32_t h) const
{
    auto region = wl_compositor_create_region(compositor);
    if (!region)
        return;

    wl_region_add(region, 0, 0, w, h);
    wl_surface_set_opaque_region(c_ptr(), region);
    wl_region_destroy(region);
}


////////---- Seat Impl ----//////////////////////////////////////////

struct Keyboard::KeyMapper
{
    xkb_context *kcontext = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    xkb_keymap  *keymap   = nullptr;
    xkb_state   *kstate   = nullptr;
    uint32_t keycode   = 0;
    uint32_t keysymbol = 0;

    ~KeyMapper()
    {
        if (kstate)
            xkb_state_unref(kstate);
        if (keymap)
            xkb_keymap_unref(keymap);
        if (kcontext)
            xkb_context_unref(kcontext);
    }

    bool map(int32_t fd, uint32_t size) noexcept;
    bool keySymbol(uint32_t key) noexcept;
};

bool Keyboard::KeyMapper::map(int32_t fd, uint32_t size) noexcept
{
    if(!kcontext){
        std::cerr<<"Error: create xkb context"<<std::endl;
        return false;
    }

    void *addr = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (addr == MAP_FAILED) {
        std::cerr<<"Error: XKBKeyboard mmap code "<<errno<<std::endl;
        return false;
    }

    if (keymap)
        xkb_keymap_unref(keymap);
    if (!(keymap = xkb_keymap_new_from_string(kcontext,
                                        (const char*)addr,
                                        XKB_KEYMAP_FORMAT_TEXT_V1,
                                        XKB_KEYMAP_COMPILE_NO_FLAGS))){
        std::cerr<<"Error: create xkb keymap"<<std::endl;
        return false;
    }

    if (kstate)
        xkb_state_unref(kstate);
    if (!(kstate = xkb_state_new(keymap))){
        std::cerr<<"Error: create xkb kstate"<<std::endl;
        return false;
    }

    if (munmap(addr, size) < 0){
        std::cerr<<"Error: XKBKeyboard munmap code "<<errno<<std::endl;
        return false;
    }
    
    return true;
}

bool Keyboard::KeyMapper::keySymbol(uint32_t key) noexcept
{
    auto raw_key = key + 8;
    auto sym = xkb_state_key_get_one_sym(kstate, raw_key);

    if ((sym >= XKB_KEY_Shift_L && sym <= XKB_KEY_Hyper_R) ||
        (sym >= XKB_KEY_ISO_Lock && sym <= XKB_KEY_ISO_Last_Group_Lock) ||
        sym == XKB_KEY_Mode_switch ||
        sym == XKB_KEY_Num_Lock)
        return false;

    keycode = raw_key;
    keysymbol = sym;
    return true;
}

Keyboard::~Keyboard()
{
    delete m_mapper;
}

unsigned Keyboard::modifiers(ModSet group) const
{
    if (m_mapper)
        switch (group) {
        case ModSet::effective:
            return xkb_state_serialize_mods(
                m_mapper->kstate, XKB_STATE_MODS_EFFECTIVE);
        case ModSet::consumed:
            return xkb_state_key_get_consumed_mods2(
                m_mapper->kstate,  m_mapper->keycode, XKB_CONSUMED_MODE_XKB);
        case ModSet::locked:
            return xkb_state_serialize_mods(
                m_mapper->kstate, XKB_STATE_MODS_LOCKED);
        default:
            break;
        }

    return 0;
}

const char* Keyboard::utf8() const
{
    return "";
}

uint32_t Keyboard::symbol() const
{
    return m_mapper->keysymbol;
}

void Seat::emitKeypress() const
{
    assert(m_focused_surf);
    assert(m_focused_surf->onKey);

    m_focused_surf->onKey(m_keyboard);
}

const wl_keyboard_listener Seat::keyboard_listener = {
    .keymap = [](void *data, wl_keyboard*,
                 uint32_t format, int32_t fd,
                 uint32_t size){

        auto& kb = static_cast<Seat*>(data)->m_keyboard;

        if (format == WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1 && !kb.m_mapper)
            kb.m_mapper = new(std::nothrow) Keyboard::KeyMapper;
        //else
            //To Do WL_KEYBOARD_KEYMAP_FORMAT_NO_KEYMAP

        if(kb.m_mapper){
            if (!kb.m_mapper->map(fd, size)) {
                delete kb.m_mapper;
                kb.m_mapper = nullptr;
                //To Do log file
            }
        }
        else
            std::cerr<<"KEYMAP_FORMAT not supported, format code: "<<format<<std::endl;

        close(fd);
    },
    .enter = [](void *data, wl_keyboard*, uint32_t serial,
                wl_surface *surface, wl_array *keys){
            
        auto seat = static_cast<Seat*>(data);

        seat->m_focused_surf = static_cast<SeatListener*>(
                                            wl_surface_get_user_data(surface));

        if (seat->m_focused_surf && seat->m_focused_surf->onFocused)
            seat->m_focused_surf->onFocused(true);
    },
    .leave = [](void *data, wl_keyboard *wl_kd, uint32_t,
                wl_surface*){
            
        auto seat = static_cast<Seat*>(data);

        if (seat->m_focused_surf->onFocused)
            seat->m_focused_surf->onFocused(false);

        seat->m_focused_surf = nullptr;

        struct itimerspec timer = {0};
        timerfd_settime(seat->key_repeat_fd, 0, &timer, NULL);
    },
    .key = [](void *data, wl_keyboard *wl_kd, uint32_t /*serial*/,
              uint32_t time,
              uint32_t key,
              uint32_t state){

        auto seat = static_cast<Seat*>(data);
        auto mapper = seat->m_keyboard.m_mapper;
            
        if (seat->m_focused_surf &&
            mapper &&
            seat->m_focused_surf->onKey){

            bool pressed = (state == WL_KEYBOARD_KEY_STATE_PRESSED);

            itimerspec timer = {0};

            if (seat->rate != 0 &&
                xkb_keymap_key_repeats(mapper->keymap, key+8) &&
                pressed) {

                timer = {
                    .it_interval = {.tv_sec = 0, .tv_nsec = seat->rate},
                    .it_value    = {.tv_sec = 0, .tv_nsec = seat->delay},
                };

                if (timer.it_value.tv_nsec >= 1000000000) {
                    timer.it_value.tv_sec += timer.it_value.tv_nsec / 1000000000;
                    timer.it_value.tv_nsec %= 1000000000;
                }
                if (timer.it_interval.tv_nsec >= 1000000000) {
                    timer.it_interval.tv_sec += timer.it_interval.tv_nsec / 1000000000;
                    timer.it_interval.tv_nsec %= 1000000000;
                }
            }

            //std::cout<<"repeat_info "<<timer.it_interval.tv_nsec<<std::endl;

            timerfd_settime(seat->key_repeat_fd, 0, &timer, NULL);

            if (pressed && mapper->keySymbol(key))
                seat->emitKeypress();
        }
    },
    .modifiers = [](void *data, wl_keyboard *, uint32_t /*serial*/,
                        uint32_t mods_depressed, //which key
                        uint32_t mods_latched,
                        uint32_t mods_locked,
                        uint32_t group){

        auto mapper = static_cast<Seat*>(data)->m_keyboard.m_mapper;

        if (mapper)
            xkb_state_update_mask(mapper->kstate,
                                  mods_depressed, mods_latched, mods_locked,
                                  0, 0, group);
            
        //std::cout<<"modifiers "<<mods_depressed<<" "<<mods_latched<<" "<<mods_locked<<" "<<group<<std::endl;
    },
    .repeat_info = [](void *data, wl_keyboard *wl_kd,
                          int32_t rate, int32_t delay){
            
        auto seat = static_cast<Seat*>(data);
            //std::cout<<"repeat_info "<<rate<<" "<<delay<<" "<<std::endl;
        if (seat){

            /**
             * rate скорость генерации кол-во символов в s
             * delay кол-во ms в течении которых нужно удерживать клавишу для начала повтора
            */
            seat->delay = delay * 1000000;
            seat->rate = 1000000000 / rate;
        }
    }
};

const wl_pointer_listener Seat::pointer_listener = {
    .enter = [](void *data, wl_pointer *pointer,
                uint32_t serial, wl_surface *surface,
                wl_fixed_t sx, wl_fixed_t sy)
    {
        // Happens in the case we just destroyed the surface.
        if (!surface) return;

        auto seat = static_cast<Seat*>(data);
        auto surf = static_cast<SeatListener*>(
            wl_surface_get_user_data(surface));

        assert(seat && seat->m_pointer.c_ptr() == pointer);

        if (surf){
            seat->m_hovered_surf = surf;

            if (surf->onPoint)
                surf->onPoint(SeatListener::enter, 0, seat->m_pointer.set(sx, sy));
        }
    },
    .leave = [](void *data, wl_pointer *pointer,
                uint32_t serial, wl_surface *surface)
    {
        auto seat = static_cast<Seat*>(data);

        assert(seat && seat->m_pointer.c_ptr() == pointer);

        if (seat->m_hovered_surf) {

            if (seat->m_hovered_surf->onPoint)
                seat->m_hovered_surf->onPoint(SeatListener::leave, 0, seat->m_pointer);

            seat->m_hovered_surf = nullptr;
        }
    },
    .motion = [](void *data, wl_pointer *pointer,
                 uint32_t time, wl_fixed_t sx, wl_fixed_t sy)
    {
        auto seat = static_cast<Seat*>(data);

        assert(seat && seat->m_pointer.c_ptr() == pointer);

        if (seat->m_hovered_surf && seat->m_hovered_surf->onPoint)
            seat->m_hovered_surf->onPoint(SeatListener::motion, time, seat->m_pointer.set(sx, sy));
    },
    .button = [](void *data, wl_pointer *pointer,
                 uint32_t serial, uint32_t time, uint32_t button,
                 uint32_t state)
    {
        auto seat = static_cast<Seat*>(data);

        assert(seat && seat->m_pointer.c_ptr() == pointer);

        if (seat->m_hovered_surf && seat->m_hovered_surf->onClick) {

            /* count click */
            timespec now;
            clock_gettime(CLOCK_MONOTONIC, &now);

            if (state == WL_POINTER_BUTTON_STATE_PRESSED){

                if (seat->m_last_released_button == button &&
                    now.tv_sec == seat->m_stamp &&
                    time - seat->m_last_time <= 300)
                    seat->m_count_click++;
                else
                    seat->m_count_click = 1;

                auto& key = seat->m_keyboard;

                seat->m_hovered_surf->onClick((Pointer::BTN)button, true,
                                            seat->m_count_click, 
                                            key ? key.modifiers(Keyboard::ModSet::effective) : 0,
                                            seat->m_pointer );
            }
            else {
                seat->m_last_released_button = button;
                seat->m_last_time = time;
                seat->m_stamp = now.tv_sec;
                seat->m_hovered_surf->onClick((Pointer::BTN)button, false, 0, 0, seat->m_pointer);
            }
        }
    },
    .axis = [](void *data, wl_pointer *pointer,
               uint32_t time, uint32_t axis, wl_fixed_t value)
    {
        auto seat = static_cast<Seat*>(data);

        assert(seat && seat->m_pointer.c_ptr() == pointer);

        if (seat->m_hovered_surf && seat->m_hovered_surf->onScroll)
            seat->m_hovered_surf->onScroll(time, axis, wl_fixed_to_double(value));
    }
};

Seat::~Seat()
{
    if (key_repeat_fd >= 0)
        close(key_repeat_fd);
}

void Seat::bind(wl_registry *reg, uint32_t name, uint32_t version) noexcept
{
    registry(reg, name, 4);

    static const wl_seat_listener seat_listener = {
        .capabilities = [](void *data, wl_seat *wl_seat, uint32_t flags){

            auto seat = static_cast<Seat*>(data);
            //std::cout<<"capabilities "<<flags<<std::endl;

            if (flags & WL_SEAT_CAPABILITY_POINTER) {
                auto pointer = wl_seat_get_pointer(wl_seat);
                seat->m_pointer.reset(pointer);
                if (wl_pointer_add_listener(pointer, &pointer_listener, seat) < 0)
                    std::cerr<<"Error: не удалось добавить pointer listener"<<std::endl;
            }   
            else 
                seat->m_pointer.reset(nullptr);

            if (flags &  WL_SEAT_CAPABILITY_KEYBOARD){
                auto kb = wl_seat_get_keyboard(wl_seat);
                seat->m_keyboard.reset(kb);
                if (wl_keyboard_add_listener(kb, &keyboard_listener, seat) < 0)
                    std::cerr<<"Error: не удалось добавить keyboard listener"<<std::endl;
            }
            else
                seat->m_keyboard.reset(nullptr);

                //self->touch.init(app, wl_seat, capabilities);
        },
        .name = [](void*, wl_seat*, const char *name){
            std::cout<<"Seat name: "<<name<<std::endl;
        }//пока не реализовано
    };

    if (wl_seat_add_listener(c_ptr(), &seat_listener, this) < 0)
        std::cerr<<"Error: не удалось добавить seat listener"<<std::endl;
}

Viewport::Viewport(Viewporter const *vpr, NativeSurface const &surface)
    :Base(vpr ? wp_viewporter_get_viewport(vpr->c_ptr(), surface.c_ptr()) : nullptr)
{}

}

