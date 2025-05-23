// -*- c++ -*-
// Generated by gmmproc 2.45.3 -- DO NOT MODIFY!
#ifndef _GTKMM_STYLE_H
#define _GTKMM_STYLE_H

#include <ytkmm/ytkmmconfig.h>


#include <glibmm/ustring.h>
#include <sigc++/sigc++.h>

/* $Id: style.hg,v 1.11 2006/11/23 14:47:48 murrayc Exp $ */

/* Copyright (C) 1998-2002 The gtkmm Development Team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

// This is for including the config header before any code (such as
// the #ifndef GTKMM_DISABLE_DEPRECATED in deprecated classes) is generated:


#include <pangomm/fontdescription.h>
#include <pangomm/layout.h>

#include <ydkmm/gc.h>
#include <ydkmm/pixmap.h>
#include <ydkmm/types.h>
#include <ydkmm/window.h>

#include <ytkmm/object.h>
#include <ytkmm/enums.h>
#include <ytkmm/iconsource.h>
#include <ytkmm/iconset.h>
#include <ytkmm/ytkmmconfig.h>


#ifndef DOXYGEN_SHOULD_SKIP_THIS
typedef struct _GtkStyle GtkStyle;
typedef struct _GtkStyleClass GtkStyleClass;
#endif /* DOXYGEN_SHOULD_SKIP_THIS */


#ifndef DOXYGEN_SHOULD_SKIP_THIS
namespace Gtk
{ class Style_Class; } // namespace Gtk
#endif //DOXYGEN_SHOULD_SKIP_THIS

namespace Gtk
{

class Widget;
class RcStyle;


class Style : public Glib::Object
{
  
#ifndef DOXYGEN_SHOULD_SKIP_THIS

public:
  typedef Style CppObjectType;
  typedef Style_Class CppClassType;
  typedef GtkStyle BaseObjectType;
  typedef GtkStyleClass BaseClassType;

private:  friend class Style_Class;
  static CppClassType style_class_;

private:
  // noncopyable
  Style(const Style&);
  Style& operator=(const Style&);

protected:
  explicit Style(const Glib::ConstructParams& construct_params);
  explicit Style(GtkStyle* castitem);

#endif /* DOXYGEN_SHOULD_SKIP_THIS */

public:
  virtual ~Style();

  /** Get the GType for this class, for use with the underlying GObject type system.
   */
  static GType get_type()      G_GNUC_CONST;

#ifndef DOXYGEN_SHOULD_SKIP_THIS


  static GType get_base_type() G_GNUC_CONST;
#endif

  ///Provides access to the underlying C GObject.
  GtkStyle*       gobj()       { return reinterpret_cast<GtkStyle*>(gobject_); }

  ///Provides access to the underlying C GObject.
  const GtkStyle* gobj() const { return reinterpret_cast<GtkStyle*>(gobject_); }

  ///Provides access to the underlying C instance. The caller is responsible for unrefing it. Use when directly setting fields in structs.
  GtkStyle* gobj_copy();

private:

  
protected:
  Style();

public:
  
  static Glib::RefPtr<Style> create();

  
  void set_fg(Gtk::StateType state, const Gdk::Color& color);
  Gdk::Color get_fg(Gtk::StateType state) const;

  void set_bg(Gtk::StateType state, const Gdk::Color& color);
  Gdk::Color get_bg(Gtk::StateType state) const;

  void set_light(Gtk::StateType state, const Gdk::Color& color);
  Gdk::Color get_light(Gtk::StateType state) const;

  void set_dark(Gtk::StateType state, const Gdk::Color& color);
  Gdk::Color get_dark(Gtk::StateType state) const;

  void set_mid(Gtk::StateType state, const Gdk::Color& color);
  Gdk::Color get_mid(Gtk::StateType state) const;

  void set_text(Gtk::StateType state, const Gdk::Color& color);
  Gdk::Color get_text(Gtk::StateType state) const;

  void set_base(Gtk::StateType state, const Gdk::Color& color);
  Gdk::Color get_base(Gtk::StateType state) const;

  void set_text_aa(Gtk::StateType state, const Gdk::Color& color);
  Gdk::Color get_text_aa(Gtk::StateType state) const;


   void set_black(const Gdk::Color& value);
    Gdk::Color get_black() const;
    void set_white(const Gdk::Color& value);
    Gdk::Color get_white() const;
 
  void set_font(const Pango::FontDescription& font_desc);

  // These are only available after being attached to a window.
  Pango::FontDescription get_font() const;

  void set_xthickness(int xthickness);
   int get_xthickness() const;
 
  void set_ythickness(int ythickness);
   int get_ythickness() const;
 
  Glib::RefPtr<Gdk::GC> get_fg_gc(Gtk::StateType state_type);
  Glib::RefPtr<const Gdk::GC> get_fg_gc(Gtk::StateType state_type) const;

  Glib::RefPtr<Gdk::GC> get_bg_gc(Gtk::StateType state_type);
  Glib::RefPtr<const Gdk::GC> get_bg_gc(Gtk::StateType state_type) const;

  Glib::RefPtr<Gdk::GC> get_light_gc(Gtk::StateType state_type);
  Glib::RefPtr<const Gdk::GC> get_light_gc(Gtk::StateType state_type) const;

  Glib::RefPtr<Gdk::GC> get_dark_gc(Gtk::StateType state_type);
  Glib::RefPtr<const Gdk::GC> get_dark_gc(Gtk::StateType state_type) const;

  Glib::RefPtr<Gdk::GC> get_mid_gc ( Gtk::StateType state_type);
  Glib::RefPtr<const Gdk::GC> get_mid_gc ( Gtk::StateType state_type) const;

  Glib::RefPtr<Gdk::GC> get_text_gc(Gtk::StateType state_type);
  Glib::RefPtr<const Gdk::GC> get_text_gc(Gtk::StateType state_type) const;

  Glib::RefPtr<Gdk::GC> get_base_gc(Gtk::StateType state_type);
  Glib::RefPtr<const Gdk::GC> get_base_gc(Gtk::StateType state_type) const;

   Glib::RefPtr<Gdk::GC> get_black_gc();
  Glib::RefPtr<const Gdk::GC> get_black_gc() const;
    Glib::RefPtr<Gdk::GC> get_white_gc();
  Glib::RefPtr<const Gdk::GC> get_white_gc() const;
 
  #ifndef GTKMM_DISABLE_DEPRECATED

  /* @deprecated use get_bg() instead.
   */
  Gdk::Color get_background(Gtk::StateType state_type) const;

  /*@deprecated use get_bg() instead.
   */
  Glib::RefPtr<Gdk::Pixmap> get_background_pixmap(Gtk::StateType state_type);

  /** @deprecated Use set_bg_pixmap(Gtk::StateType state_type, const Glib::RefPtr<const Gdk::Pixmap>& pixmap)
   */
  void set_bg_pixmap(Gtk::StateType state_type, const Glib::RefPtr<Gdk::Pixmap>& pixmap);
  #endif // GTKMM_DISABLE_DEPRECATED


  void set_bg_pixmap(Gtk::StateType state_type, const Glib::RefPtr<const Gdk::Pixmap>& pixmap);

  Glib::RefPtr<Gdk::Pixmap>       get_bg_pixmap(Gtk::StateType state_type);
  Glib::RefPtr<const Gdk::Pixmap> get_bg_pixmap(Gtk::StateType state_type) const;

  
  /** Draws an arrow in the given rectangle on @a window using the given 
   * parameters. @a arrow_type determines the direction of the arrow.
   * 
   * @param window A Gdk::Window.
   * @param state_type A state.
   * @param shadow_type The type of shadow to draw.
   * @param area Clip rectangle, or <tt>0</tt> if the
   * output should not be clipped.
   * @param widget The widget.
   * @param detail A style detail.
   * @param arrow_type The type of arrow to draw.
   * @param fill <tt>true</tt> if the arrow tip should be filled.
   * @param x X origin of the rectangle to draw the arrow in.
   * @param y Y origin of the rectangle to draw the arrow in.
   * @param width Width of the rectangle to draw the arrow in.
   * @param height Height of the rectangle to draw the arrow in.
   */
  void paint_arrow(const Glib::RefPtr<Gdk::Window>& window, Gtk::StateType state_type, ShadowType shadow_type, const Gdk::Rectangle& area, Widget& widget, const Glib::ustring& detail, ArrowType arrow_type, bool fill, int x, int y, int width, int height) const;

  
  /** Draws a box on @a window with the given parameters.
   * 
   * @param window A Gdk::Window.
   * @param state_type A state.
   * @param shadow_type The type of shadow to draw.
   * @param area Clip rectangle, or <tt>0</tt> if the
   * output should not be clipped.
   * @param widget The widget.
   * @param detail A style detail.
   * @param x X origin of the box.
   * @param y Y origin of the box.
   * @param width The width of the box.
   * @param height The height of the box.
   */
  void paint_box(const Glib::RefPtr<Gdk::Window>& window, Gtk::StateType state_type, ShadowType shadow_type, const Gdk::Rectangle& area, Widget& widget, const Glib::ustring& detail, int x, int y, int width, int height) const;

  
  /** Draws a box in @a window using the given style and state and shadow type, 
   * leaving a gap in one side.
   * 
   * @param window A Gdk::Window.
   * @param state_type A state.
   * @param shadow_type Type of shadow to draw.
   * @param area Clip rectangle, or <tt>0</tt> if the
   * output should not be clipped.
   * @param widget The widget.
   * @param detail A style detail.
   * @param x X origin of the rectangle.
   * @param y Y origin of the rectangle.
   * @param width Width of the rectangle.
   * @param height Width of the rectangle.
   * @param gap_side Side in which to leave the gap.
   * @param gap_x Starting position of the gap.
   * @param gap_width Width of the gap.
   */
  void paint_box_gap(const Glib::RefPtr<Gdk::Window>& window, Gtk::StateType state_type, ShadowType shadow_type, const Gdk::Rectangle& area, Widget& widget, const Glib::ustring& detail, int x, int y, int width, int height, PositionType gap_side, int gap_x, int gap_width) const;

  
  /** Draws a check button indicator in the given rectangle on @a window with 
   * the given parameters.
   * 
   * @param window A Gdk::Window.
   * @param state_type A state.
   * @param shadow_type The type of shadow to draw.
   * @param area Clip rectangle, or <tt>0</tt> if the
   * output should not be clipped.
   * @param widget The widget.
   * @param detail A style detail.
   * @param x X origin of the rectangle to draw the check in.
   * @param y Y origin of the rectangle to draw the check in.
   * @param width The width of the rectangle to draw the check in.
   * @param height The height of the rectangle to draw the check in.
   */
  void paint_check(const Glib::RefPtr<Gdk::Window>& window, Gtk::StateType state_type, ShadowType shadow_type, const Gdk::Rectangle& area, Widget& widget, const Glib::ustring& detail, int x, int y, int width, int height) const;

  
  /** Draws a diamond in the given rectangle on @a window using the given
   * parameters.
   * 
   * @param window A Gdk::Window.
   * @param state_type A state.
   * @param shadow_type The type of shadow to draw.
   * @param area Clip rectangle, or <tt>0</tt> if the
   * output should not be clipped.
   * @param widget The widget.
   * @param detail A style detail.
   * @param x X origin of the rectangle to draw the diamond in.
   * @param y Y origin of the rectangle to draw the diamond in.
   * @param width Width of the rectangle to draw the diamond in.
   * @param height Height of the rectangle to draw the diamond in.
   */
  void paint_diamond(const Glib::RefPtr<Gdk::Window>& window, Gtk::StateType state_type, ShadowType shadow_type, const Gdk::Rectangle& area, Widget& widget, const Glib::ustring& detail, int x, int y, int width, int height) const;

  
  /** Draws an extension, i.e.\ a notebook tab.
   * 
   * @param window A Gdk::Window.
   * @param state_type A state.
   * @param shadow_type Type of shadow to draw.
   * @param area Clip rectangle, or <tt>0</tt> if the
   * output should not be clipped.
   * @param widget The widget.
   * @param detail A style detail.
   * @param x X origin of the extension.
   * @param y Y origin of the extension.
   * @param width Width of the extension.
   * @param height Width of the extension.
   * @param gap_side The side on to which the extension is attached.
   */
  void paint_extension(const Glib::RefPtr<Gdk::Window>& window, Gtk::StateType state_type, ShadowType shadow_type, const Gdk::Rectangle& area, Widget& widget, const Glib::ustring& detail, int x, int y, int width, int height, PositionType gap_side) const;

  
  /** Draws a flat box on @a window with the given parameters.
   * 
   * @param window A Gdk::Window.
   * @param state_type A state.
   * @param shadow_type The type of shadow to draw.
   * @param area Clip rectangle, or <tt>0</tt> if the
   * output should not be clipped.
   * @param widget The widget.
   * @param detail A style detail.
   * @param x X origin of the box.
   * @param y Y origin of the box.
   * @param width The width of the box.
   * @param height The height of the box.
   */
  void paint_flat_box(const Glib::RefPtr<Gdk::Window>& window, Gtk::StateType state_type, ShadowType shadow_type, const Gdk::Rectangle& area, Widget& widget, const Glib::ustring& detail, int x, int y, int width, int height) const;

  
  /** Draws a focus indicator around the given rectangle on @a window using the
   * given style.
   * 
   * @param window A Gdk::Window.
   * @param state_type A state.
   * @param area Clip rectangle, or <tt>0</tt> if the
   * output should not be clipped.
   * @param widget The widget.
   * @param detail A style detail.
   * @param x The x origin of the rectangle around which to draw a focus indicator.
   * @param y The y origin of the rectangle around which to draw a focus indicator.
   * @param width The width of the rectangle around which to draw a focus indicator.
   * @param height The height of the rectangle around which to draw a focus indicator.
   */
  void paint_focus(const Glib::RefPtr<Gdk::Window>& window, Gtk::StateType state_type, const Gdk::Rectangle& area, Widget& widget, const Glib::ustring& detail, int x, int y, int width, int height) const;

  
  /** Draws a handle as used in Gtk::HandleBox and Gtk::Paned.
   * 
   * @param window A Gdk::Window.
   * @param state_type A state.
   * @param shadow_type Type of shadow to draw.
   * @param area Clip rectangle, or <tt>0</tt> if the
   * output should not be clipped.
   * @param widget The widget.
   * @param detail A style detail.
   * @param x X origin of the handle.
   * @param y Y origin of the handle.
   * @param width With of the handle.
   * @param height Height of the handle.
   * @param orientation The orientation of the handle.
   */
  void paint_handle(const Glib::RefPtr<Gdk::Window>& window, Gtk::StateType state_type, ShadowType shadow_type, const Gdk::Rectangle& area, Widget& widget, const Glib::ustring& detail, int x, int y, int width, int height, Orientation orientation) const;

  
  /** Draws a horizontal line from ( @a x1, @a y) to ( @a x2, @a y) in @a window
   * using the given style and state.
   * 
   * @param window A Gdk::Window.
   * @param state_type A state.
   * @param area Rectangle to which the output is clipped, or <tt>0</tt> if the
   * output should not be clipped.
   * @param widget The widget.
   * @param detail A style detail.
   * @param x1 The starting x coordinate.
   * @param x2 The ending x coordinate.
   * @param y The y coordinate.
   */
  void paint_hline(const Glib::RefPtr<Gdk::Window>& window, Gtk::StateType state_type, const Gdk::Rectangle& area, Widget& widget, const Glib::ustring& detail, int x1, int x2, int y) const;

  
  /** Draws a radio button indicator in the given rectangle on @a window with 
   * the given parameters.
   * 
   * @param window A Gdk::Window.
   * @param state_type A state.
   * @param shadow_type The type of shadow to draw.
   * @param area Clip rectangle, or <tt>0</tt> if the
   * output should not be clipped.
   * @param widget The widget.
   * @param detail A style detail.
   * @param x X origin of the rectangle to draw the option in.
   * @param y Y origin of the rectangle to draw the option in.
   * @param width The width of the rectangle to draw the option in.
   * @param height The height of the rectangle to draw the option in.
   */
  void paint_option(const Glib::RefPtr<Gdk::Window>& window, Gtk::StateType state_type, ShadowType shadow_type, const Gdk::Rectangle& area, Widget& widget, const Glib::ustring& detail, int x, int y, int width, int height) const;

  void paint_polygon(
                  const Glib::RefPtr<Gdk::Window>&      window,
                  Gtk::StateType                        state_type,
                  ShadowType                            shadow_type,
                  const Gdk::Rectangle&                 area,
                  Widget&                               widget,
                  const Glib::ustring&                  detail,
                  const Glib::ArrayHandle<Gdk::Point>&  points,
                  bool                                  fill) const;

  
  /** Draws a shadow around the given rectangle in @a window 
   * using the given style and state and shadow type.
   * 
   * @param window A Gdk::Window.
   * @param state_type A state.
   * @param shadow_type Type of shadow to draw.
   * @param area Clip rectangle or <tt>0</tt> if the
   * output should not be clipped.
   * @param widget The widget.
   * @param detail A style detail.
   * @param x X origin of the rectangle.
   * @param y Y origin of the rectangle.
   * @param width Width of the rectangle.
   * @param height Width of the rectangle.
   */
  void paint_shadow(const Glib::RefPtr<Gdk::Window>& window, Gtk::StateType state_type, ShadowType shadow_type, const Gdk::Rectangle& area, Widget& widget, const Glib::ustring& detail, int x, int y, int width, int height) const;

  
  /** Draws a shadow around the given rectangle in @a window 
   * using the given style and state and shadow type, leaving a 
   * gap in one side.
   * 
   * @param window A Gdk::Window.
   * @param state_type A state.
   * @param shadow_type Type of shadow to draw.
   * @param area Clip rectangle, or <tt>0</tt> if the
   * output should not be clipped.
   * @param widget The widget.
   * @param detail A style detail.
   * @param x X origin of the rectangle.
   * @param y Y origin of the rectangle.
   * @param width Width of the rectangle.
   * @param height Width of the rectangle.
   * @param gap_side Side in which to leave the gap.
   * @param gap_x Starting position of the gap.
   * @param gap_width Width of the gap.
   */
  void paint_shadow_gap(const Glib::RefPtr<Gdk::Window>& window, Gtk::StateType state_type, ShadowType shadow_type, const Gdk::Rectangle& area, Widget& widget, const Glib::ustring& detail, int x, int y, int width, int height, PositionType gap_side, int gap_x, int gap_width) const;

  
  /** Draws a slider in the given rectangle on @a window using the
   * given style and orientation.
   * 
   * @param window A Gdk::Window.
   * @param state_type A state.
   * @param shadow_type A shadow.
   * @param area Clip rectangle, or <tt>0</tt> if the
   * output should not be clipped.
   * @param widget The widget.
   * @param detail A style detail.
   * @param x The x origin of the rectangle in which to draw a slider.
   * @param y The y origin of the rectangle in which to draw a slider.
   * @param width The width of the rectangle in which to draw a slider.
   * @param height The height of the rectangle in which to draw a slider.
   * @param orientation The orientation to be used.
   */
  void paint_slider(const Glib::RefPtr<Gdk::Window>& window, Gtk::StateType state_type, ShadowType shadow_type, const Gdk::Rectangle& area, Widget& widget, const Glib::ustring& detail, int x, int y, int width, int height, Orientation orientation) const;

  
  /** Draws an option menu tab (i.e.\ the up and down pointing arrows)
   * in the given rectangle on @a window using the given parameters.
   * 
   * @param window A Gdk::Window.
   * @param state_type A state.
   * @param shadow_type The type of shadow to draw.
   * @param area Clip rectangle, or <tt>0</tt> if the
   * output should not be clipped.
   * @param widget The widget.
   * @param detail A style detail.
   * @param x X origin of the rectangle to draw the tab in.
   * @param y Y origin of the rectangle to draw the tab in.
   * @param width The width of the rectangle to draw the tab in.
   * @param height The height of the rectangle to draw the tab in.
   */
  void paint_tab(const Glib::RefPtr<Gdk::Window>& window, Gtk::StateType state_type, ShadowType shadow_type, const Gdk::Rectangle& area, Widget& widget, const Glib::ustring& detail, int x, int y, int width, int height) const;

                  
  /** Draws a vertical line from ( @a x, @a y1) to ( @a x, @a y2) in @a window
   * using the given style and state.
   * 
   * @param window A Gdk::Window.
   * @param state_type A state.
   * @param area Rectangle to which the output is clipped, or <tt>0</tt> if the
   * output should not be clipped.
   * @param widget The widget.
   * @param detail A style detail.
   * @param y1 The starting y coordinate.
   * @param y2 The ending y coordinate.
   * @param x The x coordinate.
   */
  void paint_vline(const Glib::RefPtr<Gdk::Window>& window, Gtk::StateType state_type, const Gdk::Rectangle& area, Widget& widget, const Glib::ustring& detail, int y1, int y2, int x) const;

  
  /** Draws an expander as used in Gtk::TreeView. @a x and @a y specify the
   * center the expander. The size of the expander is determined by the
   * "expander-size" style property of @a widget.  (If widget is not
   * specified or doesn't have an "expander-size" property, an
   * unspecified default size will be used, since the caller doesn't
   * have sufficient information to position the expander, this is
   * likely not useful.) The expander is expander_size pixels tall
   * in the collapsed position and expander_size pixels wide in the
   * expanded position.
   * 
   * @param window A Gdk::Window.
   * @param state_type A state.
   * @param area Clip rectangle, or <tt>0</tt> if the
   * output should not be clipped.
   * @param widget The widget.
   * @param detail A style detail.
   * @param x The x position to draw the expander at.
   * @param y The y position to draw the expander at.
   * @param expander_style The style to draw the expander in; determines
   * whether the expander is collapsed, expanded, or in an
   * intermediate state.
   */
  void paint_expander(const Glib::RefPtr<Gdk::Window>& window, Gtk::StateType state_type, const Gdk::Rectangle& area, Widget& widget, const Glib::ustring& detail, int x, int y, ExpanderStyle expander_style) const;

  
  /** Draws a layout on @a window using the given parameters.
   * 
   * @param window A Gdk::Window.
   * @param state_type A state.
   * @param use_text Whether to use the text or foreground
   * graphics context of @a style.
   * @param area Clip rectangle, or <tt>0</tt> if the
   * output should not be clipped.
   * @param widget The widget.
   * @param detail A style detail.
   * @param x X origin.
   * @param y Y origin.
   * @param layout The layout to draw.
   */
  void paint_layout(const Glib::RefPtr<Gdk::Window>& window, Gtk::StateType state_type, bool use_text, const Gdk::Rectangle& area, Widget& widget, const Glib::ustring& detail, int x, int y, const Glib::RefPtr<Pango::Layout>& layout) const;

  
  /** Draws a resize grip in the given rectangle on @a window using the given
   * parameters.
   * 
   * @param window A Gdk::Window.
   * @param state_type A state.
   * @param area Clip rectangle, or <tt>0</tt> if the
   * output should not be clipped.
   * @param widget The widget.
   * @param detail A style detail.
   * @param edge The edge in which to draw the resize grip.
   * @param x The x origin of the rectangle in which to draw the resize grip.
   * @param y The y origin of the rectangle in which to draw the resize grip.
   * @param width The width of the rectangle in which to draw the resize grip.
   * @param height The height of the rectangle in which to draw the resize grip.
   */
  void paint_resize_grip(const Glib::RefPtr<Gdk::Window>& window, Gtk::StateType state_type, const Gdk::Rectangle& area, Widget& widget, const Glib::ustring& detail, Gdk::WindowEdge edge, int x, int y, int width, int height) const;


#ifndef GTKMM_DISABLE_DEPRECATED

  /** Creates a copy of the passed in Gtk::Style object.
   * 
   * @deprecated Use the const version
   * 
   * @return A copy of @a style.
   */
  Glib::RefPtr<Style> copy();
#endif // GTKMM_DISABLE_DEPRECATED


  /** Creates a copy of the passed in Gtk::Style object.
   * 
   * @return A copy of @a style.
   */
  Glib::RefPtr<Style> copy() const;

  
  /** Attaches a style to a window; this process allocates the
   * colors and creates the GC's for the style - it specializes
   * it to a particular visual and colormap. The process may
   * involve the creation of a new style if the style has already
   * been attached to a window with a different style and colormap.
   * 
   * Since this function may return a new object, you have to use it
   * in the following way:
   * <tt>style = gtk_style_attach (style, window)</tt>
   * 
   * @param window A Gdk::Window.
   * @return Either @a style, or a newly-created Gtk::Style.
   * If the style is newly created, the style parameter
   * will be unref'ed, and the new style will have
   * a reference count belonging to the caller.
   */
  Glib::RefPtr<Style> attach(const Glib::RefPtr<Gdk::Window>& window);
  
  /** Detaches a style from a window. If the style is not attached
   * to any windows anymore, it is unrealized. See attach().
   */
  void detach();

  
  /** Sets the background of @a window to the background color or pixmap
   * specified by @a style for the given state.
   * 
   * @param window A Gdk::Window.
   * @param state_type A state.
   */
  void set_background(const Glib::RefPtr<Gdk::Window>& window, Gtk::StateType state_type);

  
  /** 
   */
  void apply_default_background(const Glib::RefPtr<Gdk::Window>& window, bool set_bg, Gtk::StateType state_type, const Gdk::Rectangle& area, int x, int y, int width, int height);

  
  /** Looks up @a stock_id in the icon factories associated with @a style
   * and the default icon factory, returning an icon set if found,
   * otherwise <tt>0</tt>.
   * 
   * @param stock_id An icon name.
   * @return Icon set of @a stock_id.
   */

  IconSet lookup_icon_set(const Gtk::StockID& stock_id);

  
  /** Looks up @a color_name in the style's logical color mappings,
   * filling in @a color and returning <tt>true</tt> if found, otherwise
   * returning <tt>false</tt>. Do not cache the found mapping, because
   * it depends on the Gtk::Style and might change when a theme
   * switch occurs.
   * 
   * @param color_name The name of the logical color to look up.
   * @param color The Gdk::Color to fill in.
   * @return <tt>true</tt> if the mapping was found.
   */
  bool lookup_color(const Glib::ustring& color_name, Gdk::Color& color) const;


  /** Renders the icon specified by @a source at the given @a size
   * according to the given parameters and returns the result in a
   * pixbuf.
   * 
   * @param source The Gtk::IconSource specifying the icon to render.
   * @param direction A text direction.
   * @param state A state.
   * @param size (type int) the size to render the icon at. A size of
   * (GtkIconSize)-1 means render at the size of the source and
   * don't scale.
   * @param widget The widget.
   * @param detail A style detail.
   * @return A newly-created Gdk::Pixbuf
   * containing the rendered icon.
   */
  Glib::RefPtr<Gdk::Pixbuf> render_icon(const IconSource& source, Gtk::TextDirection direction, Gtk::StateType state, Gtk::IconSize size, Gtk::Widget& widget, const Glib::ustring& detail);

  
  /** Queries the value of a style property corresponding to a
   * widget class is in the given style.
   * 
   * @param widget_type The Type of a descendant of Gtk::Widget.
   * @param property_name The name of the style property to get.
   * @param value A Value where the value of the property being
   * queried will be stored.
   */
  void get_style_property_value(GType widget_type, const Glib::ustring& property_name, Glib::ValueBase& value);
  
  /** Queries the value of a style property corresponding to a
   * widget class in the given style.
   *
   * @param widget_type the GType of a descendant of GtkWidget.
   * @param property_name The name of the style property to get.
   * @param value: An output parameter in which  the value of the property being queried will be stored.
   */
  template <class PropertyType>
  void get_style_property(GType widget_type, const Glib::ustring& property_name, PropertyType& value) const;

  
protected:
    virtual void realize_vfunc();

    virtual void unrealize_vfunc();


    virtual void copy_vfunc(const Glib::RefPtr<Style>& src);


    virtual Glib::RefPtr<Style> clone_vfunc();

 
    virtual void init_from_rc_vfunc(const Glib::RefPtr<RcStyle>& rc_style);


    virtual void set_background_vfunc(const Glib::RefPtr<Gdk::Window>& window, Gtk::StateType state_type);


    virtual Glib::RefPtr<Gdk::Pixbuf> render_icon_vfunc(const IconSource& source, TextDirection direction, Gtk::StateType state, IconSize size, Widget* widget, const Glib::ustring& detail);

    virtual void draw_hline_vfunc(const Glib::RefPtr<Gdk::Window>& window, Gtk::StateType state_type, const Gdk::Rectangle& area, Widget* widget, const Glib::ustring& detail, int x1, int x2, int y);

    virtual void draw_vline_vfunc(const Glib::RefPtr<Gdk::Window>& window, Gtk::StateType state_type, const Gdk::Rectangle& area, Widget* widget, const Glib::ustring& detail, int y1, int y2, int x);

    virtual void draw_shadow_vfunc(const Glib::RefPtr<Gdk::Window>& window, Gtk::StateType state_type, ShadowType shadow_type, const Gdk::Rectangle& area, Widget* widget, const Glib::ustring& detail, int x, int y, int width, int height);

    virtual void draw_polygon_vfunc(const Glib::RefPtr<Gdk::Window>& window, Gtk::StateType state_type, ShadowType shadow_type, const Gdk::Rectangle& area, Widget* widget, const Glib::ustring& detail, GdkPoint* point, int npoints, bool fill);

    virtual void draw_arrow_vfunc(const Glib::RefPtr<Gdk::Window>& window, Gtk::StateType state_type, ShadowType shadow_type, const Gdk::Rectangle& area, Widget* widget, const Glib::ustring& detail, Gtk::ArrowType arrow_type, bool fill, int x, int y, int width, int height);

    virtual void draw_diamond_vfunc(const Glib::RefPtr<Gdk::Window>& window, Gtk::StateType state_type, ShadowType shadow_type, const Gdk::Rectangle& area, Widget* widget, const Glib::ustring& detail, int x, int y, int width, int height);

    virtual void draw_string_vfunc(const Glib::RefPtr<Gdk::Window>& window, Gtk::StateType state_type, const Gdk::Rectangle& area, Widget* widget, const Glib::ustring& detail, int x, int y, const Glib::ustring& string);

    virtual void draw_box_vfunc(const Glib::RefPtr<Gdk::Window>& window, Gtk::StateType state_type, ShadowType shadow_type, const Gdk::Rectangle& area, Widget* widget, const Glib::ustring& detail, int x, int y, int width, int height);

    virtual void draw_flat_box_vfunc(const Glib::RefPtr<Gdk::Window>& window, Gtk::StateType state_type, ShadowType shadow_type, const Gdk::Rectangle& area, Widget* widget, const Glib::ustring& detail, int x, int y, int width, int height);

    virtual void draw_check_vfunc(const Glib::RefPtr<Gdk::Window>& window, Gtk::StateType state_type, ShadowType shadow_type, const Gdk::Rectangle& area, Widget* widget, const Glib::ustring& detail, int x, int y, int width, int height);

    virtual void draw_option_vfunc(const Glib::RefPtr<Gdk::Window>& window, Gtk::StateType state_type, ShadowType shadow_type, const Gdk::Rectangle& area, Widget* widget, const Glib::ustring& detail, int x, int y, int width, int height);

    virtual void draw_tab_vfunc(const Glib::RefPtr<Gdk::Window>& window, Gtk::StateType state_type, ShadowType shadow_type, const Gdk::Rectangle& area, Widget* widget, const Glib::ustring& detail, int x, int y, int width, int height);

    virtual void draw_shadow_gap_vfunc(const Glib::RefPtr<Gdk::Window>& window, Gtk::StateType state_type, ShadowType shadow_type, const Gdk::Rectangle& area, Widget* widget, const Glib::ustring& detail, int x, int y, int width, int height, Gtk::PositionType gap_side, int gap_x, int gap_width);

    virtual void draw_box_gap_vfunc(const Glib::RefPtr<Gdk::Window>& window, Gtk::StateType state_type, ShadowType shadow_type, const Gdk::Rectangle& area, Widget* widget, const Glib::ustring& detail, int x, int y, int width, int height, Gtk::PositionType gap_side, int gap_x, int gap_width);

    virtual void draw_extension_vfunc(const Glib::RefPtr<Gdk::Window>& window, Gtk::StateType state_type, ShadowType shadow_type, const Gdk::Rectangle& area, Widget* widget, const Glib::ustring& detail, int x, int y, int width, int height, PositionType gap_side);

    virtual void draw_focus_vfunc(const Glib::RefPtr<Gdk::Window>& window, Gtk::StateType state_type, const Gdk::Rectangle& area, Widget* widget, const Glib::ustring& detail, int x, int y, int width, int height);

    virtual void draw_slider_vfunc(const Glib::RefPtr<Gdk::Window>& window, Gtk::StateType state_type, ShadowType shadow_type, const Gdk::Rectangle& area, Widget* widget, const Glib::ustring& detail, int x, int y, int width, int height, Orientation orientation);

    virtual void draw_handle_vfunc(const Glib::RefPtr<Gdk::Window>& window, Gtk::StateType state_type, ShadowType shadow_type, const Gdk::Rectangle& area, Widget* widget, const Glib::ustring& detail, int x, int y, int width, int height, Orientation orientation);

    virtual void draw_expander_vfunc(const Glib::RefPtr<Gdk::Window>& window, Gtk::StateType state_type, const Gdk::Rectangle& area, Widget* widget, const Glib::ustring& detail, int x, int y, ExpanderStyle expander_style);

 
    virtual void draw_layout_vfunc(const Glib::RefPtr<Gdk::Window>& window, Gtk::StateType state_type, bool use_text, const Gdk::Rectangle& area, Widget* widget, const Glib::ustring& detail, int x, int y, const Glib::RefPtr<Pango::Layout>& layout);

    virtual void draw_resize_grip_vfunc(const Glib::RefPtr<Gdk::Window>& window, Gtk::StateType state_type, const Gdk::Rectangle& area, Widget* widget, const Glib::ustring& detail, Gdk::WindowEdge edge, int x, int y, int width, int height);


  /**
   * @par Slot Prototype:
   * <tt>void on_my_%realize()</tt>
   *
   */

  Glib::SignalProxy0< void > signal_realize();

  
  /**
   * @par Slot Prototype:
   * <tt>void on_my_%unrealize()</tt>
   *
   */

  Glib::SignalProxy0< void > signal_unrealize();


public:

public:
  //C++ methods used to invoke GTK+ virtual functions:

protected:
  //GTK+ Virtual Functions (override these to change behaviour):

  //Default Signal Handlers::
  /// This is a default handler for the signal signal_realize().
  virtual void on_realize();
  /// This is a default handler for the signal signal_unrealize().
  virtual void on_unrealize();


};


template <class PropertyType> inline
void Style::get_style_property(GType widget_type, const Glib::ustring& property_name, PropertyType& value) const
{
  Glib::Value<PropertyType> property_value;
  property_value.init(Glib::Value<PropertyType>::value_type());

  //We cast away the const.
  //TODO: Either this get_style_property() should be non-const, 
  //or get_style_property_value() should be const.
  //We can't really have both const and unconst versions of them because output parameters can't be const.
  //Bug https://bugzilla.gnome.org/show_bug.cgi?id=594171
  Style* unconst_this = const_cast<Style*>(this);
  unconst_this->get_style_property_value(widget_type, property_name, property_value);

  value = property_value.get();
}

} // namespace Gtk


namespace Glib
{
  /** A Glib::wrap() method for this object.
   * 
   * @param object The C instance.
   * @param take_copy False if the result should take ownership of the C instance. True if it should take a new copy or ref.
   * @result A C++ instance that wraps this C instance.
   *
   * @relates Gtk::Style
   */
  Glib::RefPtr<Gtk::Style> wrap(GtkStyle* object, bool take_copy = false);
}


#endif /* _GTKMM_STYLE_H */

