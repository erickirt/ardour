/*
 * Copyright (C) 2016 Paul Davis <paul@linuxaudiosystems.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#pragma once

#include "ardour/libardour_visibility.h"

#include <memory>

namespace ARDOUR {

class ControlGroup;

class LIBARDOUR_API ControlGroupMember
{
  public:
	virtual ~ControlGroupMember () {};
  private:
	friend class ControlGroup;
	/* Only a ControlGroup can call this; all membership changes must be
	   mediated by the ControlGroup, not by operating on the member.
	*/
	virtual void set_group (std::shared_ptr<ControlGroup>) = 0;
	virtual bool push_group (std::shared_ptr<ControlGroup>) = 0;
	virtual bool pop_group () = 0;
};

} /* namespace */

