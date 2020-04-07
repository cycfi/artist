/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include "../../app.hpp"

using namespace cycfi::artist;
float elapsed_ = 0;  // rendering elapsed time

///// --->

#include <math.h>

float boxv[][3] = {
        { -0.5, -0.5, -0.5 },
        {  0.5, -0.5, -0.5 },
        {  0.5,  0.5, -0.5 },
        { -0.5,  0.5, -0.5 },
        { -0.5, -0.5,  0.5 },
        {  0.5, -0.5,  0.5 },
        {  0.5,  0.5,  0.5 },
        { -0.5,  0.5,  0.5 }
};
#define ALPHA 0.5

static float ang = 30.;

static gboolean
expose(GtkWidget *da, GdkEventExpose *event, gpointer user_data)
{
   GdkGLContext*  glcontext = gtk_widget_get_gl_context(da);
   GdkGLDrawable* gldrawable = gtk_widget_get_gl_drawable(da);

   // g_print(" :: expose\n");

   if (!gdk_gl_drawable_gl_begin(gldrawable, glcontext))
      g_assert_not_reached();

   // draw in here
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
   glPushMatrix();

   glRotatef(ang, 1, 0, 1);
   // glRotatef(ang, 0, 1, 0);
   // glRotatef(ang, 0, 0, 1);

   glShadeModel(GL_FLAT);

   glBegin(GL_LINES);
   glColor3f(1., 0., 0.);
   glVertex3f(0., 0., 0.);
   glVertex3f(1., 0., 0.);
   glEnd();

   glBegin(GL_LINES);
   glColor3f(0., 1., 0.);
   glVertex3f(0., 0., 0.);
   glVertex3f(0., 1., 0.);
   glEnd();

   glBegin(GL_LINES);
   glColor3f(0., 0., 1.);
   glVertex3f(0., 0., 0.);
   glVertex3f(0., 0., 1.);
   glEnd();

   glBegin(GL_LINES);
   glColor3f(1., 1., 1.);
   glVertex3fv(boxv[0]);
   glVertex3fv(boxv[1]);

   glVertex3fv(boxv[1]);
   glVertex3fv(boxv[2]);

   glVertex3fv(boxv[2]);
   glVertex3fv(boxv[3]);

   glVertex3fv(boxv[3]);
   glVertex3fv(boxv[0]);

   glVertex3fv(boxv[4]);
   glVertex3fv(boxv[5]);

   glVertex3fv(boxv[5]);
   glVertex3fv(boxv[6]);

   glVertex3fv(boxv[6]);
   glVertex3fv(boxv[7]);

   glVertex3fv(boxv[7]);
   glVertex3fv(boxv[4]);

   glVertex3fv(boxv[0]);
   glVertex3fv(boxv[4]);

   glVertex3fv(boxv[1]);
   glVertex3fv(boxv[5]);

   glVertex3fv(boxv[2]);
   glVertex3fv(boxv[6]);

   glVertex3fv(boxv[3]);
   glVertex3fv(boxv[7]);
   glEnd();

   glPopMatrix();

   if (gdk_gl_drawable_is_double_buffered(gldrawable))
      gdk_gl_drawable_swap_buffers(gldrawable);
   else
      glFlush();

   gdk_gl_drawable_gl_end(gldrawable);

   return true;
}

static gboolean
configure(GtkWidget* da, GdkEventConfigure* event, gpointer user_data)
{
   GdkGLContext*  glcontext = gtk_widget_get_gl_context(da);
   GdkGLDrawable* gldrawable = gtk_widget_get_gl_drawable(da);

   if (!gdk_gl_drawable_gl_begin(gldrawable, glcontext))
      g_assert_not_reached();

   glLoadIdentity();
   glViewport(0, 0, da->allocation.width, da->allocation.height);
   glOrtho(-10,10,-10,10,-20050,10000);
   glEnable(GL_BLEND);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

   glScalef(10., 10., 10.);
   gdk_gl_drawable_gl_end(gldrawable);
   return true;
}

static gboolean
rotate(gpointer user_data)
{
   GtkWidget *da = GTK_WIDGET(user_data);

   ang++;

   gdk_window_invalidate_rect(da->window, &da->allocation, FALSE);
   gdk_window_process_updates(da->window, FALSE);

   return true;
}

int run_app(
   int argc
 , char const* argv[]
 , extent window_size
 , color background_color
 , bool animate
)
{
   gtk_init(&argc, const_cast<char***>(&argv));
   gtk_gl_init(&argc, const_cast<char***>(&argv));

   auto* window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
   gtk_window_set_default_size(
      GTK_WINDOW(window), window_size.x, window_size.y);
   auto* da = gtk_drawing_area_new();

   gtk_container_add(GTK_CONTAINER(window), da);
   g_signal_connect_swapped(
      window, "destroy", G_CALLBACK(gtk_main_quit), nullptr);
   gtk_widget_set_events(da, GDK_EXPOSURE_MASK);

   gtk_widget_show(window);

   // prepare GL
   auto* glconfig = gdk_gl_config_new_by_mode(GdkGLConfigMode(
           GDK_GL_MODE_RGB |
           GDK_GL_MODE_DEPTH |
           GDK_GL_MODE_DOUBLE));

   if (!glconfig)
      g_assert_not_reached();

   if (!gtk_widget_set_gl_capability(
      da, glconfig, nullptr, true, GDK_GL_RGBA_TYPE))
      g_assert_not_reached();

   g_signal_connect(
      da, "configure-event", G_CALLBACK(configure), nullptr);
   g_signal_connect(
      da, "expose-event", G_CALLBACK(expose), nullptr);

   gtk_widget_show_all(window);
   g_timeout_add(1000 / 60, rotate, da);

   gtk_main();
   return 0;
}

void stop_app()
{
   gtk_main_quit();
}

void print_elapsed(canvas& cnv, point br)
{
   static font open_sans = font_descr{ "Open Sans", 12 };
   static int i = 0;
   static float t_elapsed = 0;
   static float c_elapsed = 0;

   if (++i == 30)
   {
      i = 0;
      c_elapsed = t_elapsed / 30;
      t_elapsed = 0;
   }
   else
   {
      t_elapsed += elapsed_;
   }

   if (c_elapsed)
   {
      cnv.fill_style(rgba(220, 220, 220, 200));
      cnv.font(open_sans);
      cnv.text_align(cnv.right | cnv.bottom);
      cnv.fill_text(std::to_string(1 / c_elapsed) + " fps", { br.x, br.y });
   }
}


