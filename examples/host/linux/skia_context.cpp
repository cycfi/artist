#include "skia_context.h"
#include "ganesh/gl/GrGLDirectContext.h"

#include "SkColorSpace.h"

#include <GL/gl.h>

void GlSkiaContext::init(sk_sp<const GrGLInterface> iface)
{
    GrContextOptions opts;
    opts.fSuppressPrints = true;

    _ctx = GrDirectContexts::MakeGL(iface, opts);
    if (!_ctx)
        throw std::runtime_error("failed to make Skia context");
}

sk_sp<SkSurface> GlSkiaContext::makeSurface(int width, int height) noexcept
{
    GrGLFramebufferInfo framebufferInfo;
    framebufferInfo.fFBOID = 0; // assume default framebuffer
    framebufferInfo.fFormat = GL_RGBA8;

    SkColorType colorType = kRGBA_8888_SkColorType;
    target = std::make_unique<GrBackendRenderTarget>(
                                width,
                                height,
                                1, // sample count
                                8, // stencil bits
                                framebufferInfo);

    auto result = target->isValid() ? SkSurface::MakeFromBackendRenderTarget(_ctx.get(),
                                                        *target.get(),
                                                        kBottomLeft_GrSurfaceOrigin,
                                                        colorType,
                                                        nullptr,
                                                        nullptr)
                                    : nullptr;  

    if (!result)
        throw std::runtime_error("failed to make Skia surface");

    return std::move(result);
}

