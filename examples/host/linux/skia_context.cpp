#include "skia_context.h"
#include "SkColorSpace.h"

#include <GL/gl.h>

void GlSkiaContext::init(GrGLGetProc gl_get_proc)
{
    auto _xface = GrGLMakeNativeInterface();
    if (_xface == nullptr) {
        //backup plan. see https://gist.github.com/ad8e/dd150b775ae6aa4d5cf1a092e4713add?permalink_comment_id=4680136#gistcomment-4680136
        _xface = GrGLMakeAssembledInterface(
            nullptr, gl_get_proc);
    }

    _ctx = GrDirectContext::MakeGL(_xface);
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

