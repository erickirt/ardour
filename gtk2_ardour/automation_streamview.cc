/*
 * Copyright (C) 2007-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2008-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2014-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015 Tim Mayberry <mojofunk@gmail.com>
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
#include <list>
#include <utility>

#include <ytkmm/ytkmm.h>

#include "gtkmm2ext/gtk_ui.h"

#include "pbd/compose.h"
#include "canvas/debug.h"

#include "ardour/midi_region.h"
#include "ardour/midi_source.h"

#include "automation_region_view.h"
#include "automation_streamview.h"
#include "automation_time_axis.h"
#include "gui_thread.h"
#include "public_editor.h"
#include "region_selection.h"
#include "region_view.h"
#include "rgb_macros.h"
#include "selection.h"
#include "ui_config.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace ARDOUR_UI_UTILS;
using namespace PBD;
using namespace Editing;

AutomationStreamView::AutomationStreamView (AutomationTimeAxisView& tv)
	: StreamView (*dynamic_cast<RouteTimeAxisView*>(tv.get_parent()),
	              tv.canvas_display())
	, _automation_view(tv)
	, _pending_automation_state (Off)
{
	CANVAS_DEBUG_NAME (_canvas_group, string_compose ("SV canvas group auto %1", tv.name()));
	CANVAS_DEBUG_NAME (canvas_rect, string_compose ("SV canvas rectangle auto %1", tv.name()));

	color_handler ();

	UIConfiguration::instance().ColorsChanged.connect(sigc::mem_fun(*this, &AutomationStreamView::color_handler));
}

AutomationStreamView::~AutomationStreamView ()
{
}

RegionView*
AutomationStreamView::add_region_view_internal (std::shared_ptr<Region> region, bool /*wait_for_data*/, bool /*recording*/)
{
	if (!region) {
		return nullptr;
	}

	std::shared_ptr<AutomationList> list;

	const std::shared_ptr<AutomationControl> control = std::dynamic_pointer_cast<AutomationControl> (
		region->control (_automation_view.parameter(), true)
		);

	if (control) {
		list = std::dynamic_pointer_cast<AutomationList>(control->list());
		if (control->list() && !list) {
			error << _("unable to display automation region for control without list") << endmsg;
			return nullptr;
		}
	}

	RegionView *region_view;

	for (auto const & rv : region_views) {
		if (rv->region() == region) {

			/* great. we already have an AutomationRegionView for this Region. use it again. */
			AutomationRegionView* arv = dynamic_cast<AutomationRegionView*>(rv);;

			if (arv->line()) {
				arv->line()->set_list (list);
			}
			rv->set_valid (true);
			display_region (arv);

			return nullptr;
		}
	}

	region_view = new AutomationRegionView (
		_canvas_group, _automation_view, region,
		_automation_view.parameter (), list,
		_samples_per_pixel, region_color
		);

	region_view->init (false);
	region_views.push_front (region_view);

	/* follow global waveform setting */

	display_region (region_view);

	/* catch regionview going away */
	region->DropReferences.connect (*this, invalidator (*this), std::bind (&AutomationStreamView::remove_region_view, this, std::weak_ptr<Region>(region)), gui_context());

	/* setup automation state for this region */
	if (_automation_view.parameter().type() != MidiVelocityAutomation) {
		std::shared_ptr<EditorAutomationLine> line = dynamic_cast<AutomationRegionView*>(region_view)->line ();
		if (line && line->the_list()) {
			line->the_list()->set_automation_state (automation_state ());
		}
	}

	RegionViewAdded (region_view);

	return region_view;
}

void
AutomationStreamView::display_region (RegionView* region_view)
{
	if (_automation_view.parameter().type() != MidiVelocityAutomation) {
		dynamic_cast<AutomationRegionView*>(region_view)->line().reset();
	}
}

void
AutomationStreamView::set_automation_state (AutoState state)
{
	/* Setting the automation state for this view sets the state of all regions' lists to the same thing */

	if (region_views.empty()) {
		_pending_automation_state = state;
	} else {
		for (auto & line : get_lines()) {
			if (line->the_list()) {
				line->the_list()->set_automation_state (state);
			}
		}
	}
}

void
AutomationStreamView::redisplay_track ()
{
	vector<RegionView::DisplaySuspender> vds;
	// Flag region views as invalid and disable drawing
	for (list<RegionView*>::iterator i = region_views.begin(); i != region_views.end(); ++i) {
		(*i)->set_valid (false);
		vds.push_back (RegionView::DisplaySuspender (**i));

	}

	// Add and display region views, and flag them as valid
	if (_trackview.is_track()) {
		_trackview.track()->playlist()->foreach_region (
			sigc::hide_return (sigc::mem_fun (*this, &StreamView::add_region_view))
			);
	}

	// Stack regions by layer, and remove invalid regions
	layer_regions();
}


void
AutomationStreamView::setup_rec_box ()
{
}

void
AutomationStreamView::color_handler ()
{
	if (_trackview.is_midi_track()) {
		canvas_rect->set_fill_color (UIConfiguration::instance().color_mod ("midi track base", "midi track base"));
	} else {
		canvas_rect->set_fill_color (UIConfiguration::instance().color ("midi bus base"));
	}
}

AutoState
AutomationStreamView::automation_state () const
{
	if (region_views.empty()) {
		return _pending_automation_state;
	}

	std::shared_ptr<EditorAutomationLine> line = ((AutomationRegionView*) region_views.front())->line ();
	if (!line || !line->the_list()) {
		return Off;
	}

	return line->the_list()->automation_state ();
}

bool
AutomationStreamView::has_automation () const
{
	for (auto const & line : get_lines()) {
		if (line->npoints() > 0) {
			return true;
		}
	}

	return false;
}

/** Our parent AutomationTimeAxisView calls this when the user requests a particular
 *  InterpolationStyle; tell the AutomationLists in our regions.
 */
void
AutomationStreamView::set_interpolation (AutomationList::InterpolationStyle s)
{
	for (auto & line : get_lines()) {
		line->the_list()->set_interpolation (s);
	}
}

AutomationList::InterpolationStyle
AutomationStreamView::interpolation () const
{
	if (region_views.empty()) {
		return AutomationList::Linear;
	}

	AutomationRegionView* v = dynamic_cast<AutomationRegionView*> (region_views.front());
	if (v) {
		return v->line()->the_list()->interpolation ();
	}
	return AutomationList::Linear;
}

/** Clear all automation displayed in this view */
void
AutomationStreamView::clear ()
{
	for (auto & line : get_lines()) {
		line->clear ();
	}
}

/** @param start Start position in session samples.
 *  @param end End position in session samples.
 *  @param bot Bottom position expressed as a fraction of track height where 0 is the bottom of the track.
 *  @param top Top position expressed as a fraction of track height where 0 is the bottom of the track.
 *  NOTE: this y system is different to that for the StreamView method that this overrides, which is a little
 *  confusing.
 */
void
AutomationStreamView::_get_selectables (timepos_t const & start, timepos_t const & end, double botfrac, double topfrac, list<Selectable*>& results, bool /*within*/)
{
	for (list<RegionView*>::iterator i = region_views.begin(); i != region_views.end(); ++i) {
		AutomationRegionView* arv = dynamic_cast<AutomationRegionView*> (*i);
		if (arv) {
			arv->line()->get_selectables (start, end, botfrac, topfrac, results);
		}
	}
}

void
AutomationStreamView::set_selected_points (PointSelection& ps)
{
	for (auto & line : get_lines()) {
		line->set_selected_points (ps);
	}
}

list<std::shared_ptr<AutomationLine> >
AutomationStreamView::get_lines () const
{
	list<std::shared_ptr<AutomationLine> > lines;

	for (auto const & rv : region_views) {
		AutomationRegionView* arv = dynamic_cast<AutomationRegionView*> (rv);
		if (arv) {
			lines.push_back (arv->line());
		}
	}

	return lines;
}

bool
AutomationStreamView::paste (timepos_t const &                         pos,
                             unsigned                                  paste_count,
                             float                                     times,
                             std::shared_ptr<ARDOUR::AutomationList> alist)
{
	/* XXX: not sure how best to pick this; for now, just use the last region which starts before pos */

	if (region_views.empty()) {
		return false;
	}

	region_views.sort (RegionView::PositionOrder());

	list<RegionView*>::const_iterator prev = region_views.begin ();

	for (list<RegionView*>::const_iterator i = region_views.begin(); i != region_views.end(); ++i) {
		if ((*i)->region()->position() > pos) {
			break;
		}
		prev = i;
	}

	std::shared_ptr<Region> r = (*prev)->region ();

	/* If *prev doesn't cover pos, it's no good */
	if (r->position() > pos || ((r->position() + r->length()) < pos)) {
		return false;
	}

	AutomationRegionView* arv = dynamic_cast<AutomationRegionView*> (*prev);
	return arv ? arv->paste(pos, paste_count, times, alist) : false;
}
