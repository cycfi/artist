/*
 * Copyright 2023 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef DisplayParams_DEFINED
#define DisplayParams_DEFINED

#include "include/core/SkColorSpace.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkSurfaceProps.h"
#include "include/gpu/ganesh/GrContextOptions.h"
#include "src/base/SkMathPriv.h"

#include <memory>

namespace skwindow {

struct GraphiteTestOptions;

// DisplayParams should be treated as a immutable object once created.
class DisplayParams {
public:
    DisplayParams()
        : fColorType(kN32_SkColorType)
        , fColorSpace(nullptr)
        , fMSAASampleCount(1)
        , fSurfaceProps(0, kRGB_H_SkPixelGeometry)
        , fDisableVsync(false)
        , fDelayDrawableAcquisition(false)
        , fCreateProtectedNativeBackend(false)
    {}

    DisplayParams(const DisplayParams* other) {
        SkASSERT(other);
        fColorType = other->fColorType;
        fColorSpace = other->fColorSpace;
        fMSAASampleCount = other->fMSAASampleCount;
        fGrContextOptions = other->fGrContextOptions;
        fSurfaceProps = other->fSurfaceProps;
        fDisableVsync = other->fDisableVsync;
        fDelayDrawableAcquisition = other->fDelayDrawableAcquisition;
        fCreateProtectedNativeBackend = other->fCreateProtectedNativeBackend;
    }
    DisplayParams(const DisplayParams& other) : DisplayParams(&other) {}

    virtual ~DisplayParams() = default;

    virtual std::unique_ptr<DisplayParams> clone() const {
        return std::make_unique<DisplayParams>(*this);
    }

    SkColorType colorType() const { return fColorType; }
    sk_sp<SkColorSpace> colorSpace() const { return fColorSpace; }
    int msaaSampleCount() const { return fMSAASampleCount; }
    const GrContextOptions& grContextOptions() const { return fGrContextOptions; }
    const SkSurfaceProps& surfaceProps() const { return fSurfaceProps; }
    bool disableVsync() const { return fDisableVsync; }
    bool delayDrawableAcquisition() const { return fDelayDrawableAcquisition; }
    bool createProtectedNativeBackend() const { return fCreateProtectedNativeBackend; }
    virtual const GraphiteTestOptions* graphiteTestOptions() const { return nullptr; }

private:
    friend class DisplayParamsBuilder;

    SkColorType            fColorType;
    sk_sp<SkColorSpace>    fColorSpace;
    int                    fMSAASampleCount;
    GrContextOptions fGrContextOptions;
    SkSurfaceProps         fSurfaceProps;
    bool                   fDisableVsync;
    bool                   fDelayDrawableAcquisition;
    bool                   fCreateProtectedNativeBackend = false;
};

class DisplayParamsBuilder {
public:
    DisplayParamsBuilder() : fDisplayParams(std::make_unique<DisplayParams>()) {}

    // Call clone() in case other is a subclass of DisplayParams
    DisplayParamsBuilder(const DisplayParams* other) : fDisplayParams(other->clone()) {}

    DisplayParamsBuilder& colorType(SkColorType colorType) {
        fDisplayParams->fColorType = colorType;
        return *this;
    }

    DisplayParamsBuilder& colorSpace(const sk_sp<SkColorSpace>& colorSpace) {
        fDisplayParams->fColorSpace = colorSpace;
        return *this;
    }

    DisplayParamsBuilder& msaaSampleCount(int MSAASampleCount) {
        fDisplayParams->fMSAASampleCount = MSAASampleCount;
        return *this;
    }

    DisplayParamsBuilder& roundUpMSAA() {
        // SkNextPow2 is undefined for 0, so handle that ourselves.
        if (fDisplayParams->fMSAASampleCount <= 1) {
            fDisplayParams->fMSAASampleCount = 1;
        } else {
            fDisplayParams->fMSAASampleCount = SkNextPow2(fDisplayParams->fMSAASampleCount);
        }
        return *this;
    }

    DisplayParamsBuilder& grContextOptions(const GrContextOptions& grContextOptions) {
        fDisplayParams->fGrContextOptions = grContextOptions;
        return *this;
    }

    DisplayParamsBuilder& surfaceProps(const SkSurfaceProps& surfaceProps) {
        fDisplayParams->fSurfaceProps = surfaceProps;
        return *this;
    }

    DisplayParamsBuilder& disableVsync(bool disableVsync) {
        fDisplayParams->fDisableVsync = disableVsync;
        return *this;
    }

    DisplayParamsBuilder& delayDrawableAcquisition(bool delayDrawableAcquisition) {
        fDisplayParams->fDelayDrawableAcquisition = delayDrawableAcquisition;
        return *this;
    }

    DisplayParamsBuilder& createProtectedNativeBackend(bool createProtectedNativeBackend) {
        fDisplayParams->fCreateProtectedNativeBackend = createProtectedNativeBackend;
        return *this;
    }

    std::unique_ptr<DisplayParams> build() { return std::move(fDisplayParams); }

protected:
    DisplayParamsBuilder(std::unique_ptr<DisplayParams> other) : fDisplayParams(std::move(other)) {}

    std::unique_ptr<DisplayParams> fDisplayParams;
};

}  // namespace skwindow

#endif
