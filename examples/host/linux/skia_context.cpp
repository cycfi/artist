#include "skia_context.h"

#include "ganesh/gl/GrGLDirectContext.h"
#include "ganesh/GrBackendSurface.h"
#include "ganesh/SkSurfaceGanesh.h"
#include "ganesh/gl/GrGLBackendSurface.h"

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

sk_sp<SkSurface> GlSkiaContext::makeSurface(int width, int height)
{
    GrGLFramebufferInfo framebufferInfo;
    framebufferInfo.fFBOID = 0; // assume default framebuffer
    framebufferInfo.fFormat = GL_RGBA8;

    SkColorType colorType = kRGBA_8888_SkColorType;
    auto target = GrBackendRenderTargets::MakeGL(
                                width,
                                height,
                                1, // sample count
                                8, // stencil bits
                                framebufferInfo);

    auto result = SkSurfaces::WrapBackendRenderTarget(_ctx.get(),
                                                      target,
                                                      kBottomLeft_GrSurfaceOrigin,
                                                      colorType,
                                                      nullptr,
                                                      nullptr);

    if (!result)
        throw std::runtime_error("failed to make Skia surface");

    return std::move(result);
}

void GlSkiaContext::flush(const sk_sp<SkSurface> &surf) const
{
    _ctx->flushAndSubmit(surf.get());
}
