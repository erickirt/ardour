/*
 * Copyright (C) 2007-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2008-2013 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2017 Julien "_FrnchFrgg_" RIVAUD <frnchfrgg@free.fr>
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
#include <string>
#include <vector>

#include <glibmm/threads.h>

#include "pbd/signals.h"

#include "ardour/data_type.h"
#include "ardour/chan_count.h"

namespace ARDOUR {

class AudioEngine;

/** A set of `channels', each of which is associated with 0 or more ports.
 *  Each channel has a name which can be anything useful, and a data type.
 *  Intended for grouping things like, for example, a buss' outputs.
 *  `Channel' is a rather overloaded term but I can't think of a better
 *  one right now.
 */
class LIBARDOUR_API Bundle : public PBD::ScopedConnectionList
{
  public:

	/// List of ports associated with a channel.  We can't use a
	/// PortSet because we might want to involve non-Ardour ports
	/// (ie those without a Port object)
	typedef std::vector<std::string> PortList;

	struct Channel {
		/* Constructor with no port name(s) */
		Channel (std::string n, DataType t) : name (n), type (t) {}
		/* Constructor with list of port names */
		Channel (std::string n, DataType t, PortList p) : name (n), type (t), ports (p) {}
		/* Constructor with a port name */
		Channel (std::string n, DataType t, std::string const & p) : name (n), type (t) {
			ports.push_back (p);
		}

		bool operator== (Channel const &o) const {
			return name == o.name && type == o.type && ports == o.ports;
		}

		std::string name;
		DataType type;
		PortList ports;
	};

	Bundle (bool i = true);
	Bundle (std::string const &, bool i = true);
	Bundle (std::shared_ptr<Bundle>);

	virtual ~Bundle() {}

	/** @return Number of channels that this Bundle has */
	ChanCount nchannels () const;
	uint32_t n_total () const; /* shortcut for nchannels().n_total() */

	/** @param c Channel index.
	 *  @return Ports associated with this channel.
	 */
	PortList const & channel_ports (uint32_t c) const;

	void add_channel (std::string const &, DataType);
	void add_channel (std::string const &, DataType, std::string const &);
	void add_channel (std::string const &, DataType, PortList);
	std::string channel_name (uint32_t) const;
	DataType channel_type (uint32_t) const;
	void set_channel_name (uint32_t, std::string const &);
	void add_port_to_channel (uint32_t, std::string);
	void set_port (uint32_t, std::string);
	void remove_port_from_channel (uint32_t, std::string);
	void remove_ports_from_channel (uint32_t);
	void remove_ports_from_channels ();
	bool port_attached_to_channel (uint32_t, std::string);
	bool offers_port (std::string) const;
	bool offers_port_alone (std::string) const;
	void remove_channel (uint32_t);
	void remove_channels ();
	void add_channels_from_bundle (std::shared_ptr<Bundle>);
	void connect (std::shared_ptr<Bundle>, AudioEngine &,
	              bool allow_partial = false);
	void disconnect (std::shared_ptr<Bundle>, AudioEngine &);
	bool connected_to (std::shared_ptr<Bundle>, AudioEngine &,
	                   DataType type = DataType::NIL,
	                   bool exclusive = false);
	bool connected_to_anything (AudioEngine &);
	bool has_same_ports (std::shared_ptr<Bundle>) const;
	uint32_t type_channel_to_overall (DataType, uint32_t) const;
	uint32_t overall_channel_to_type (DataType, uint32_t) const;

	void set_name (std::string const &);

	/** @return Bundle name */
	std::string name () const { return _name; }

	void set_ports_are_inputs ();
	void set_ports_are_outputs ();
	bool ports_are_inputs () const { return _ports_are_inputs; }
	bool ports_are_outputs () const { return !_ports_are_inputs; }

	void suspend_signals ();
	void resume_signals ();

	bool operator== (Bundle const & other);

	/** Things that might change about this bundle */
	enum Change {
		NameChanged = 0x1, ///< the bundle name or a channel name has changed
		ConfigurationChanged = 0x2, ///< the number of channels has changed
		PortsChanged = 0x4, ///< the port list associated with one of our channels has changed
		TypeChanged = 0x8, ///< the data type has changed
		DirectionChanged = 0x10 ///< the direction (whether ports are inputs or outputs) has changed
	};

	PBD::Signal<void(Change)> Changed;

  protected:

	/// mutex for _channel_ports and _channel_names
	/// XXX: is this necessary?
	mutable Glib::Threads::Mutex _channel_mutex;
	std::vector<Channel> _channel;

  private:
	int set_channels (std::string const &);
	int parse_io_string (std::string const &, std::vector<std::string> &);
	void emit_changed (Change);

	std::string _name;
	bool _ports_are_inputs;

	bool _signals_suspended;
	Change _pending_change;
};

class LIBARDOUR_API BundleChannel
{
public:
	BundleChannel () : channel (-1) {}

	BundleChannel (std::shared_ptr<Bundle> b, int c)
		: bundle (b), channel (c) {}

	bool operator== (BundleChannel const& other) const {
		return bundle == other.bundle && channel == other.channel;
	}

	bool operator!= (BundleChannel const& other) const {
		return bundle != other.bundle || channel != other.channel;
	}

	ChanCount nchannels () const {
		if (!_nchannels) {
			_nchannels = bundle->nchannels ();
		}
		return _nchannels.value ();
	}

	std::shared_ptr<Bundle> bundle;
	int channel; ///< channel index, or -1 for "all"

private:
	mutable std::optional<ChanCount> _nchannels;
};

}

std::ostream & operator<< (std::ostream & o, ARDOUR::Bundle const &);

