/*
 * Copyright 2024 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GaneshGLWindowContext_mac_DEFINED
#define GaneshGLWindowContext_mac_DEFINED

#include <memory>

namespace skwindow {
class WindowContext;
class DisplayParams;
struct MacWindowInfo;

std::unique_ptr<WindowContext> MakeGaneshGLForMac(const MacWindowInfo&,
                                                  std::unique_ptr<const DisplayParams>);
}  // namespace skwindow

#endif
