#include "core.h"

#include <unistd.h>
#include <xkbcommon/xkbcommon.h>
#include <sys/mman.h>
#include <errno.h>
#include <cassert>

#include "../logger.h"

namespace WL {

////////---- Surface Impl ----///////////////////////////////////////

Surface::Surface(wl_compositor *compositor,
    const FractionalScaleManager *manager):
Base(wl_compositor_create_surface(compositor), "Can't create surface"),
m_scale(1),
m_fscale(manager ? wp_fractional_scale_manager_v1_get_fractional_scale(manager->c_ptr(), c_ptr())
        : nullptr),
m_input_hundler(nullptr)
{
    static const wl_surface_listener lsr {
        .enter = [](void *data,
            struct wl_surface *wl_surface,
            struct wl_output *output){

        },

        .leave = [](void *data,
            struct wl_surface *wl_surface,
            struct wl_output *output){
        },

        .preferred_buffer_scale = [](void *data,
                                struct wl_surface *wl_surface,
                                int32_t factor){
            auto surface = static_cast<Surface*>(data);
            if (!surface->m_fscale)
            surface->buffer_scale(factor);
        },

        .preferred_buffer_transform = [](void *data,
                                    struct wl_surface *wl_surface,
                                    uint32_t transform){

        }
    };

    wl_surface_add_listener(c_ptr(), &lsr, this);

    if (m_fscale) {
        static const wp_fractional_scale_v1_listener lsr{
            .preferred_scale = [](void *data, wp_fractional_scale_v1 *, uint32_t scale) {
                auto surface = static_cast<Surface *>(data);
                auto val = (float)scale / 120;

                surface->m_scale = val;
                surface->buffer_scale(val);
            }
        };

        wp_fractional_scale_v1_add_listener(m_fscale.c_ptr(), &lsr, this);
    }
}

void Surface::setAreaOpaque(wl_compositor *compositor, int32_t w, int32_t h) const
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
        Logger::error("xkb context failed");
        return false;
    }

    void *addr = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (addr == MAP_FAILED) {
        Logger::error("Error: XKBKeyboard mmap code %i", errno);
        return false;
    }

    if (keymap)
        xkb_keymap_unref(keymap);
    if (!(keymap = xkb_keymap_new_from_string(kcontext,
                                        (const char*)addr,
                                        XKB_KEYMAP_FORMAT_TEXT_V1,
                                        XKB_KEYMAP_COMPILE_NO_FLAGS))){
        Logger::error("Error: create xkb keymap");
        return false;
    }

    if (kstate)
        xkb_state_unref(kstate);
    if (!(kstate = xkb_state_new(keymap))){
        Logger::error("Error: create xkb kstate");
        return false;
    }

    if (munmap(addr, size) < 0){
        Logger::error("Error: XKBKeyboard munmap code %i", errno);
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

    m_focused_surf->key(m_keyboard);
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
            if(!kb.m_mapper->map(fd, size)){
                delete kb.m_mapper;
                kb.m_mapper = nullptr;

                Logger::error("KEYMAP_FORMAT failed");
            }
        }
        else
            Logger::error("KEYMAP_FORMAT not supported, format code: %d", format);

        close(fd);
    },
    .enter = [](void *data, wl_keyboard*, uint32_t serial,
                wl_surface *surface, wl_array *keys){
            
        auto seat = static_cast<Seat*>(data);
        auto surf = static_cast<Surface*>(
            wl_surface_get_user_data(surface));

        if (surf){
            seat->m_focused_surf = surf->m_input_hundler;

            if (seat->m_focused_surf)
                seat->m_focused_surf->keyFocused(true);
        }
    },
    .leave = [](void *data, wl_keyboard *wl_kd, uint32_t,
                wl_surface*){
            
        auto seat = static_cast<Seat*>(data);

        if (seat->m_focused_surf){

            seat->m_focused_surf->keyFocused(false);
            seat->m_focused_surf = nullptr;

            struct itimerspec timer = {0};
            timerfd_settime(seat->key_repeat_fd, 0, &timer, NULL);
        }
    },
    .key = [](void *data, wl_keyboard *wl_kd, uint32_t /*serial*/,
              uint32_t time,
              uint32_t key,
              uint32_t state){

        auto seat = static_cast<Seat*>(data);
        auto mapper = seat->m_keyboard.m_mapper;
            
        if (seat->m_focused_surf && mapper){

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

            Logger::debug("repeat_info %d", timer.it_interval.tv_nsec);

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
            
    },
    .repeat_info = [](void *data, wl_keyboard *wl_kd,
                          int32_t rate, int32_t delay){
            
        auto seat = static_cast<Seat*>(data);
    
        if (seat){

            //The protocol guarantees non-zero values, but it doesn't hurt to check.
            //Just in case, in Russian
            if (rate != 0 && delay != 0){

                /**
                * rate - generation speed (number of characters in sec)
                * delay - number of ms during which you need to hold the key before the repeat starts
                */
                seat->delay = delay * 1000000;
                seat->rate = 1000000000 / rate;
            }
            else
                Logger::error("Incorrect keyboard parameters rate = %i, delay = %i",
                              rate, delay);
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
        auto surf = static_cast<Surface*>(
            wl_surface_get_user_data(surface));

        assert(seat && seat->m_pointer.c_ptr() == pointer);

        if (surf){
            seat->m_hovered_surf = surf->m_input_hundler;

            if (seat->m_hovered_surf)
                seat->m_hovered_surf->point(SeatListener::enter, 0, seat->m_pointer.set(sx, sy));
        }
    },
    .leave = [](void *data, wl_pointer *pointer,
                uint32_t serial, wl_surface *surface)
    {
        auto seat = static_cast<Seat*>(data);

        assert(seat && seat->m_pointer.c_ptr() == pointer);

        if (seat->m_hovered_surf) {

            seat->m_hovered_surf->point(SeatListener::leave, 0, seat->m_pointer);
            seat->m_hovered_surf = nullptr;
        }
    },
    .motion = [](void *data, wl_pointer *pointer,
                 uint32_t time, wl_fixed_t sx, wl_fixed_t sy)
    {
        auto seat = static_cast<Seat*>(data);

        assert(seat && seat->m_pointer.c_ptr() == pointer);

        if (seat->m_hovered_surf)
            seat->m_hovered_surf->point(SeatListener::motion, time, seat->m_pointer.set(sx, sy));
    },
    .button = [](void *data, wl_pointer *pointer,
                 uint32_t serial, uint32_t time, uint32_t button,
                 uint32_t state)
    {
        auto seat = static_cast<Seat*>(data);

        assert(seat && seat->m_pointer.c_ptr() == pointer);

        if (seat->m_hovered_surf) {

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

                seat->m_hovered_surf->click((Pointer::BTN)button, true,
                                            seat->m_count_click, 
                                            key ? key.modifiers(Keyboard::ModSet::effective) : 0,
                                            seat->m_pointer );
            }
            else {
                seat->m_last_released_button = button;
                seat->m_last_time = time;
                seat->m_stamp = now.tv_sec;
                seat->m_hovered_surf->click((Pointer::BTN)button, false, 0, 0, seat->m_pointer);
            }
        }
    },
    .axis = [](void *data, wl_pointer *pointer,
               uint32_t time, uint32_t axis, wl_fixed_t value)
    {
        auto seat = static_cast<Seat*>(data);

        assert(seat && seat->m_pointer.c_ptr() == pointer);

        if (seat->m_hovered_surf)
            seat->m_hovered_surf->scroll(time, axis, wl_fixed_to_double(value));
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

            if (flags & WL_SEAT_CAPABILITY_POINTER) {
                auto pointer = wl_seat_get_pointer(wl_seat);
                seat->m_pointer.reset(pointer);
                if (wl_pointer_add_listener(pointer, &pointer_listener, seat) < 0)
                    Logger::error("failed to add pointer listener");
            }   
            else 
                seat->m_pointer.reset(nullptr);

            if (flags &  WL_SEAT_CAPABILITY_KEYBOARD){
                auto kb = wl_seat_get_keyboard(wl_seat);
                seat->m_keyboard.reset(kb);
                if (wl_keyboard_add_listener(kb, &keyboard_listener, seat) < 0)
                    Logger::error("failed to add keyboard listener");
            }
            else
                seat->m_keyboard.reset(nullptr);
        },
        .name = [](void*, wl_seat*, const char *name){
            Logger::debug("Seat connected, name: %s", name);
        }
    };

    if (wl_seat_add_listener(c_ptr(), &seat_listener, this) < 0)
        Logger::error("failed to add seat listener");
}

Viewport::Viewport(Viewporter const *vpr, Surface const &surface)
    :Base(vpr ? wp_viewporter_get_viewport(vpr->c_ptr(), surface.c_ptr()) : nullptr)
{}

}

