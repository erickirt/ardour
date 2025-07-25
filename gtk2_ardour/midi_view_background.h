/*
 * Copyright (C) 2006-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2007 Doug McLain <doug@nostar.net>
 * Copyright (C) 2008-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2016-2017 Robin Gareus <robin@gareus.org>
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

#ifndef __gtk2_ardour_midi_view_background_h__
#define __gtk2_ardour_midi_view_background_h__

#include <cstdint>

#include <ytkmm/adjustment.h>

#include "pbd/signals.h"

#include "ardour/types.h"

#include "gtkmm2ext/colors.h"

#include "view_background.h"

namespace ARDOUR {
	class InstrumentInfo;
}

namespace ArdourCanvas {
	class Item;
	class RectSet;
}

class EditingContext;

/** A class that provides various context for a MidiVieww:
        = note ranges
        * color information
        * etc.
 */

class MidiViewBackground : public virtual ViewBackground
{
  public:
	MidiViewBackground (ArdourCanvas::Item* parent, EditingContext& ec);
	~MidiViewBackground ();

	Gtk::Adjustment note_range_adjustment;

	struct NoteRangeSuspender {
		NoteRangeSuspender (MidiViewBackground& mv) : mvb (mv) {
			mvb.NoteRangeChanged.block ();
		}

		~NoteRangeSuspender() {
			mvb.NoteRangeChanged.unblock ();
			mvb.NoteRangeChanged(); /* EMIT SIGNAL */
		}

		MidiViewBackground& mvb;
	};

	enum VisibleNoteRange {
		FullRange,
		ContentsRange
	};

	ARDOUR::NoteMode  note_mode() const { return _note_mode; }
	void set_note_mode (ARDOUR::NoteMode nm);
	sigc::signal<void> NoteModeChanged;

	ARDOUR::ColorMode color_mode() const { return _color_mode; }
	void set_color_mode (ARDOUR::ColorMode);

	Gtkmm2ext::Color region_color() const { return _region_color; }

	void set_note_visibility_range_style (VisibleNoteRange r);
	VisibleNoteRange visibility_range_style() const { return _visibility_note_range; }

	inline uint8_t lowest_note()  const { return _lowest_note; }
	inline uint8_t highest_note() const { return _highest_note; }

	void maybe_extend_note_range (uint8_t note_num);

	int note_height() const {
		return (int) ceil ((double) contents_height() / contents_note_range());
	}

	int note_to_y (uint8_t note) const {
		return (highest_note() - note) * note_height();
	}

	uint8_t y_to_note (int y) const;

	uint8_t contents_note_range() const {
		return highest_note() - lowest_note();
	}

	sigc::signal<void> NoteRangeChanged;

	enum RangeCanMove {
		CanMoveTop = 0x1,
		CanMoveBottom = 0x2
	};

	bool apply_note_range (uint8_t lowest, uint8_t highest, bool to_children, RangeCanMove = RangeCanMove (CanMoveTop|CanMoveBottom));
	void maybe_apply_note_range (uint8_t lowest, uint8_t highest, bool to_children, RangeCanMove = RangeCanMove (CanMoveTop|CanMoveBottom));

	/** @return y position, or -1 if hidden */
	virtual int y_position () const { return 0; }

	virtual uint8_t get_preferred_midi_channel () const = 0;
	virtual void set_note_highlight (bool) = 0;
	virtual void record_layer_check (std::shared_ptr<ARDOUR::Region>, samplepos_t) = 0;

	virtual void set_size (int w, int h) {}
	PBD::Signal<void()> HeightChanged;

	virtual ARDOUR::InstrumentInfo* instrument_info() const = 0;

	void get_note_positions (std::vector<int>& numbers, std::vector<int>& pos, std::vector<int>& heights) const;

	EditingContext& editing_context() const { return _editing_context; }

	sigc::signal<void,bool> NoteVisibilityShouldChange;

	bool update_data_note_range (uint8_t min, uint8_t max);

  protected:
	EditingContext&           _editing_context;
	bool                      _range_dirty;
	double                    _range_sum_cache;
	uint8_t                   _lowest_note;   ///< currently visible
	uint8_t                   _highest_note;  ///< currently visible
	uint8_t                   _data_note_min; ///< in data
	uint8_t                   _data_note_max; ///< in data
	ArdourCanvas::RectSet*    _note_lines;
	ARDOUR::NoteMode          _note_mode;
	Gtkmm2ext::Color          _region_color;
	ARDOUR::ColorMode         _color_mode;
	VisibleNoteRange          _visibility_note_range;
	bool                       note_range_set;

	void color_handler ();
	void parameter_changed (std::string const &);
	void note_range_adjustment_changed();
	void setup_note_lines();
	void update_contents_height ();
	virtual void apply_note_range_to_children () = 0;
	virtual bool updates_suspended() const { return false; }

	void sync_data_and_visual_range ();
};


#endif /* __gtk2_ardour_midi_view_background_h__ */
