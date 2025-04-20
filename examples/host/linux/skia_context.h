#ifndef SKIA_CONTEXT_H
#define SKIA_CONTEXT_H

#include "GrDirectContext.h"
#include "gl/GrGLAssembleInterface.h"

#include "SkSurface.h"

class GlSkiaContext
{
public:
    GlSkiaContext(GlSkiaContext const&) = delete;
    GlSkiaContext& operator=(GlSkiaContext const&) = delete;
    GlSkiaContext(GlSkiaContext&& other) = delete;
    GlSkiaContext& operator=(GlSkiaContext&&) = delete;

    GlSkiaContext() = default;

    void init(GrGLGetProc gl_get_proc);

    sk_sp<SkSurface> makeSurface(int width, int height) noexcept;

private:
    sk_sp<GrDirectContext> _ctx;
    std::unique_ptr<GrBackendRenderTarget> target;
};

#endif //  SKIA_CONTEXT_H
