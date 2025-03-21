// -*- c++ -*-
// Generated by gmmproc 2.45.3 -- DO NOT MODIFY!
#ifndef _GTKMM_ITEM_P_H
#define _GTKMM_ITEM_P_H


#include <ytkmm/private/bin_p.h>

#include <glibmm/class.h>

namespace Gtk
{

class Item_Class : public Glib::Class
{
public:
#ifndef DOXYGEN_SHOULD_SKIP_THIS
  typedef Item CppObjectType;
  typedef GtkItem BaseObjectType;
  typedef GtkItemClass BaseClassType;
  typedef Gtk::Bin_Class CppClassParent;
  typedef GtkBinClass BaseClassParent;

  friend class Item;
#endif /* DOXYGEN_SHOULD_SKIP_THIS */

  const Glib::Class& init();


  static void class_init_function(void* g_class, void* class_data);

  static Glib::ObjectBase* wrap_new(GObject*);

protected:

  //Callbacks (default signal handlers):
  //These will call the *_impl member methods, which will then call the existing default signal callbacks, if any.
  //You could prevent the original default signal handlers being called by overriding the *_impl method.
  static void select_callback(GtkItem* self);
  static void deselect_callback(GtkItem* self);
  static void toggle_callback(GtkItem* self);

  //Callbacks (virtual functions):
};


} // namespace Gtk


#endif /* _GTKMM_ITEM_P_H */

