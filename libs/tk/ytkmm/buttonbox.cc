// Generated by gmmproc 2.45.3 -- DO NOT MODIFY!

//Allow us to set the struct fields for the style properties.
//TODO: Find a better way to set them.
//TODO: Remove these functions anyway in an ABI break.
#undef GSEAL_ENABLE
 

#include <glibmm.h>

#include <ytkmm/buttonbox.h>
#include <ytkmm/private/buttonbox_p.h>


// -*- c++ -*-
/* $Id: buttonbox.ccg,v 1.1 2003/01/21 13:38:43 murrayc Exp $ */

/*
 *
 * Copyright 1998-2002 The gtkmm Development Team
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

#include <ytk/ytk.h>

namespace Gtk
{

VButtonBox::VButtonBox(ButtonBoxStyle layout, int spacing)
:
  // Mark this class as non-derived to allow C++ vfuncs to be skipped.
  Glib::ObjectBase(0),
  ButtonBox(Glib::ConstructParams(vbuttonbox_class_.init()))
{
  set_layout(layout);
  set_spacing(spacing);
}

HButtonBox::HButtonBox(ButtonBoxStyle layout, int spacing)
:
  // Mark this class as non-derived to allow C++ vfuncs to be skipped.
  Glib::ObjectBase(0),
  ButtonBox(Glib::ConstructParams(hbuttonbox_class_.init()))
{
  set_layout(layout);
  set_spacing(spacing);
}


int ButtonBox::get_child_min_width() const
{
  int result = 0;
  gtk_widget_style_get(const_cast<GtkWidget*>(GTK_WIDGET(gobj())), "child-min-width", &result, (void*)0);
  return result;
}

void ButtonBox::set_child_min_width(const int& value)
{
  //TODO: Find a way to set style properties in code, though this is deprecated anyway:
  gobj()->child_min_width = value;
}

int ButtonBox::get_child_min_height() const
{
  int result = 0;
  gtk_widget_style_get(const_cast<GtkWidget*>(GTK_WIDGET(gobj())), "child-min-width", &result, (void*)0);
  return result;
}

void ButtonBox::set_child_min_height(const int& value)
{
  //TODO: Find a way to set style properties in code, though this is deprecated anyway:
  gobj()->child_min_height = value;
}

int ButtonBox::get_child_ipadding_x() const
{
  int result = 0;
  gtk_widget_style_get(const_cast<GtkWidget*>(GTK_WIDGET(gobj())), "child-internal-padding-x", &result, (void*)0);
  return result;
}

void ButtonBox::set_child_ipadding_x(const int& value)
{
  //TODO: Find a way to set style properties in code, though this is deprecated anyway:
  gobj()->child_ipad_x = value;
}

int ButtonBox::get_child_ipadding_y() const
{
  int result = 0;
  gtk_widget_style_get(const_cast<GtkWidget*>(GTK_WIDGET(gobj())), "child-internal-padding-y", &result, (void*)0);
  return result;
}

void ButtonBox::set_child_ipadding_y(const int& value)
{
  //TODO: Find a way to set style properties in code, though this is deprecated anyway:
  gobj()->child_ipad_y = value;
}

} // namespace Gtk

namespace
{
} // anonymous namespace


namespace Glib
{

Gtk::ButtonBox* wrap(GtkButtonBox* object, bool take_copy)
{
  return dynamic_cast<Gtk::ButtonBox *> (Glib::wrap_auto ((GObject*)(object), take_copy));
}

} /* namespace Glib */

namespace Gtk
{


/* The *_Class implementation: */

const Glib::Class& ButtonBox_Class::init()
{
  if(!gtype_) // create the GType if necessary
  {
    // Glib::Class has to know the class init function to clone custom types.
    class_init_func_ = &ButtonBox_Class::class_init_function;

    // This is actually just optimized away, apparently with no harm.
    // Make sure that the parent type has been created.
    //CppClassParent::CppObjectType::get_type();

    // Create the wrapper type, with the same class/instance size as the base type.
    register_derived_type(gtk_button_box_get_type());

    // Add derived versions of interfaces, if the C type implements any interfaces:

  }

  return *this;
}


void ButtonBox_Class::class_init_function(void* g_class, void* class_data)
{
  BaseClassType *const klass = static_cast<BaseClassType*>(g_class);
  CppClassParent::class_init_function(klass, class_data);


}


Glib::ObjectBase* ButtonBox_Class::wrap_new(GObject* o)
{
  return manage(new ButtonBox((GtkButtonBox*)(o)));

}


/* The implementation: */

ButtonBox::ButtonBox(const Glib::ConstructParams& construct_params)
:
  Gtk::Box(construct_params)
{
  }

ButtonBox::ButtonBox(GtkButtonBox* castitem)
:
  Gtk::Box((GtkBox*)(castitem))
{
  }

ButtonBox::~ButtonBox()
{
  destroy_();
}

ButtonBox::CppClassType ButtonBox::buttonbox_class_; // initialize static member

GType ButtonBox::get_type()
{
  return buttonbox_class_.init().get_type();
}


GType ButtonBox::get_base_type()
{
  return gtk_button_box_get_type();
}


ButtonBoxStyle ButtonBox::get_layout() const
{
  return ((ButtonBoxStyle)(gtk_button_box_get_layout(const_cast<GtkButtonBox*>(gobj()))));
}

void ButtonBox::set_layout(ButtonBoxStyle layout_style)
{
  gtk_button_box_set_layout(gobj(), ((GtkButtonBoxStyle)(layout_style)));
}

void ButtonBox::set_child_secondary(Widget& child, bool is_secondary)
{
  gtk_button_box_set_child_secondary(gobj(), (child).gobj(), static_cast<int>(is_secondary));
}

bool ButtonBox::get_child_secondary(const Gtk::Widget& child) const
{
  return gtk_button_box_get_child_secondary(const_cast<GtkButtonBox*>(gobj()), const_cast<GtkWidget*>(child.gobj()));
}


Glib::PropertyProxy< ButtonBoxStyle > ButtonBox::property_layout_style() 
{
  return Glib::PropertyProxy< ButtonBoxStyle >(this, "layout-style");
}

Glib::PropertyProxy_ReadOnly< ButtonBoxStyle > ButtonBox::property_layout_style() const
{
  return Glib::PropertyProxy_ReadOnly< ButtonBoxStyle >(this, "layout-style");
}


} // namespace Gtk


namespace Glib
{

Gtk::VButtonBox* wrap(GtkVButtonBox* object, bool take_copy)
{
  return dynamic_cast<Gtk::VButtonBox *> (Glib::wrap_auto ((GObject*)(object), take_copy));
}

} /* namespace Glib */

namespace Gtk
{


/* The *_Class implementation: */

const Glib::Class& VButtonBox_Class::init()
{
  if(!gtype_) // create the GType if necessary
  {
    // Glib::Class has to know the class init function to clone custom types.
    class_init_func_ = &VButtonBox_Class::class_init_function;

    // This is actually just optimized away, apparently with no harm.
    // Make sure that the parent type has been created.
    //CppClassParent::CppObjectType::get_type();

    // Create the wrapper type, with the same class/instance size as the base type.
    register_derived_type(gtk_vbutton_box_get_type());

    // Add derived versions of interfaces, if the C type implements any interfaces:

  }

  return *this;
}


void VButtonBox_Class::class_init_function(void* g_class, void* class_data)
{
  BaseClassType *const klass = static_cast<BaseClassType*>(g_class);
  CppClassParent::class_init_function(klass, class_data);


}


Glib::ObjectBase* VButtonBox_Class::wrap_new(GObject* o)
{
  return manage(new VButtonBox((GtkVButtonBox*)(o)));

}


/* The implementation: */

VButtonBox::VButtonBox(const Glib::ConstructParams& construct_params)
:
  Gtk::ButtonBox(construct_params)
{
  }

VButtonBox::VButtonBox(GtkVButtonBox* castitem)
:
  Gtk::ButtonBox((GtkButtonBox*)(castitem))
{
  }

VButtonBox::~VButtonBox()
{
  destroy_();
}

VButtonBox::CppClassType VButtonBox::vbuttonbox_class_; // initialize static member

GType VButtonBox::get_type()
{
  return vbuttonbox_class_.init().get_type();
}


GType VButtonBox::get_base_type()
{
  return gtk_vbutton_box_get_type();
}


} // namespace Gtk


namespace Glib
{

Gtk::HButtonBox* wrap(GtkHButtonBox* object, bool take_copy)
{
  return dynamic_cast<Gtk::HButtonBox *> (Glib::wrap_auto ((GObject*)(object), take_copy));
}

} /* namespace Glib */

namespace Gtk
{


/* The *_Class implementation: */

const Glib::Class& HButtonBox_Class::init()
{
  if(!gtype_) // create the GType if necessary
  {
    // Glib::Class has to know the class init function to clone custom types.
    class_init_func_ = &HButtonBox_Class::class_init_function;

    // This is actually just optimized away, apparently with no harm.
    // Make sure that the parent type has been created.
    //CppClassParent::CppObjectType::get_type();

    // Create the wrapper type, with the same class/instance size as the base type.
    register_derived_type(gtk_hbutton_box_get_type());

    // Add derived versions of interfaces, if the C type implements any interfaces:

  }

  return *this;
}


void HButtonBox_Class::class_init_function(void* g_class, void* class_data)
{
  BaseClassType *const klass = static_cast<BaseClassType*>(g_class);
  CppClassParent::class_init_function(klass, class_data);


}


Glib::ObjectBase* HButtonBox_Class::wrap_new(GObject* o)
{
  return manage(new HButtonBox((GtkHButtonBox*)(o)));

}


/* The implementation: */

HButtonBox::HButtonBox(const Glib::ConstructParams& construct_params)
:
  Gtk::ButtonBox(construct_params)
{
  }

HButtonBox::HButtonBox(GtkHButtonBox* castitem)
:
  Gtk::ButtonBox((GtkButtonBox*)(castitem))
{
  }

HButtonBox::~HButtonBox()
{
  destroy_();
}

HButtonBox::CppClassType HButtonBox::hbuttonbox_class_; // initialize static member

GType HButtonBox::get_type()
{
  return hbuttonbox_class_.init().get_type();
}


GType HButtonBox::get_base_type()
{
  return gtk_hbutton_box_get_type();
}


} // namespace Gtk


