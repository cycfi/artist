/*
 * Copyright 2022 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
// Important to put this first because webgpu_cpp.h and X.h don't get along.
// Include these first, before X11 defines None, Success, Status etc.
#include "dawn/native/DawnNative.h"  // NO_G3_REWRITE
#include "webgpu/webgpu_cpp.h"       // NO_G3_REWRITE

#include "tools/window/unix/GraphiteDawnVulkanWindowContext_unix.h"

#include "tools/window/GraphiteDawnWindowContext.h"
#include "tools/window/unix/XlibWindowInfo.h"

using skwindow::XlibWindowInfo;
using skwindow::DisplayParams;
using skwindow::internal::GraphiteDawnWindowContext;

namespace {

class GraphiteDawnVulkanWindowContext_unix : public GraphiteDawnWindowContext {
public:
    GraphiteDawnVulkanWindowContext_unix(const XlibWindowInfo& info,
                                         std::unique_ptr<const DisplayParams> params);

    ~GraphiteDawnVulkanWindowContext_unix() override;

    bool onInitializeContext() override;
    void onDestroyContext() override;
    void resize(int w, int h) override;

private:
    Display*     fDisplay;
    XWindow      fWindow;
};

GraphiteDawnVulkanWindowContext_unix::GraphiteDawnVulkanWindowContext_unix(
        const XlibWindowInfo& info, std::unique_ptr<const DisplayParams> params)
        : GraphiteDawnWindowContext(std::move(params), wgpu::TextureFormat::BGRA8Unorm)
        , fDisplay(info.fDisplay)
        , fWindow(info.fWindow) {
    XWindow root;
    int x, y;
    unsigned int border_width, depth;
    unsigned int width, height;
    XGetGeometry(fDisplay, fWindow, &root, &x, &y, &width, &height, &border_width, &depth);
    this->initializeContext(width, height);
}

GraphiteDawnVulkanWindowContext_unix::~GraphiteDawnVulkanWindowContext_unix() {
    this->destroyContext();
}

bool GraphiteDawnVulkanWindowContext_unix::onInitializeContext() {
    SkASSERT(!!fWindow);

    auto device = this->createDevice(wgpu::BackendType::Vulkan);
    if (!device) {
        SkASSERT(device);
        return false;
    }

    wgpu::SurfaceSourceXlibWindow surfaceChainedDesc;
    surfaceChainedDesc.display = fDisplay;
    surfaceChainedDesc.window = fWindow;

    wgpu::SurfaceDescriptor surfaceDesc;
    surfaceDesc.nextInChain = &surfaceChainedDesc;

    auto surface = wgpu::Instance(fInstance->Get()).CreateSurface(&surfaceDesc);
    if (!surface) {
        SkASSERT(false);
        return false;
    }

    fDevice = std::move(device);
    fSurface = std::move(surface);
    configureSurface();

    return true;
}

void GraphiteDawnVulkanWindowContext_unix::onDestroyContext() {}

void GraphiteDawnVulkanWindowContext_unix::resize(int w, int h) {
    configureSurface();
}

}  // anonymous namespace

namespace skwindow {

std::unique_ptr<WindowContext> MakeGraphiteDawnVulkanForXlib(
        const XlibWindowInfo& info, std::unique_ptr<const DisplayParams> params) {
    std::unique_ptr<WindowContext> ctx(
            new GraphiteDawnVulkanWindowContext_unix(info, std::move(params)));
    if (!ctx->isValid()) {
        return nullptr;
    }
    return ctx;
}

}  // namespace skwindow
