#ifndef SKIA_CONTEXT_H
#define SKIA_CONTEXT_H

//#include "ganesh/GrBackendSurface.h"
#include "ganesh/GrDirectContext.h"

#include "SkSurface.h"
#include "ganesh/gl/GrGLInterface.h"

class GlSkiaContext
{
public:
    GlSkiaContext(GlSkiaContext const&) = delete;
    GlSkiaContext& operator=(GlSkiaContext const&) = delete;
    GlSkiaContext(GlSkiaContext&& other) = delete;
    GlSkiaContext& operator=(GlSkiaContext&&) = delete;

    GlSkiaContext() = default;

    void init(sk_sp<const GrGLInterface> iface);

    sk_sp<SkSurface> makeSurface(int width, int height);

    void flush(const sk_sp<SkSurface> &surf) const;

private:
    sk_sp<GrDirectContext> _ctx;
    //GrBackendRenderTarget target;
};

#endif //  SKIA_CONTEXT_H
