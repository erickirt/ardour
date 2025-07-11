/*
 * Copyright (C) 2005-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005 Karsten Wiese <fzuuzf@googlemail.com>
 * Copyright (C) 2005 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2006-2015 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2007-2016 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2013-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2014-2017 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2014-2018 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2015 Colin Fletcher <colin.m.fletcher@googlemail.com>
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

#ifdef WAF_BUILD
#include "gtk2ardour-config.h"
#endif

#include <atomic>
#include <map>

#include <string>
#include <glib.h>
#include <ydk/gdktypes.h>
#include <ytkmm/box.h>
#include <ytkmm/window.h>
#include <ytkmm/actiongroup.h>
#include <ytkmm/notebook.h>
#include <sigc++/signal.h>

#include "pbd/statefuldestructible.h"

#include "temporal/beats.h"

#include "evoral/Note.h"

#include "ardour/session_handle.h"

#include "canvas/fwd.h"

#include "gtkmm2ext/actions.h"
#include "gtkmm2ext/bindings.h"
#include "gtkmm2ext/visibility_tracker.h"

#include "widgets/tabbable.h"

#include "editing.h"
#include "editing_context.h"
#include "selection.h"

namespace Temporal {
	class TempoPoint;
	class MeterPoint;
}

namespace ARDOUR {
	class Session;
	class Region;
	class Playlist;
	class RouteGroup;
	class Trimmable;
	class Movable;
	class Stripable;
	class MidiOperator;
}

namespace Gtk {
	class Container;
	class Menu;
}

class AudioRegionView;
class EditorAutomationLine;
class AutomationTimeAxisView;
class ControlPoint;
class DragManager;
class EditorCursor;
class ArdourMarker;
class MeterMarker;
class MixerStrip;
class MouseCursors;
class RegionView;
class MidiView;
class RouteTimeAxisView;
class Selection;
class SimpleExport;
class StripableTimeAxisView;
class TempoCurve;
class TempoMarker;
class TimeAxisView;
class VerboseCursor;
struct SelectionRect;

class DisplaySuspender;

namespace ARDOUR_UI_UTILS {
bool relay_key_press (GdkEventKey* ev, Gtk::Window* win);
bool forward_key_press (GdkEventKey* ev);
}

using ARDOUR::samplepos_t;
using ARDOUR::samplecnt_t;

/// Representation of the interface of the Editor class

/** This class contains just the public interface of the Editor class,
 * in order to decouple it from the private implementation, so that callers
 * of PublicEditor need not be recompiled if private methods or member variables
 * change.
 */
class PublicEditor : public EditingContext, public ArdourWidgets::Tabbable
{
public:
	PublicEditor ();
	virtual ~PublicEditor ();

	/** @return Singleton PublicEditor instance */
	static PublicEditor& instance () { assert (_instance); return *_instance; }

	virtual bool have_idled() const = 0;
	virtual void first_idle() = 0;

	virtual void setup_tooltips() = 0;

	/** Attach this editor to a Session.
	 * @param s Session to connect to.
	 */
	virtual void set_session (ARDOUR::Session* s) = 0;

	virtual samplepos_t playhead_cursor_sample () const = 0;

	/** Possibly start the audition of a region.
	 *
	 * If \p r is 0, or not an AudioRegion any current audition is cancelled.
	 * If we are currently auditioning \p r , the audition will be cancelled.
	 * Otherwise an audition of \p r will start.
	 *
	 * @param r Region to consider auditioning
	 */
	virtual void consider_auditioning (std::shared_ptr<ARDOUR::Region> r) = 0;

	/* import dialogs -> ardour-ui ?! */
	virtual void external_audio_dialog () = 0;
	virtual void session_import_dialog () = 0;

	virtual void new_region_from_selection () = 0;
	virtual void separate_region_from_selection () = 0;

	virtual void reverse_region () = 0;
	virtual void normalize_region () = 0;
	virtual void pitch_shift_region () = 0;

	virtual void transition_to_rolling (bool fwd) = 0;

	virtual bool get_selection_extents (Temporal::timepos_t &start, Temporal::timepos_t &end) const = 0;

	virtual void set_selection (std::list<Selectable*>, ARDOUR::SelectionOperation) = 0;

	virtual std::shared_ptr<ARDOUR::Route> current_mixer_stripable () const = 0;

	virtual bool extend_selection_to_track (TimeAxisView&) = 0;
	virtual void play_solo_selection(bool restart) = 0;
	virtual void play_selection () = 0;
	virtual void play_with_preroll () = 0;
	virtual void rec_with_preroll () = 0;
	virtual void rec_with_count_in () = 0;
	virtual void maybe_locate_with_edit_preroll (samplepos_t location) = 0;
	virtual void goto_nth_marker (int nth) = 0;
	virtual void jump_to_loop_marker (bool start) = 0;
	virtual void trigger_script (int nth) = 0;
	virtual void add_bbt_marker_at_playhead_cursor () = 0;
	virtual void add_location_from_playhead_cursor () = 0;
	virtual void add_section_from_playhead () = 0;
	virtual void remove_location_at_playhead_cursor () = 0;
	void add_location_mark (Temporal::timepos_t const & where, ARDOUR::Location::Flags flags = ARDOUR::Location::IsMark, int32_t cue_id = 0) {
		add_location_mark_with_flag (where, flags, cue_id);
	}
	virtual void add_location_mark_with_flag (Temporal::timepos_t const & where, ARDOUR::Location::Flags, int32_t cue_id) = 0;
	virtual void remove_tracks () = 0;
	virtual void set_punch_range (Temporal::timepos_t const & start, Temporal::timepos_t const & end, std::string cmd) = 0;

	void jump_forward_to_mark () {
		jump_forward_to_mark_flagged (ARDOUR::Location::Flags (0), ARDOUR::Location::Flags (0), ARDOUR::Location::Flags (0));
	}
	void jump_backward_to_mark () {
		jump_backward_to_mark_flagged (ARDOUR::Location::Flags (0), ARDOUR::Location::Flags (0), ARDOUR::Location::Flags (0));
	}
	virtual void jump_forward_to_mark_flagged (ARDOUR::Location::Flags whitelist, ARDOUR::Location::Flags blacklist, ARDOUR::Location::Flags equalist) = 0;
	virtual void jump_backward_to_mark_flagged (ARDOUR::Location::Flags whitelist, ARDOUR::Location::Flags blacklist, ARDOUR::Location::Flags equalist) = 0;

	virtual void set_session_start_from_playhead () = 0;
	virtual void set_session_end_from_playhead () = 0;

	virtual void toggle_location_at_playhead_cursor () = 0;

	virtual void nudge_forward (bool next, bool force_playhead) = 0;
	virtual void nudge_backward (bool next, bool force_playhead) = 0;

	virtual void playhead_forward_to_grid () = 0;
	virtual void playhead_backward_to_grid () = 0;

	virtual void keyboard_selection_begin ( Editing::EditIgnoreOption = Editing::EDIT_IGNORE_NONE) = 0;
	virtual void keyboard_selection_finish (bool add, Editing::EditIgnoreOption = Editing::EDIT_IGNORE_NONE) = 0;

	virtual void set_punch_start_from_edit_point () = 0;
	virtual void set_punch_end_from_edit_point () = 0;
	virtual void set_loop_start_from_edit_point () = 0;
	virtual void set_loop_end_from_edit_point () = 0;

	virtual Editing::MouseMode effective_mouse_mode () const = 0;

	/** Import existing media */
	virtual void do_import (std::vector<std::string> paths, Editing::ImportDisposition, Editing::ImportMode mode, ARDOUR::SrcQuality,
	                        ARDOUR::MidiTrackNameSource, ARDOUR::MidiTempoMapDisposition, Temporal::timepos_t&,
	                        std::shared_ptr<ARDOUR::PluginInfo> instrument = std::shared_ptr<ARDOUR::PluginInfo>(),
	                        std::shared_ptr<ARDOUR::Track> track = std::shared_ptr<ARDOUR::Track>(),
	                        bool with_markers = false) = 0;
	virtual void do_embed (std::vector<std::string> paths, Editing::ImportDisposition, Editing::ImportMode mode, Temporal::timepos_t&,
	                       std::shared_ptr<ARDOUR::PluginInfo> instrument = std::shared_ptr<ARDOUR::PluginInfo>(),
	                       std::shared_ptr<ARDOUR::Track> track = std::shared_ptr<ARDOUR::Track>()) = 0;

	/** Open main export dialog */
	virtual void export_audio () = 0;

	/** Open stem export dialog */
	virtual void stem_export () = 0;

	/** Open export dialog with current selection pre-selected */
	virtual void export_selection () = 0;

	/** Open export dialog with current range pre-selected */
	virtual void export_range () = 0;

	/** Open Simple Export Dialog */
	virtual void quick_export () = 0;

	virtual void surround_export () = 0;

	virtual void loudness_assistant (bool) = 0;

	virtual void register_actions () = 0;
	virtual void clear_playlist (std::shared_ptr<ARDOUR::Playlist>) = 0;
	virtual void clear_grouped_playlists (RouteUI*) = 0;

	virtual void mapped_select_playlist_matching (RouteUI&, std::weak_ptr<ARDOUR::Playlist> pl) = 0;

	virtual void mapover_grouped_routes (sigc::slot<void, RouteUI&> sl, RouteUI*, PBD::PropertyID) const = 0;
	virtual void mapover_armed_routes (sigc::slot<void, RouteUI&> sl) const = 0;
	virtual void mapover_selected_routes (sigc::slot<void, RouteUI&> sl) const = 0;
	virtual void mapover_all_routes (sigc::slot<void, RouteUI&> sl) const = 0;

	virtual void new_playlists_for_all_tracks(bool copy) = 0;
	virtual void new_playlists_for_grouped_tracks(RouteUI* rui, bool copy) = 0;
	virtual void new_playlists_for_selected_tracks(bool copy) = 0;
	virtual void new_playlists_for_armed_tracks(bool copy) = 0;

	virtual void select_all_visible_lanes () = 0;
	virtual void select_all_tracks () = 0;
	virtual void deselect_all () = 0;
	virtual void invert_selection () = 0;
	virtual void set_selected_track (TimeAxisView&, ARDOUR::SelectionOperation op = ARDOUR::SelectionSet, bool no_remove = false) = 0;
	virtual void set_selected_mixer_strip (TimeAxisView&) = 0;
	virtual void hide_track_in_display (TimeAxisView* tv, bool apply_to_selection = false) = 0;
	virtual void show_track_in_display (TimeAxisView* tv, bool move_into_view = false) = 0;

	virtual void set_stationary_playhead (bool yn) = 0;
	virtual void toggle_stationary_playhead () = 0;
	virtual bool stationary_playhead() const = 0;

	virtual void toggle_cue_behavior () = 0;

	/** @return true if the playhead is currently being dragged, otherwise false */
	virtual bool dragging_playhead () const = 0;
	virtual samplepos_t leftmost_sample() const = 0;
	virtual samplecnt_t current_page_samples() const = 0;
	virtual double visible_canvas_height () const = 0;
	virtual void ensure_time_axis_view_is_visible (TimeAxisView const & tav, bool at_top = false) = 0;
	virtual void override_visible_track_count () = 0;
	virtual void scroll_tracks_down_line () = 0;
	virtual void scroll_tracks_up_line () = 0;
	virtual bool scroll_down_one_track (bool skip_child_views = false) = 0;
	virtual bool scroll_up_one_track (bool skip_child_views = false) = 0;
	virtual void select_topmost_track () = 0;
	virtual void cleanup_regions () = 0;
	virtual void prepare_for_cleanup () = 0;
	virtual void finish_cleanup () = 0;
	virtual void set_visible_track_count (int32_t) = 0;
	virtual void fit_selection () = 0;
	virtual void remove_last_capture () = 0;
	virtual void maximise_editing_space () = 0;
	virtual void restore_editing_space () = 0;
	virtual void toggle_meter_updating() = 0;
	virtual void split_regions_at (Temporal::timepos_t const &, RegionSelection&) = 0;
	virtual void split_region_at_points (std::shared_ptr<ARDOUR::Region>, ARDOUR::AnalysisFeatureList&, bool can_ferret, bool select_new = false) = 0;
	virtual void foreach_time_axis_view (sigc::slot<void,TimeAxisView&>) = 0;
	virtual void add_to_idle_resize (TimeAxisView*, int32_t) = 0;
	virtual Temporal::timecnt_t get_paste_offset (Temporal::timepos_t const & pos, unsigned paste_count, Temporal::timecnt_t const & duration) = 0;

	virtual void queue_visual_videotimeline_update () = 0;
	virtual void set_close_video_sensitive (bool) = 0;
	virtual void toggle_ruler_video (bool) = 0;
	virtual void toggle_xjadeo_proc (int) = 0;
	virtual void toggle_xjadeo_viewoption (int, int) = 0;
	virtual void set_xjadeo_sensitive (bool onoff) = 0;
	virtual int  get_videotl_bar_height () const = 0;
	virtual void set_video_timeline_height (const int h, bool force = false) = 0;
	virtual void embed_audio_from_video (std::string, samplepos_t n = 0, bool lock_position_to_video = true) = 0;

	virtual void trigger_script_by_name (const std::string script_name, const std::string args = "") = 0;

	virtual bool track_selection_change_without_scroll () const = 0;
	virtual bool show_touched_automation () const = 0;

	virtual TimeAxisView* time_axis_view_from_stripable (std::shared_ptr<ARDOUR::Stripable> s) const = 0;

	virtual void get_equivalent_regions (RegionView* rv, std::vector<RegionView*>&, PBD::PropertyID) const = 0;
	virtual RegionView* regionview_from_region (std::shared_ptr<ARDOUR::Region>) const = 0;
	virtual RouteTimeAxisView* rtav_from_route (std::shared_ptr<ARDOUR::Route>) const = 0;

	sigc::signal<void> Realized;
	sigc::signal<void,samplepos_t> UpdateAllTransportClocks;

	virtual bool pending_locate_request() const = 0;

	struct RegionAction {
		Glib::RefPtr<Gtk::Action> action;
		Editing::RegionActionTarget target;

		RegionAction (Glib::RefPtr<Gtk::Action> a, Editing::RegionActionTarget tgt)
			: action (a), target (tgt) {}
	};

	/* data-type of [region] object currently dragged with x-ardour/region.pbdid */
	static ARDOUR::DataType pbdid_dragged_dt;

	std::map<std::string,RegionAction> region_action_map;

	Glib::RefPtr<Gtk::ActionGroup> editor_menu_actions;
	Glib::RefPtr<Gtk::ActionGroup> _region_actions;

	virtual bool canvas_scroll_event (GdkEventScroll* event, bool from_canvas) = 0;
	virtual bool canvas_control_point_event (GdkEvent* event, ArdourCanvas::Item*, ControlPoint*) = 0;
	virtual bool canvas_velocity_event (GdkEvent* event, ArdourCanvas::Item*) = 0;
	virtual bool canvas_velocity_base_event (GdkEvent* event, ArdourCanvas::Item*) = 0;
	virtual bool canvas_line_event (GdkEvent* event, ArdourCanvas::Item*, EditorAutomationLine*) = 0;
	virtual bool canvas_selection_rect_event (GdkEvent* event, ArdourCanvas::Item*, SelectionRect*) = 0;
	virtual bool canvas_selection_start_trim_event (GdkEvent* event, ArdourCanvas::Item*, SelectionRect*) = 0;
	virtual bool canvas_selection_end_trim_event (GdkEvent* event, ArdourCanvas::Item*, SelectionRect*) = 0;
	virtual bool canvas_start_xfade_event (GdkEvent* event, ArdourCanvas::Item*, AudioRegionView*) = 0;
	virtual bool canvas_end_xfade_event (GdkEvent* event, ArdourCanvas::Item*, AudioRegionView*) = 0;
	virtual bool canvas_fade_in_event (GdkEvent* event, ArdourCanvas::Item*, AudioRegionView*) = 0;
	virtual bool canvas_fade_in_handle_event (GdkEvent* event, ArdourCanvas::Item*, AudioRegionView*, bool) = 0;
	virtual bool canvas_fade_out_event (GdkEvent* event, ArdourCanvas::Item*, AudioRegionView*) = 0;
	virtual bool canvas_fade_out_handle_event (GdkEvent* event, ArdourCanvas::Item*, AudioRegionView*, bool) = 0;
	virtual bool canvas_region_view_event (GdkEvent* event, ArdourCanvas::Item*, RegionView*) = 0;
	virtual bool canvas_wave_view_event (GdkEvent* event, ArdourCanvas::Item*, RegionView*) = 0;
	virtual bool canvas_frame_handle_event (GdkEvent* event, ArdourCanvas::Item*, RegionView*) = 0;
	virtual bool canvas_region_view_name_highlight_event (GdkEvent* event, ArdourCanvas::Item*, RegionView*) = 0;
	virtual bool canvas_region_view_name_event (GdkEvent* event, ArdourCanvas::Item*, RegionView*) = 0;
	virtual bool canvas_feature_line_event (GdkEvent* event, ArdourCanvas::Item*, RegionView*) = 0;
	virtual bool canvas_stream_view_event (GdkEvent* event, ArdourCanvas::Item*, RouteTimeAxisView*) = 0;
	virtual bool canvas_marker_event (GdkEvent* event, ArdourCanvas::Item*, ArdourMarker*) = 0;
	virtual bool canvas_videotl_bar_event (GdkEvent* event, ArdourCanvas::Item*) = 0;
	virtual bool canvas_selection_marker_event (GdkEvent* event, ArdourCanvas::Item*) = 0;
	virtual bool canvas_tempo_marker_event (GdkEvent* event, ArdourCanvas::Item*, TempoMarker*) = 0;
	virtual bool canvas_tempo_curve_event (GdkEvent* event, ArdourCanvas::Item*, TempoCurve*) = 0;
	virtual bool canvas_meter_marker_event (GdkEvent* event, ArdourCanvas::Item*, MeterMarker*) = 0;
	virtual bool canvas_bbt_marker_event (GdkEvent* event, ArdourCanvas::Item*, BBTMarker*) = 0;
	virtual bool canvas_automation_track_event(GdkEvent* event, ArdourCanvas::Item*, AutomationTimeAxisView*) = 0;

	static const int window_border_width;
	static const int container_border_width;
	static const int vertical_spacing;
	static const int horizontal_spacing;

	virtual ArdourCanvas::Container* get_trackview_group () const = 0;
	virtual ArdourCanvas::ScrollGroup* get_hscroll_group () const = 0;
	virtual ArdourCanvas::ScrollGroup* get_hvscroll_group () const = 0;
	virtual ArdourCanvas::ScrollGroup* get_cursor_scroll_group () const = 0;
	virtual ArdourCanvas::Container* get_drag_motion_group () const = 0;

	virtual void set_current_trimmable (std::shared_ptr<ARDOUR::Trimmable>) = 0;
	virtual void set_current_movable (std::shared_ptr<ARDOUR::Movable>) = 0;

	virtual TrackViewList const & get_track_views () const = 0;

	virtual MixerStrip* get_current_mixer_strip () const = 0;

	virtual Temporal::TempoMap::WritableSharedPtr begin_tempo_map_edit () = 0;
	virtual void abort_tempo_map_edit () = 0;
	void commit_tempo_map_edit (Temporal::TempoMap::WritableSharedPtr& map, bool with_update = false) {
		_commit_tempo_map_edit (map, with_update);
	}

	virtual Temporal::TempoMap::WritableSharedPtr begin_tempo_mapping (Temporal::DomainBounceInfo&) = 0;
	virtual void abort_tempo_mapping () = 0;
	virtual void commit_tempo_mapping (Temporal::TempoMap::WritableSharedPtr& map) = 0;

	virtual void access_action (const std::string&, const std::string&) = 0;
	virtual void set_toggleaction (const std::string&, const std::string&, bool) = 0;

	virtual bool get_smart_mode () const = 0;

	virtual void get_pointer_position (double &, double &) const = 0;

	virtual std::pair <Temporal::timepos_t, Temporal::timepos_t> session_gui_extents (bool use_extra = true) const = 0;

	virtual void get_regions_at (RegionSelection &, Temporal::timepos_t const & where, TrackViewList const &) const = 0;
	virtual void get_regions_after (RegionSelection&, Temporal::timepos_t const & where, const TrackViewList& ts) const = 0;
	virtual RegionSelection get_regions_from_selection_and_mouse (Temporal::timepos_t const &) = 0;

	virtual void build_region_boundary_cache () = 0;
	virtual void mark_region_boundary_cache_dirty () = 0;

	virtual void mouse_add_new_tempo_event (Temporal::timepos_t where) = 0;
	virtual void mouse_add_new_meter_event (Temporal::timepos_t where) = 0;
	virtual void edit_tempo_section (Temporal::TempoPoint&) = 0;
	virtual void edit_meter_section (Temporal::MeterPoint&) = 0;

	virtual bool should_ripple () const = 0;

	virtual void queue_redisplay_track_views () = 0;

	virtual void set_tempo_curve_range (double& max, double& min) const = 0;

	virtual void start_track_drag (TimeAxisView&, int y, Gtk::Widget&, bool can_change_cursor) = 0;
	virtual void mid_track_drag (GdkEventMotion*, Gtk::Widget&) = 0;
	virtual void end_track_drag () = 0;
	virtual bool track_dragging() const = 0;

	virtual void focus_on_clock() = 0;

	/// Singleton instance, set up by Editor::Editor()

	static PublicEditor* _instance;

	friend bool ARDOUR_UI_UTILS::relay_key_press (GdkEventKey*, Gtk::Window*);
	friend bool ARDOUR_UI_UTILS::forward_key_press (GdkEventKey*);

protected:
	friend class DisplaySuspender;
	virtual void suspend_route_redisplay () = 0;
	virtual void resume_route_redisplay () = 0;
	virtual void _commit_tempo_map_edit (Temporal::TempoMap::WritableSharedPtr&, bool with_update) = 0;

	std::atomic<int> _suspend_route_redisplay_counter;
};

class DisplaySuspender {
	public:
		DisplaySuspender() {
			if (PublicEditor::instance()._suspend_route_redisplay_counter.fetch_add (1) == 0) {
				PublicEditor::instance().suspend_route_redisplay ();
			}
		}
		~DisplaySuspender () {
			if (PBD::atomic_dec_and_test (PublicEditor::instance()._suspend_route_redisplay_counter)) {
				PublicEditor::instance().resume_route_redisplay ();
			}
		}
};

class MainMenuDisabler {
public:
	MainMenuDisabler () {
		/* The global menu bar continues to be accessible to applications
		   with modal dialogs on mac, which means that we need to desensitize
		   all items in the menu bar.
		*/
		ActionManager::disable_active_actions ();
	}

	~MainMenuDisabler () {
		ActionManager::enable_active_actions ();
	}
};

