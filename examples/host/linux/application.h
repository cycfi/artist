#ifndef APP_H
#define APP_H

#include "SkSurface.h"

class Application
{
public:
    void run();
    void stop(){isRuning = false;}

    class DisplayImpl;

private:
    bool isRuning{false};
};

enum SurfaceState
{
    resizing   = 1 << 0,
    maximized  = 1 << 1,
    activated  = 1 << 2,
    fullscreen = 1 << 3
};

class Surface
{
public:
    Surface(int width, int height);
    ~Surface();

    void setTitle(const char* txt);

protected:
    /* 
    * Draw frame. Returns true if the next frame needs to be drawn
    */
    virtual bool draw(float /*scale*/) = 0;
    virtual void configure(unsigned /*w*/, unsigned /*h*/, unsigned /*state*/) = 0;
    virtual void closed() = 0;

    sk_sp<SkSurface> skia_surf;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;

    friend class Application;
};

#endif // APP_H
