/*
 * Copyright (C) 2008-2015 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
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

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "pbd/signals.h"

#include <ytkmm/widget.h>
#include <ytkmm/checkbutton.h>

#include "ardour/bundle.h"
#include "ardour/data_type.h"
#include "ardour/types.h"

namespace ARDOUR {
	class Session;
	class Processor;
	class IO;
}

class PortMatrix;
class PublicEditor;

/** A list of bundles grouped by some aspect of their type e.g. busses, tracks, system.
 *  A group has 0 or more bundles.
 */
class PortGroup : public sigc::trackable
{
public:
	PortGroup (std::string const & n);
	~PortGroup ();

	void add_bundle (std::shared_ptr<ARDOUR::Bundle>, bool allow_dups = false);
	void add_bundle (std::shared_ptr<ARDOUR::Bundle>, std::shared_ptr<ARDOUR::IO> io);
	void add_bundle (std::shared_ptr<ARDOUR::Bundle>, std::shared_ptr<ARDOUR::IO>, Gdk::Color);
	void remove_bundle (std::shared_ptr<ARDOUR::Bundle>);
	std::shared_ptr<ARDOUR::Bundle> only_bundle ();
	void clear ();
	ARDOUR::ChanCount total_channels () const;
	std::shared_ptr<ARDOUR::IO> io_from_bundle (std::shared_ptr<ARDOUR::Bundle>) const;
	void remove_duplicates ();

	std::string name; ///< name for the group

	bool has_port (std::string const &) const;

	/** The bundle list has changed in some way; a bundle has been added or removed, or the list cleared etc. */
	PBD::Signal<void()> Changed;

	/** An individual bundle on our list has changed in some way */
	PBD::Signal<void(ARDOUR::Bundle::Change)> BundleChanged;

	struct BundleRecord {
	    std::shared_ptr<ARDOUR::Bundle> bundle;
	    /** IO whose ports are in the bundle, or 0.  This is so that we can do things like adding
		ports to the IO from matrix editor menus. */
	    std::weak_ptr<ARDOUR::IO> io;
	    Gdk::Color colour;
	    bool has_colour;
	    PBD::ScopedConnection changed_connection;

	    BundleRecord (std::shared_ptr<ARDOUR::Bundle>, std::shared_ptr<ARDOUR::IO>, Gdk::Color, bool has_colour);
	};

	typedef std::list<BundleRecord*> BundleList;

	BundleList const & bundles () const {
		return _bundles;
	}

private:
	void bundle_changed (ARDOUR::Bundle::Change);
	void add_bundle_internal (std::shared_ptr<ARDOUR::Bundle>, std::shared_ptr<ARDOUR::IO>, bool, Gdk::Color, bool);

	BundleList _bundles;
};

/// A list of PortGroups
class PortGroupList : public sigc::trackable
{
public:
	PortGroupList ();
	~PortGroupList();

	typedef std::vector<std::shared_ptr<PortGroup> > List;

	void add_group (std::shared_ptr<PortGroup>);
	void add_group_if_not_empty (std::shared_ptr<PortGroup>);
	void gather (ARDOUR::Session *, ARDOUR::DataType, bool, bool, bool);
	PortGroup::BundleList const & bundles () const;
	void clear ();
	void remove_bundle (std::shared_ptr<ARDOUR::Bundle>);
	ARDOUR::ChanCount total_channels () const;
	uint32_t size () const {
		return _groups.size();
	}
	std::shared_ptr<ARDOUR::IO> io_from_bundle (std::shared_ptr<ARDOUR::Bundle>) const;

	void suspend_signals ();
	void resume_signals ();

	List::const_iterator begin () const {
		return _groups.begin ();
	}

	List::const_iterator end () const {
		return _groups.end ();
	}

	bool empty () const;

	/** The group list has changed in some way; a group has been added or removed, or the list cleared etc. */
	PBD::Signal<void()> Changed;

	/** A bundle in one of our groups has changed */
	PBD::Signal<void(ARDOUR::Bundle::Change)> BundleChanged;

private:
	bool port_has_prefix (std::string const &, std::string const &) const;
	std::string common_prefix (std::vector<std::string> const &) const;
	std::string common_prefix_before (std::vector<std::string> const &, std::string const &) const;
	void emit_changed ();
	void emit_bundle_changed (ARDOUR::Bundle::Change);
	std::shared_ptr<ARDOUR::Bundle> make_bundle_from_ports (std::vector<std::string> const &, ARDOUR::DataType, bool, std::string const& bundle_name = std::string()) const;
	void add_bundles_for_ports (std::vector<std::string> const &, ARDOUR::DataType, bool, bool, std::shared_ptr<PortGroup>) const;
	void maybe_add_processor_to_list (std::weak_ptr<ARDOUR::Processor>, std::list<std::shared_ptr<ARDOUR::IO> > *, bool, std::set<std::shared_ptr<ARDOUR::IO> > &);

	mutable PortGroup::BundleList _bundles;
	List _groups;
	PBD::ScopedConnectionList _bundle_changed_connections;
	PBD::ScopedConnectionList _changed_connections;
	bool _signals_suspended;
	bool _pending_change;
	ARDOUR::Bundle::Change _pending_bundle_change;
};

