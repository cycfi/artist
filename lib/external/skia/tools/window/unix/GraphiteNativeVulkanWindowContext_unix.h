/*
 * Copyright 2024 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GraphiteNativeVulkanWindowContext_unix_DEFINED
#define GraphiteNativeVulkanWindowContext_unix_DEFINED

#include <memory>

namespace skwindow {
class WindowContext;
class DisplayParams;
struct XlibWindowInfo;

std::unique_ptr<WindowContext> MakeGraphiteNativeVulkanForXlib(
        const XlibWindowInfo&, std::unique_ptr<const DisplayParams>);
}  // namespace skwindow

#endif
