/*
 * Copyright (C) 2006-2016 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2008 Hans Baier <hansfbaier@googlemail.com>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2012-2015 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2016-2017 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2016-2017 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2016 Julien "_FrnchFrgg_" RIVAUD <frnchfrgg@free.fr>
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

#include <cmath>
#include <climits>
#include <cfloat>

#include <set>

#include <glibmm/threads.h>
#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>

#include "temporal/beats.h"

#include "pbd/xml++.h"
#include "pbd/basename.h"
#include "pbd/types_convert.h"

#include "ardour/automation_control.h"
#include "ardour/midi_cursor.h"
#include "ardour/midi_model.h"
#include "ardour/midi_region.h"
#include "ardour/midi_ring_buffer.h"
#include "ardour/midi_source.h"
#include "ardour/playlist.h"
#include "ardour/region_factory.h"
#include "ardour/session.h"
#include "ardour/source_factory.h"
#include "ardour/tempo.h"
#include "ardour/thawlist.h"
#include "ardour/types.h"
#include "ardour/evoral_types_convert.h"

#include "pbd/i18n.h"
#include <locale.h>

using namespace std;
using namespace ARDOUR;
using namespace PBD;

namespace PBD {
  DEFINE_ENUM_CONVERT(ARDOUR::Source::Flag);
}

/* Basic MidiRegion constructor (many channels) */
MidiRegion::MidiRegion (const SourceList& srcs)
	: Region (srcs)
	, _ignore_shift (false)
{
	/* by default MIDI regions are transparent,
	 * this should probably be set depending on use-case,
	 * (eg. loop recording, vs copy/edit/paste)
	 */
	override_opaqueness (false);

	midi_source(0)->ModelChanged.connect_same_thread (_source_connection, std::bind (&MidiRegion::model_changed, this));
	model_changed ();
	assert(_name.val().find("/") == string::npos);
	assert(_type == DataType::MIDI);
}

MidiRegion::MidiRegion (std::shared_ptr<const MidiRegion> other)
	: Region (other)
	, _ignore_shift (false)
{
	assert(_name.val().find("/") == string::npos);
	midi_source(0)->ModelChanged.connect_same_thread (_source_connection, std::bind (&MidiRegion::model_changed, this));
	model_changed ();
}

/** Create a new MidiRegion that is part of an existing one */
MidiRegion::MidiRegion (std::shared_ptr<const MidiRegion> other, timecnt_t const & offset)
	: Region (other, offset)
	, _ignore_shift (false)
{

	assert(_name.val().find("/") == string::npos);
	midi_source(0)->ModelChanged.connect_same_thread (_source_connection, std::bind (&MidiRegion::model_changed, this));
	model_changed ();
}

MidiRegion::~MidiRegion ()
{
}

/** Export the MIDI data of the MidiRegion to a new MIDI file (SMF).
 */
bool
MidiRegion::do_export (string const& path) const
{
	std::shared_ptr<MidiSource> newsrc;

	/* caller must check for pre-existing file */
	assert (!path.empty());
	assert (!Glib::file_test (path, Glib::FILE_TEST_EXISTS));
	newsrc = std::dynamic_pointer_cast<MidiSource> (SourceFactory::createWritable (DataType::MIDI, _session, path, _session.sample_rate (), false, true));

	{
		/* Lock our source since we'll be reading from it.  write_to() will
		 * take a lock on newsrc.
		 */
		Source::ReaderLock lm (midi_source(0)->mutex());
		if (midi_source(0)->export_write_to (lm, newsrc, _start.val().beats(), _start.val().beats() + _length.val().beats())) {
			return false;
		}
	}

	return true;
}


/** Create a new MidiRegion that has its own version of some/all of the Source used by another.
 */
std::shared_ptr<MidiRegion>
MidiRegion::clone (string path) const
{
	std::shared_ptr<MidiSource> newsrc;

	/* caller must check for pre-existing file */
	assert (!path.empty());
	assert (!Glib::file_test (path, Glib::FILE_TEST_EXISTS));
	newsrc = std::dynamic_pointer_cast<MidiSource>(
		SourceFactory::createWritable(DataType::MIDI, _session, path, _session.sample_rate()));
	return clone (newsrc);
}

std::shared_ptr<MidiRegion>
MidiRegion::clone (std::shared_ptr<MidiSource> newsrc, ThawList* tl) const
{
	{
		std::shared_ptr<MidiSource> ms = midi_source(0);

		/* copy source state (cue markers, captured_for, CC/param interpolation */
		XMLNode& node (ms->get_state());
		/* these must be set, otherwise Source::set_state() returns early */
		node.set_property (X_("id"), newsrc->id());
		node.set_property (X_("name"), newsrc->name());
		node.set_property (X_("flags"), newsrc->flags ());
		node.set_property (X_("take-id"), newsrc->take_id());

		/* Lock our source since we'll be reading from it.  write_to() will
		   take a lock on newsrc.
		*/

		Source::ReaderLock lm (ms->mutex());
		if (ms->write_to (lm, newsrc, Temporal::Beats(), std::numeric_limits<Temporal::Beats>::max())) {
			delete &node;
			return std::shared_ptr<MidiRegion> ();
		}

		/* compare to SMFSource::set_state */
		newsrc->MidiSource::set_state (node, Stateful::current_state_version);
		newsrc->Source::set_state (node, Stateful::current_state_version);
		delete &node;
	}

	PropertyList plist (derive_properties ());

	plist.add (Properties::name, PBD::basename_nosuffix (newsrc->name()));
	plist.add (Properties::whole_file, true);
	plist.add (Properties::external, false);
	plist.add (Properties::import, false);
	plist.add (Properties::layer, 0);

	std::shared_ptr<MidiRegion> ret (std::dynamic_pointer_cast<MidiRegion> (RegionFactory::create (newsrc, plist, true, tl)));

	return ret;
}

timecnt_t
MidiRegion::read_at (Evoral::EventSink<samplepos_t>& out,
                     timepos_t const &               position,
                     timecnt_t const &               dur,
                     Temporal::Range*                loop_range,
                     MidiCursor&                     cursor,
                     uint32_t                        chan_n,
                     NoteMode                        mode,
                     MidiNoteTracker*               tracker,
                     MidiChannelFilter*              filter) const
{
	return _read_at (_sources, out, position, dur, loop_range, cursor, chan_n, mode, tracker, filter);
}

timecnt_t
MidiRegion::master_read_at (MidiRingBuffer<samplepos_t>& out,
                            timepos_t const &            position,
                            timecnt_t const &            dur,
                            Temporal::Range*             loop_range,
                            MidiCursor&                  cursor,
                            uint32_t                     chan_n,
                            NoteMode                     mode) const
{
	return _read_at (_master_sources, out, position, dur, loop_range, cursor, chan_n, mode); /* no tracker */
}

timecnt_t
MidiRegion::_read_at (const SourceList&              /*srcs*/,
                      Evoral::EventSink<samplepos_t>& dst,
                      timepos_t const &               pos,
                      timecnt_t const &               xdur,
                      Temporal::Range*                loop_range,
                      MidiCursor&                     cursor,
                      uint32_t                        chan_n,
                      NoteMode                        mode,
                      MidiNoteTracker*               tracker,
                      MidiChannelFilter*              filter) const
{
	timecnt_t dur (xdur);
	timecnt_t internal_offset;
	timecnt_t to_read;

	/* precondition: caller has verified that we cover the desired section */

	assert(chan_n == 0);

	if (muted()) {
		return timecnt_t(); /* read nothing */
	}

	if (pos < position()) {
		/* we are starting the read from before the start of the region */
		internal_offset = timecnt_t (Temporal::BeatTime);;
		dur -= pos.distance (position());
	} else {
		/* we are starting the read from after the start of the region */
		internal_offset = position().distance (pos);
	}

	if (internal_offset >= _length) {
		return timecnt_t(); /* read nothing */
	}

	if ((to_read = min (dur, _length - internal_offset)) == 0) {
		return timecnt_t(); /* read nothing */
	}

	std::shared_ptr<MidiSource> src = midi_source(chan_n);

	Source::ReaderLock lm (src->mutex());

#if 0
	cerr << "MR " << name () << " read @ " << pos << " + " << to_read
	     << " dur was " << dur
	     << " len " << _length
	     << " l-io " << (_length - internal_offset)
	     << " position = " << position()
	     << " _start = " << _start
	     << " intoffset = " << internal_offset
	     << " quarter_note = " << quarter_note()
	     << " start_beat = " << _start_beats
	     << endl;
#endif

	/* This call reads events from a source and writes them to `dst' timed in session samples */

	if (src->midi_read (
		    lm, // source lock
		    dst, // destination buffer
		    position().earlier (_start.val()), // start position of the source on timeline
		    _start.val() + internal_offset, // where to start reading in the source
		    to_read, // read duration in samples
		    loop_range,
		    cursor,
		    tracker,
		    filter,
		    _filtered_parameters
		    ) != to_read) {
		return timecnt_t(); /* "read nothing" */
	}

	return to_read;
}

int
MidiRegion::render (Evoral::EventSink<samplepos_t>& dst,
                    uint32_t                        chan_n,
                    NoteMode                        mode,
                    MidiChannelFilter*              filter) const
{
	return render_range (dst, chan_n, mode, this->start(), _length, filter);
}

int
MidiRegion::render_range (Evoral::EventSink<samplepos_t>& dst,
                          uint32_t                        chan_n,
                          NoteMode                        mode,
                          timepos_t const &               read_start,
                          timecnt_t const &               read_length,
                          MidiChannelFilter*              filter) const
{
	/* precondition: caller has verified that we cover the desired section */

	assert(chan_n == 0);

	if (muted()) {
		return 0; /* read nothing */
	}

	std::shared_ptr<MidiSource> src = midi_source(chan_n);


#if 0
	cerr << "MR " << name () << " render "
	     << " _position = " << position ()
	     << " _start = " << _start
	     << " _length = " << _length
	     << " read_start = " << read_start
	     << " read_length = " << read_length
	     << endl;
#endif

	MidiCursor cursor;
	MidiNoteTracker tracker;

	/* This call reads events from a source and writes them to `dst' timed in session samples */

	Source::ReaderLock lm (src->mutex());

	src->midi_read (
		lm, // source lock
		dst, // destination buffer
		source_position(), // start position of the source in session samples
		read_start,
		read_length,
		0,
		cursor,
		&tracker,
		filter,
		_filtered_parameters);

	/* resolve any notes that were "cut off" by the end of the region. The
	 * Note-Off's get inserted at the end of the region
	 */

	const timepos_t end = source_position() + read_start + read_length;
	tracker.resolve_notes (dst, end.samples());

	return 0;
}


XMLNode&
MidiRegion::state () const
{
	return Region::state ();
}

int
MidiRegion::set_state (const XMLNode& node, int version)
{
	int ret = Region::set_state (node, version);

	if (version < 7001) {
		/* previous sessions had uncontionally transparent MIDI regions */
		override_opaqueness (false);
	}

	return ret;
}

void
MidiRegion::recompute_at_end ()
{
	/* our length has changed
	 * so what? stuck notes are dealt with via a note state tracker
	 */
}

void
MidiRegion::recompute_at_start ()
{
	/* as above, but the shift was from the front
	 * maybe bump currently active note's note-ons up so they sound here?
	 * that could be undesireable in certain situations though.. maybe
	 * remove the note entirely, including it's note off?  something needs to
	 * be done to keep the played MIDI sane to avoid messing up voices of
	 * polyhonic things etc........
	 */
}

int
MidiRegion::separate_by_channel (vector< std::shared_ptr<Region> >&) const
{
	// TODO
	return -1;
}

std::shared_ptr<Evoral::Control>
MidiRegion::control (const Evoral::Parameter& id, bool create)
{
	return model()->control(id, create);
}

std::shared_ptr<const Evoral::Control>
MidiRegion::control (const Evoral::Parameter& id) const
{
	return model()->control(id);
}

std::shared_ptr<MidiModel>
MidiRegion::model()
{
	return midi_source()->model();
}

std::shared_ptr<const MidiModel>
MidiRegion::model() const
{
	return midi_source()->model();
}

std::shared_ptr<MidiSource>
MidiRegion::midi_source (uint32_t n) const
{
	// Guaranteed to succeed (use a static cast?)
	return std::dynamic_pointer_cast<MidiSource>(source(n));
}

void
MidiRegion::model_changed ()
{
	if (!model()) {
		return;
	}

	/* build list of filtered Parameters, being those whose automation state is not `Play' */

	_filtered_parameters.clear ();

	Automatable::Controls const & c = model()->controls();

	for (Automatable::Controls::const_iterator i = c.begin(); i != c.end(); ++i) {
		std::shared_ptr<AutomationControl> ac = std::dynamic_pointer_cast<AutomationControl> (i->second);
		assert (ac);
		if (ac->alist()->automation_state() != Play) {
			_filtered_parameters.insert (ac->parameter ());
		}
	}

	/* watch for changes to controls' AutoState */
	midi_source()->AutomationStateChanged.connect_same_thread (
		_model_connection, std::bind (&MidiRegion::model_automation_state_changed, this, _1)
		);

	model()->ContentsShifted.connect_same_thread (_model_shift_connection, std::bind (&MidiRegion::model_shifted, this, _1));
	model()->ContentsChanged.connect_same_thread (_model_changed_connection, std::bind (&MidiRegion::model_contents_changed, this));
}

void
MidiRegion::model_contents_changed ()
{
	send_change (Properties::contents);
}

void
MidiRegion::model_shifted (timecnt_t distance)
{
	if (!model()) {
		return;
	}

	if (!_ignore_shift) {
		PropertyChange what_changed;
		/* _length is a Property, so we cannot call timepos_t methods on
		   it directly. ::val() only provides a const, so use
		   operator+() rather than operator+=()
		*/
		_length = _length.val() + distance;
		if (!_start.val().is_zero()) {
			_length = _length.val() + _start.val();
			_start = timepos_t::zero (_start.val().time_domain());
			what_changed.add (Properties::start);
		}
		what_changed.add (Properties::contents);
		what_changed.add (Properties::length);
		send_change (what_changed);
	} else {
		_ignore_shift = false;
	}
}

void
MidiRegion::model_automation_state_changed (Evoral::Parameter const & p)
{
	/* Update our filtered parameters list after a change to a parameter's AutoState */

	std::shared_ptr<AutomationControl> ac = model()->automation_control (p);
	if (!ac || ac->alist()->automation_state() == Play) {
		/* It should be "impossible" for ac to be NULL, but if it is, don't
		   filter the parameter so events aren't lost. */
		_filtered_parameters.erase (p);
	} else {
		_filtered_parameters.insert (p);
	}

	/* the source will have an iterator into the model, and that iterator will have been set up
	   for a given set of filtered_parameters, so now that we've changed that list we must invalidate
	   the iterator.
	*/
	Source::WriterLock lm (midi_source(0)->mutex(), Glib::Threads::TRY_LOCK);
	if (lm.locked()) {
		/* TODO: This is too aggressive, we need more fine-grained invalidation. */
		midi_source(0)->invalidate (lm);
	}
}

/** This is called when a trim drag has resulted in a -ve _start time for this region.
 *  Fix it up by adding some empty space to the source.
 */
void
MidiRegion::fix_negative_start (HistoryOwner& history)
{
	_ignore_shift = true;

	model()->insert_silence_at_start (-start().beats(), history);

	_start = timepos_t::zero (start().time_domain());
}

bool
MidiRegion::set_name (const std::string& str)
{
	if (_name == str) {
		return true;
	}

	if (!Session::session_name_is_legal (str).empty ()) {
		return false;
	}

	return Region::set_name (str);
}

void
MidiRegion::merge (std::shared_ptr<MidiRegion const> other_region)
{
	std::shared_ptr<MidiModel const> self  = model ();
	std::shared_ptr<MidiModel const> other = other_region->model();

	Temporal::Beats other_region_start (other_region->start().beats());
	Temporal::Beats other_region_end ((other_region->start() + other_region->length()).beats());

	Evoral::Sequence<Temporal::Beats>::const_iterator e1 = self->begin();
	Evoral::Sequence<Temporal::Beats>::const_iterator e2 = other->begin();

	Source::WriterLock sl (midi_source()->mutex ());
	midi_source ()->drop_model (sl);
	std::shared_ptr<MidiModel> new_model (new MidiModel (*midi_source()));
	new_model->start_write ();

	/* Note: merge() is called with sorted region list so that
	 * self->position <= other->position ()
	 */

	assert (position() <= other_region->position ());

	while ((e1 != self->end ()) || (e2 != other->end ())) {
		/* map time from other region to this region */
		Temporal::Beats e2t;
		if (e2 != other->end ()) {
			timepos_t abs_time (other_region->source_beats_to_absolute_time (e2->time()));
			e2t = position().distance (abs_time).beats();
		}

		if (e2 == other->end () || (e1 != self->end () && e1->time() < e2t)) {
			Evoral::Event<Temporal::Beats> ev (*e1, true);
			new_model->append (ev, Evoral::next_event_id());
			++e1;
			continue;
		}

		if (e1 == self->end () || (e2 != other->end () && e1->time() >= e2t)) {
			if (e2->time() < other_region_start) {
				++e2;
				continue;
			}
			if (e2->time() > other_region_end) {
				/* No more events form other region */
				e2 = other->end ();
				continue;
			}

			Evoral::Event<Temporal::Beats> ev (*e2, true);
			ev.set_time (e2t);
			new_model->append (ev, Evoral::next_event_id());
			++e2;
			continue;
		}

		assert (0);
		break;
	}

	new_model->end_write (Evoral::Sequence<Temporal::Beats>::ResolveStuckNotes, length().beats());
	midi_source()->set_model (sl, new_model);

	set_length (max (length(), position().distance (other_region->end())));
}

void
MidiRegion::start_domain_bounce (Temporal::DomainBounceInfo& cmd)
{
	/* Deal with the region position & length */

	Region::start_domain_bounce (cmd);
	if (cmd.from != Temporal::BeatTime) {
		return;
	}

	model()->start_domain_bounce (cmd);
	model()->create_mapping_stash (source_position().beats());
}

void
MidiRegion::finish_domain_bounce (Temporal::DomainBounceInfo& cmd)
{
	Region::finish_domain_bounce (cmd);

	if (cmd.from != Temporal::BeatTime) {
		return;
	}

	model()->rebuild_from_mapping_stash (source_position().beats());
	model()->finish_domain_bounce (cmd);

	_model_changed_connection.disconnect ();
	model()->ContentsChanged ();
	model()->ContentsChanged.connect_same_thread (_model_changed_connection, std::bind (&MidiRegion::model_contents_changed, this));
}
