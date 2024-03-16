/*=============================================================================
   Copyright (c) 2016-2023 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <gtk/gtk.h>
#include <GL/gl.h>
#include <GL/glx.h>

#include "GrDirectContext.h"
#include "gl/GrGLInterface.h"
#include "SkImage.h"
#include "SkColorSpace.h"
#include "SkCanvas.h"
#include "SkSurface.h"

namespace
{
   sk_sp<const GrGLInterface> gl_interface;
   sk_sp<GrDirectContext>     gr_context;
   sk_sp<SkSurface>           skia_surface;

   void realize(GtkGLArea* area, gpointer user_data)
   {
      gtk_gl_area_make_current (area);
      if (gtk_gl_area_get_error (area) != NULL)
         return;

      glClearColor(0, 0, 0, 1);
      glClear(GL_COLOR_BUFFER_BIT);
      gl_interface = GrGLMakeNativeInterface();
      gr_context = GrDirectContext::MakeGL(gl_interface);
   }

   gboolean render(GtkGLArea* area, GdkGLContext* context, gpointer user_data)
   {
      if (!skia_surface)
      {
         GrGLint buffer;
         glGetIntegerv(GL_FRAMEBUFFER_BINDING, &buffer);
         GrGLFramebufferInfo info;
         info.fFBOID = (GrGLuint) buffer;
         SkColorType colorType = kRGBA_8888_SkColorType;

         info.fFormat = GL_RGBA8;
         GrBackendRenderTarget target(640, 480, 0, 8, info);

         skia_surface =
            SkSurface::MakeFromBackendRenderTarget(
               gr_context.get(), target,
               kBottomLeft_GrSurfaceOrigin, colorType, nullptr, nullptr
            );

         if (!skia_surface)
            throw std::runtime_error("Error: SkSurface::MakeRenderTarget returned null");
      }

      SkCanvas* gpu_canvas = skia_surface->getCanvas();
      gpu_canvas->save();

      // Drawing here...

      gpu_canvas->restore();
      skia_surface->flush();

      return true;
   }

   gboolean animate(gpointer user_data)
   {
      GtkWidget* da = GTK_WIDGET(user_data);
      gtk_widget_queue_draw(da);
      return true;
   }

   void activate(GtkApplication* app, gpointer user_data)
   {
      auto* window = gtk_application_window_new(app);
      gtk_window_set_title(GTK_WINDOW(window), "Drawing Area");

      // create a GtkGLArea instance
      auto* gl_area = gtk_gl_area_new();
      gtk_container_add(GTK_CONTAINER(window), gl_area);

      g_signal_connect(gl_area, "render", G_CALLBACK(render), user_data);
      g_signal_connect(gl_area, "realize", G_CALLBACK(realize), user_data);

      gtk_window_resize(GTK_WINDOW(window), 640, 480);
      gtk_widget_show_all(window);

      auto w = gtk_widget_get_window(GTK_WIDGET(window));
   }
}

int main(int argc, char const* argv[])
{
   auto* app = gtk_application_new("org.gtk-skia.example", G_APPLICATION_FLAGS_NONE);
   g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
   int status = g_application_run(G_APPLICATION(app), argc, const_cast<char**>(argv));
   g_object_unref(app);

   return status;
}

#include <artist/resources.hpp>
#include <filesystem>
namespace fs = std::filesystem;

float elapsed_ = 0;  // rendering elapsed time

namespace cycfi::artist
{
   void init_paths()
   {
      add_search_path(fs::current_path() / "resources");
      add_search_path(fs::current_path() / "resources/fonts");
      add_search_path(fs::current_path() / "resources/images");
   }

   // This is declared in font.hpp
   fs::path get_user_fonts_directory()
   {
      return fs::path(fs::current_path() / "resources/fonts");
   }
}