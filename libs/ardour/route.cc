/*
 * Copyright (C) 2000-2019 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2006-2007 Sampo Savolainen <v2@iki.fi>
 * Copyright (C) 2006-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2006 Jesse Chappell <jesse@essej.net>
 * Copyright (C) 2008-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2011-2012 Sakari Bergen <sakari.bergen@beatwaves.net>
 * Copyright (C) 2013-2015 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2013-2017 John Emmas <john@creativepost.co.uk>
 * Copyright (C) 2013-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2014-2018 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2015-2018 Len Ovens <len@ovenwerks.net>
 * Copyright (C) 2016 Julien "_FrnchFrgg_" RIVAUD <frnchfrgg@free.fr>
 * Copyright (C) 2016 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2017-2018 Johannes Mueller <github@johannes-mueller.org>
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

#ifdef WAF_BUILD
#include "libardour-config.h"
#endif

#include <cmath>
#include <cassert>
#include <algorithm>

#include <glibmm.h>
#include <boost/algorithm/string.hpp>

#include "pbd/xml++.h"
#include "pbd/enumwriter.h"
#include "pbd/locale_guard.h"
#include "pbd/memento_command.h"
#include "pbd/types_convert.h"
#include "pbd/unwind.h"

#include "ardour/amp.h"
#include "ardour/audio_buffer.h"
#include "ardour/audio_track.h"
#include "ardour/audio_port.h"
#include "ardour/audioengine.h"
#ifdef HAVE_BEATBOX
#include "ardour/beatbox.h"
#endif
#include "ardour/boost_debug.h"
#include "ardour/buffer.h"
#include "ardour/buffer_set.h"
#include "ardour/capturing_processor.h"
#include "ardour/debug.h"
#include "ardour/delivery.h"
#include "ardour/disk_reader.h"
#include "ardour/disk_writer.h"
#include "ardour/event_type_map.h"
#include "ardour/gain_control.h"
#include "ardour/graph.h"
#include "ardour/internal_return.h"
#include "ardour/internal_send.h"
#include "ardour/meter.h"
#include "ardour/delayline.h"
#include "ardour/midi_buffer.h"
#include "ardour/midi_port.h"
#include "ardour/midi_track.h"
#include "ardour/monitor_control.h"
#include "ardour/monitor_processor.h"
#include "ardour/monitor_return.h"
#include "ardour/pannable.h"
#include "ardour/panner.h"
#include "ardour/panner_shell.h"
#include "ardour/parameter_descriptor.h"
#include "ardour/phase_control.h"
#include "ardour/plugin_insert.h"
#include "ardour/plugin_manager.h"
#include "ardour/polarity_processor.h"
#include "ardour/port.h"
#include "ardour/port_insert.h"
#include "ardour/processor.h"
#include "ardour/profile.h"
#include "ardour/revision.h"
#include "ardour/route.h"
#include "ardour/route_group.h"
#include "ardour/send.h"
#include "ardour/session.h"
#include "ardour/solo_control.h"
#include "ardour/solo_isolate_control.h"
#include "ardour/surround_return.h"
#include "ardour/surround_send.h"
#include "ardour/triggerbox.h"
#include "ardour/types_convert.h"
#include "ardour/unknown_processor.h"
#include "ardour/utils.h"
#include "ardour/well_known_enum.h"
#include "ardour/vca.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

PBD::Signal<int(std::shared_ptr<Route>, std::shared_ptr<PluginInsert>, Route::PluginSetupOptions )> Route::PluginSetup;

PBD::Signal<void(std::weak_ptr<Route> )> Route::FanOut;

/** Base class for all routable/mixable objects (tracks and busses) */
Route::Route (Session& sess, string name, PresentationInfo::Flag flag, DataType default_type)
	: Stripable (sess, name, PresentationInfo (flag))
	, GraphNode (sess._process_graph)
	, Muteable (sess, name)
	, _active (true)
	, _signal_latency (0)
	, _output_latency (0)
	, _disk_io_point (DiskIOPreFader)
	, _meter_point (MeterPostFader)
	, _pending_meter_point (MeterPostFader)
	, _denormal_protection (false)
	, _recordable (true)
	, _comment_editor_window (0)
	, _have_internal_generator (false)
	, _default_type (default_type)
	, _instrument_fanned_out (false)
	, _loop_location (NULL)
	, _volume_applies_to_output (true)
	, _track_number (0)
	, _strict_io (false)
	, _in_configure_processors (false)
	, _initial_io_setup (false)
	, _in_sidechain_setup (false)
	, _monitor_gain (0)
	, _custom_meter_position_noted (false)
	, _pinmgr_proxy (0)
	, _patch_selector_dialog (0)
{
	processor_max_streams.reset();

	_pending_process_reorder.store (0);
	_pending_listen_change.store (0);
	_pending_surround_send.store (0);
	_pending_signals.store (0);
}

std::weak_ptr<Route>
Route::weakroute () {
	return std::weak_ptr<Route> (std::dynamic_pointer_cast<Route> (shared_from_this ()));
}

int
Route::init ()
{
	/* default master bus to use strict i/o */
	if (is_singleton ()) {
		_strict_io = true;
		_meter_point = _pending_meter_point = MeterOutput;
	}

	/* add standard controls */

	_gain_control.reset (new GainControl (_session, GainAutomation));
	_trim_control.reset (new GainControl (_session, TrimAutomation));
	/* While the route has-a gain-control for consistency with Stripable and VCA
	 * ownership is handed over to the Amp Processor which manages the
	 * state of the Control and AutomationList as part of its
	 * Automatable API. -- Don't call add_control () here.
	 */

	Temporal::TimeDomainProvider const & tdp (*this);

	_solo_control.reset (new SoloControl (_session, X_("solo"), *this, *this, tdp));
	add_control (_solo_control);
	_solo_control->Changed.connect_same_thread (*this, std::bind (&Route::solo_control_changed, this, _1, _2));

	_mute_control.reset (new MuteControl (_session, X_("mute"), *this, tdp));
	add_control (_mute_control);

	_phase_control.reset (new PhaseControl (_session, X_("phase"), tdp));
	add_control (_phase_control);

	_solo_isolate_control.reset (new SoloIsolateControl (_session, X_("solo-iso"), *this, tdp));
	add_control (_solo_isolate_control);

	_solo_safe_control.reset (new SoloSafeControl (_session, X_("solo-safe"), tdp));
	add_control (_solo_safe_control);

	/* panning */

	if (!(_presentation_info.flags() & PresentationInfo::MonitorOut)) {
		_pannable.reset (new Pannable (_session, Temporal::TimeDomainProvider (Config->get_default_automation_time_domain())));
	}

	/* input and output objects */

	_input.reset (new IO (_session, _name, IO::Input, _default_type));
	_output.reset (new IO (_session, _name, IO::Output, _default_type));

	_input->changed.connect_same_thread (*this, std::bind (&Route::input_change_handler, this, _1, _2));
	_input->PortCountChanging.connect_same_thread (*this, std::bind (&Route::input_port_count_changing, this, _1));

	_output->changed.connect_same_thread (*this, std::bind (&Route::output_change_handler, this, _1, _2));
	_output->PortCountChanging.connect_same_thread (*this, std::bind (&Route::output_port_count_changing, this, _1));

	/* add the amp/fader processor.
	 * it should be the first processor to be added on every route.
	 */

	_amp.reset (new Amp (_session, X_("Fader"), _gain_control, true));
	_amp->activate ();
	_amp->set_owner (this);

	_polarity.reset (new PolarityProcessor (_session, _phase_control));
	_polarity->set_owner (this);

	if (!is_surround_master ()) {
		_polarity->activate();
	}

	if (is_monitor ()) {
		_amp->set_display_name (_("Monitor"));
		_gain_control->set_flag (Controllable::MonitorControl);
		_trim_control->set_flag (Controllable::MonitorControl);
	}

	if (!is_singleton () && !is_auditioner ()) {
		_delayline.reset (new DelayLine (_session, name ()));
	}

	/* and input trim */

	_trim.reset (new Amp (_session, X_("Trim"), _trim_control, false));
	_trim->set_display_to_user (false);

	if (is_surround_master ()) {
		_trim->deactivate ();
	} else if (dynamic_cast<AudioTrack*>(this)) {
		/* we can't do this in the AudioTrack's constructor
		 * because _trim does not exit then
		 */
		_trim->activate();
	}
	else if (!dynamic_cast<Track*>(this) && ! (is_monitor() || is_auditioner())) {
		/* regular bus */
		_trim->activate();
	}

	/* create standard processors: meter, main outs, monitor out;
	   they will be added to _processors by setup_invisible_processors ()
	*/

	_meter.reset (new PeakMeter (_session, _name));
	_meter->set_owner (this);
	_meter->set_display_to_user (false);
	_meter->activate ();

	/* set default meter type */
	if (is_master()) {
#ifdef MIXBUS
		set_meter_type (MeterK14);
#else
		set_meter_type (Config->get_meter_type_master ());
#endif
	} else if (dynamic_cast<Track*>(this)) {
		set_meter_type (Config->get_meter_type_track ());
	} else {
		set_meter_type (Config->get_meter_type_bus ());
	}

	_main_outs.reset (new Delivery (_session, _output, _pannable, _mute_master, _name, Delivery::Main));
	/* master output volume */
	if (is_master()) {
		_volume_control.reset (new GainControl (_session, MainOutVolume));
		_volume_control->set_flag (Controllable::NotAutomatable);
		_main_outs->set_gain_control (_volume_control);
		_volume.reset (new Amp (_session, X_("LAN Amp"), _volume_control, false));
		_volume->set_display_to_user (false);
		_volume->deactivate ();
	}
	_main_outs->activate ();

	if (is_monitor()) {
		/* where we listen to tracks */
		_intreturn.reset (new MonitorReturn (_session, tdp));
		_intreturn->activate ();

		/* the thing that provides proper control over a control/monitor/listen bus
		   (such as per-channel cut, dim, solo, invert, etc).
		*/
		_monitor_control.reset (new MonitorProcessor (_session));
		_monitor_control->activate ();
	}

	if (is_surround_master ()) {
		_meter_point = _pending_meter_point = MeterPreFader;
		_surround_return.reset (new SurroundReturn (_session, this));
		_surround_return->set_owner (this);
		_surround_return->activate ();
		panner_shell()->set_bypassed (true);

		_monitor_control.reset (new MonitorProcessor (_session));
		_monitor_control->activate ();
		_output->set_audio_channel_names (std::vector<std::string> {
				_("L"), _("R"), _("C"), _("LFE"), _("Ls"), _("Rs"),
				_("Lrs"), _("Rrs"), _("Lfh"), _("Rfh"), _("Lrh"), _("Rrh"),
				_("BinL"), _("BinR")
				});
	}

	if (_presentation_info.flags() & PresentationInfo::FoldbackBus) {
		panner_shell()->select_panner_by_uri ("http://ardour.org/plugin/panner_balance");
	}

	/* now set up processor chain and invisible processors */
	{
		Glib::Threads::Mutex::Lock lx (AudioEngine::instance()->process_lock ());
		{
			Glib::Threads::RWLock::WriterLock lm (_processor_lock);
			_processors.push_back (_amp);
		}
		if (!_session.loading()) {
			configure_processors (0);
		}
	}

	return 0;
}

Route::~Route ()
{
	DEBUG_TRACE (DEBUG::Destruction, string_compose ("route %1 destructor\n", _name));

	/* do this early so that we don't get incoming signals as we are going through destruction
	 */

	drop_connections ();

	/* don't use clear_processors here, as it depends on the session which may
	   be half-destroyed by now
	*/

	Glib::Threads::RWLock::WriterLock lm (_processor_lock);
	for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
		(*i)->drop_references ();
	}

	_processors.clear ();
}

string
Route::ensure_track_or_route_name (string newname) const
{
	/* remove any colons (and other nasty chars) */
	newname = legalize_for_universal_path (newname);

	/* Some ctrl surfaces may put NULL in the middle of a string.
	 * Ben found out just how much havoc this can wreak. SysEx 1s 4 b1tch. */
	newname.erase (std::remove (newname.begin(), newname.end(), '\0'), newname.end());

	while (!_session.io_name_is_legal (newname)) {
		newname = bump_name_once (newname, ' ');
		if (newname == name()) {
			break;
		}
	}
	return newname;
}

/** Process this route for one (sub) cycle (process thread)
 *
 * @param bufs Scratch buffers to use for the signal path
 * @param start_sample Initial transport sample
 * @param end_sample Final transport sample
 * @param nframes Number of samples to output (to ports)
 *
 * Note that (end_sample - start_sample) may not be equal to nframes when the
 * transport speed isn't 1.0 (eg varispeed).
 */
void
Route::process_output_buffers (BufferSet& bufs,
			       samplepos_t start_sample, samplepos_t end_sample, pframes_t nframes,
			       bool gain_automation_ok, bool run_disk_reader)
{
	/* Caller must hold process lock */
	assert (!AudioEngine::instance()->process_lock().trylock());

	Glib::Threads::RWLock::ReaderLock lm (_processor_lock, Glib::Threads::TRY_LOCK);
	if (!lm.locked()) {
		// can this actually happen?
		// Places that need a WriterLock on (_processor_lock) must also take the process-lock.
		bufs.silence (nframes, 0);
		assert (0); // ...one way to find out.
		return;
	}

	if (_pannable) {
		/* this is only for the benfit of updating the UI.
		 *
		 * Panner's `::distribute_one_automated()` evalualte
		 * a sample-accurate curve using start/end of the
		 * delivery processor.
		 */
		_pannable->automation_run (start_sample, nframes);
	}

	const int speed = (is_auditioner() ? 1 : _session.transport_speed (true));
	assert (speed == -1 || speed == 0 || speed == 1);

	const samplecnt_t output_latency = speed * _output_latency;
	const samplecnt_t latency_offset = speed * (_signal_latency + _output_latency);

	/* Mute is the only actual route-owned control (solo, solo safe, polarity
	 * are not automatable).
	 *
	 * Here we offset mute automation to align to output/master bus
	 * to be consistent with the fader. This applied to the
	 * "Main outs" mute point.
	 *
	 * Other mute points in the middle of signal flow flow
	 * will not be handled correctly. That would mean to add
	 * _signal_latency - accumulated processor effective_latency() at mute mute
	 */

	automation_run (start_sample + output_latency, nframes);

	/* figure out if we're going to use gain automation */
	if (gain_automation_ok) {
		_amp->set_gain_automation_buffer (_session.gain_automation_buffer ());
		_amp->setup_gain_automation (
				start_sample + _amp->output_latency () + output_latency,
				end_sample + _amp->output_latency () + output_latency,
				nframes);

		_trim->set_gain_automation_buffer (_session.trim_automation_buffer ());
		_trim->setup_gain_automation (
				start_sample + _trim->output_latency () + output_latency,
				end_sample + _trim->output_latency () + output_latency,
				nframes);
	}

	/* We align the playhead to output. The user hears what the clock says:
	 * When the playhead/clock says 1:00:00:00 the user will hear the audio sample
	 * at 1:00:00:00. sample_start will be [sample at] 1:00:00:00
	 *
	 * e.g. clock says Time T = 0,  sample_start = 0
	 * Disk-read(play) -> latent-plugin (+10) -> fader-automation -> output (+5)
	 * -> total playback latency "disk -> out" is 15.
	 * -> at Time T= -15, the disk-reader reads sample T=0.
	 * By the Time T=0 is reached (dt=15 later) that sample is audible.
	 */
	start_sample += latency_offset;
	end_sample += latency_offset;

	/* Note: during initial pre-roll 'start_sample' as passed as argument can be negative.
	 * Functions calling process_output_buffers() will set  "run_disk_reader"
	 * to false if the pre-roll count-down is larger than playback_latency ().
	 *
	 * playback_latency() is guaranteed to be <= _signal_latency + _output->latency ()
	 */
	assert (!_disk_reader || !run_disk_reader || start_sample >= 0 || speed < 0);

	/* however the disk-writer may need to pick up output from other tracks
	 * during pre-roll (in particular if this route has latent effects after the disk).
	 *
	 * e.g. track 1 play -> latency A --port--> track2 capture -> latency B ---> out
	 * total pre-roll = A + B.
	 *
	 * Note the disk-writer has built-in overlap detection (it's safe to run it early)
	 * given that
	 */
	bool run_disk_writer = false;
	if (_disk_writer && speed > 0) {
		samplecnt_t latency_preroll = _session.remaining_latency_preroll ();
		run_disk_writer = latency_preroll <= nframes + (_signal_latency + _output_latency);
		if (end_sample - _disk_writer->input_latency () < _session.transport_sample ()) {
			run_disk_writer = true;
		}
	}

	/* Tell main outs what to do about monitoring.  We do this so that
	 * on a transition between monitoring states we get a de-clicking gain
	 * change in the _main_outs delivery, if config.get_use_monitor_fades()
	 * is true.
	 *
	 * We override this in the case where we have an internal generator.
	 *
	 * FIXME: when punching in/out this also depends on latency compensated time
	 * for this route. monitoring_state() does not currently handle that correctly,.
	 *
	 * Also during remaining_latency_preroll, transport_rolling () is false, but
	 * we may need to monitor disk instead.
	 */
	const MonitorState ms = monitoring_state ();
	const bool silent = _have_internal_generator || (_triggerbox && !_triggerbox->empty ()) ? false : (ms == MonitoringSilence);

	_main_outs->no_outs_cuz_we_no_monitor (silent);

	/* -------------------------------------------------------------------------------------------
	   DENORMAL CONTROL
	   ----------------------------------------------------------------------------------------- */
	/* XXX We'll need to protect silent inputs as well as silent disk
	 * (when not monitoring input or monitoring disk and there's no region
	 * for a longer time).
	 *
	 * ...or simply drop that feature.
	 */
	if (_denormal_protection || Config->get_denormal_protection()) {

		for (BufferSet::audio_iterator i = bufs.audio_begin(); i != bufs.audio_end(); ++i) {
			Sample* const sp = i->data();
			for (pframes_t nx = 0; nx < nframes; ++nx) {
				sp[nx] += 1.0e-27f;
			}
		}
	}


	/* -------------------------------------------------------------------------------------------
	   and go ....
	   ----------------------------------------------------------------------------------------- */

	samplecnt_t latency = 0;

	for (auto const & proc : _processors) {

		bool re_inject_oob_data = false;

		if (proc == _disk_reader) {
			/* ignore port-count from prior plugins, use DR's count.
			 * see also Route::try_configure_processors_unlocked
			 */
			bufs.set_count (proc->output_streams());

			/* Well now, we've made it past the disk-writer and to the disk-reader.
			 * Time to decide what to do about monitoring.
			 *
			 * Even when not doing MonitoringDisk, we need to run the processors,
			 * so that it advances its internal buffers (IFF run_disk_reader is true).
			 *
			 */
			if (ms == MonitoringDisk || ms == MonitoringSilence) {
				/* this will clear out-of-band data, too (e.g. MIDI-PC, Panic etc.
				 * OOB data is written at the end of the cycle (nframes - 1),
				 * and jack does not re-order events, so we push them back later */
				re_inject_oob_data = true;
				bufs.silence (nframes, 0);
			}
		}

		double pspeed = speed;
		if ((!run_disk_reader && ((proc == _disk_reader) || (proc == _triggerbox))) || (!run_disk_writer && proc == _disk_writer)) {
			/* run with speed 0, no-roll */
			pspeed = 0;
		}

		/* Note: plugin latency may change. The plugin does inform the session via
		 * processor_latency_changed(). But the session may not yet have gotten around to
		 * update the actual worste-case and update this track's _signal_latency.
		 * So there can be cases where adding up all latencies may not equal _signal_latency.
		 *
		 * Also running a plugin may change the plugin's latency, so we need to
		 * add the latency first. Otherwise this can lead to bistable case
		 * in case of automation playback. e.g.
		 *
		 * cycle 1: run (t): automation (t) = on: -> increase latency
		 * cycle 2: run (t-latency): automation (t-latency) = off -> decrease latency
		 * repeat.
		 */

		if (proc->active ()) {
			if (speed < 0) {
				latency -= proc->effective_latency ();
			} else {
				latency += proc->effective_latency ();
			}
		}

		if (speed < 0) {
			proc->run (bufs, start_sample + latency, end_sample + latency, pspeed, nframes, proc != _processors.back());
		} else {
			proc->run (bufs, start_sample - latency, end_sample - latency, pspeed, nframes, proc != _processors.back());
		}

		bufs.set_count (proc->output_streams());

		if (re_inject_oob_data) {
			write_out_of_band_data (bufs, nframes);
		}

#if 0
		if (proc == _delayline) {
			latency += _delayline->delay ();
		}
#endif
	}
}

void
Route::bounce_process (BufferSet& buffers, samplepos_t start, samplecnt_t nframes,
		std::shared_ptr<Processor> endpoint,
		bool include_endpoint, bool for_export, bool for_freeze)
{
	/* If no processing is required, there's no need to go any further. */
	if (!endpoint && !include_endpoint) {
		return;
	}

	samplecnt_t latency = bounce_get_latency(_amp, false, for_export, for_freeze);
	_amp->set_gain_automation_buffer (_session.gain_automation_buffer ());
	_amp->setup_gain_automation (start - latency, start - latency + nframes, nframes);

	/* trim is always at the top, for bounce no latency compensation is needed */
	_trim->set_gain_automation_buffer (_session.trim_automation_buffer ());
	_trim->setup_gain_automation (start, start + nframes, nframes);

	latency = 0;
	bool seen_disk_io = false;
	for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {

		if (!include_endpoint && (*i) == endpoint) {
			break;
		}

		if (!for_export && !seen_disk_io) {
			if (std::dynamic_pointer_cast<DiskReader> (*i)) {
				seen_disk_io = true;
				buffers.set_count ((*i)->output_streams());
			}
			continue;
		}

		/* if we're *not* exporting, stop processing if we come across a routing processor. */
		if (!for_export && !can_freeze_processor (*i, !for_freeze)) {
			break;
		}

		/* special case the panner (export outputs)
		 * Ideally we'd only run the panner, not the delivery itself...
		 * but panners need separate input/output buffers and some context
		 * (panshell, panner type, etc). AFAICT there is no ill side effect
		 * of re-using the main delivery when freewheeling/exporting a region.
		 */
		if ((*i) == _main_outs) {
			assert ((*i)->does_routing());
			(*i)->run (buffers, start - latency, start - latency + nframes, 1.0, nframes, true);
			buffers.set_count ((*i)->output_streams());
		}

		/* don't run any processors that do routing.
		 * Also don't bother with metering.
		 */
		if (!(*i)->does_routing() && !std::dynamic_pointer_cast<PeakMeter>(*i)) {
			(*i)->run (buffers, start - latency, start - latency + nframes, 1.0, nframes, true);
			buffers.set_count ((*i)->output_streams());
			latency += (*i)->effective_latency ();
		}

		if ((*i) == endpoint) {
			break;
		}
	}
}

samplecnt_t
Route::bounce_get_latency (std::shared_ptr<Processor> endpoint,
		bool include_endpoint, bool for_export, bool for_freeze) const
{
	samplecnt_t latency = 0;
	if (!endpoint && !include_endpoint) {
		return latency;
	}

	bool seen_disk_io = false;
	for (ProcessorList::const_iterator i = _processors.begin(); i != _processors.end(); ++i) {
		if (!include_endpoint && (*i) == endpoint) {
			break;
		}
		if (!for_export && !seen_disk_io) {
			if (std::dynamic_pointer_cast<DiskReader> (*i)) {
				seen_disk_io = true;
			}
			continue;
		}
		if (!for_export && !can_freeze_processor (*i, !for_freeze)) {
			break;
		}
		if (!(*i)->does_routing() && !std::dynamic_pointer_cast<PeakMeter>(*i)) {
			latency += (*i)->effective_latency ();
		}
		if ((*i) == endpoint) {
			break;
		}
	}
	return latency;
}

ChanCount
Route::bounce_get_output_streams (ChanCount &cc, std::shared_ptr<Processor> endpoint,
		bool include_endpoint, bool for_export, bool for_freeze) const
{
	if (!endpoint && !include_endpoint) {
		return cc;
	}

	Glib::Threads::RWLock::ReaderLock lm (_processor_lock);

	for (ProcessorList::const_iterator i = _processors.begin(); i != _processors.end(); ++i) {
		if (!include_endpoint && (*i) == endpoint) {
			break;
		}
		if (!for_export && !can_freeze_processor (*i, !for_freeze)) {
			break;
		}
		if (!(*i)->does_routing() && !std::dynamic_pointer_cast<PeakMeter>(*i)) {
			cc = (*i)->output_streams();
		} else if (*i == _main_outs) {
			cc = (*i)->output_streams();
		}
		if ((*i) == endpoint) {
			break;
		}
	}
	return cc;
}

ChanCount
Route::n_process_buffers ()
{
	return max (_input->n_ports(), processor_max_streams);
}

void
Route::monitor_run (samplepos_t start_sample, samplepos_t end_sample, pframes_t nframes)
{
	assert (is_monitor());
	Glib::Threads::RWLock::ReaderLock lm (_processor_lock, Glib::Threads::TRY_LOCK);
	run_route (start_sample, end_sample, nframes, true, false);
}

void
Route::process ()
{
	_graph->process_one_route (this);
}

void
Route::run_route (samplepos_t start_sample, samplepos_t end_sample, pframes_t nframes, bool gain_automation_ok, bool run_disk_reader)
{
	BufferSet& bufs (_session.get_route_buffers (n_process_buffers()));

	fill_buffers_with_input (bufs, _input, nframes);

	/* filter captured data before meter sees it */
	filter_input (bufs);

	if (is_monitor()) {
		/* control/monitor bus ignores input ports when something is
		 * feeding the listen "stream". data will "arrive" into the
		 * route from the intreturn processor element.
		 */
		gain_t monitor_target_gain = _session.listening() && !_session.is_auditioning() ? 0. : 1.;
		_monitor_gain = Amp::apply_gain (bufs, _session.nominal_sample_rate (), nframes, _monitor_gain, monitor_target_gain);
	}

	snapshot_out_of_band_data (nframes);
	/* append immediate messages to the first MIDI buffer (thus sending it to the first output port) */

	write_out_of_band_data (bufs, nframes);

	/* run processor chain */

	process_output_buffers (bufs, start_sample, end_sample, nframes, gain_automation_ok, run_disk_reader);

	/* map events (e.g. MIDI-CC) back to control-parameters */
	update_controls (bufs);

	flush_processor_buffers_locked (nframes);
}

void
Route::set_listen (bool yn)
{
	if (_monitor_send) {
		if (_monitor_send->active() == yn) {
			return;
		}
		if (yn) {
			_monitor_send->activate ();
		} else {
			_monitor_send->deactivate ();
		}
	}
}

void
Route::solo_control_changed (bool, Controllable::GroupControlDisposition)
{
	/* nothing to do if we're not using AFL/PFL. But if we are, we need
	   to alter the active state of the monitor send.
	*/

	if (Config->get_solo_control_is_listen_control ()) {
		set_listen (_solo_control->self_soloed() || _solo_control->get_masters_value());
	}
}

void
Route::push_solo_isolate_upstream (int32_t delta)
{
	/* forward propagate solo-isolate status to everything fed by this route, but not those via sends only */

	std::shared_ptr<RouteList const> routes = _session.get_routes ();
	for (auto const& i : *routes) {

		if (i.get() == this || !i->can_solo()) {
			continue;
		}

		if (feeds (i)) {
			i->solo_isolate_control()->mod_solo_isolated_by_upstream (delta);
		}
	}
}

void
Route::push_solo_upstream (int delta)
{
	DEBUG_TRACE (DEBUG::Solo, string_compose("\t ... INVERT push from %1\n", _name));
	for (auto const& i : _session._current_route_graph.to (std::dynamic_pointer_cast<Route> (shared_from_this ()))) {
		std::shared_ptr<Route> r = std::dynamic_pointer_cast<Route> (i);
		if (r) {
			r->solo_control()->mod_solo_by_others_downstream (-delta);
		}
	}
}

#if 0
static void
dump_processors(const string& name, const list<std::shared_ptr<Processor> >& procs)
{
	cerr << name << " {" << endl;
	for (list<std::shared_ptr<Processor> >::const_iterator p = procs.begin();
			p != procs.end(); ++p) {
		cerr << "\t" << (*p)->name() << " ID = " << (*p)->id() << " @ " << (*p) << endl;
	}
	cerr << "}" << endl;
}
#endif

/** Supposing that we want to insert a Processor at a given Placement, return
 *  the processor to add the new one before (or 0 to add at the end).
 */
std::shared_ptr<Processor>
Route::before_processor_for_placement (Placement p)
{
	Glib::Threads::RWLock::ReaderLock lm (_processor_lock);

	ProcessorList::iterator loc;

	if (p == PreFader) {
		/* generic pre-fader: insert immediately before the amp */
		loc = find (_processors.begin(), _processors.end(), _amp);
	} else {
		/* generic post-fader: insert right before the main outs */
		loc = find (_processors.begin(), _processors.end(), _main_outs);
	}

	return loc != _processors.end() ? *loc : std::shared_ptr<Processor> ();
}

/** Supposing that we want to insert a Processor at a given index, return
 *  the processor to add the new one before (or -1 to add at the end).
 */
std::shared_ptr<Processor>
Route::before_processor_for_index (int index)
{
	if (index == -1) {
		return std::shared_ptr<Processor> ();
	}

	Glib::Threads::RWLock::ReaderLock lm (_processor_lock);

	ProcessorList::iterator i = _processors.begin ();
	int j = 0;
	while (i != _processors.end() && (!(*i)->display_to_user() || j < index)) {
		if ((*i)->display_to_user()) {
			++j;
		}

		++i;
	}

	return (i != _processors.end() ? *i : std::shared_ptr<Processor> ());
}

/** Add a processor either pre- or post-fader
 *  @return 0 on success, non-0 on failure.
 */
int
Route::add_processor (std::shared_ptr<Processor> processor, Placement placement, ProcessorStreams* err, bool activation_allowed)
{
	return add_processor (processor, before_processor_for_placement (placement), err, activation_allowed);
}


/** Add a processor to a route such that it ends up with a given index into the visible processors.
 *  @param index Index to add the processor at, or -1 to add at the end of the list.
 *  @return 0 on success, non-0 on failure.
 */
int
Route::add_processor_by_index (std::shared_ptr<Processor> processor, int index, ProcessorStreams* err, bool activation_allowed)
{
	return add_processor (processor, before_processor_for_index (index), err, activation_allowed);
}

/** Add a processor to the route.
 *  @param before An existing processor in the list, or 0; the new processor will be inserted immediately before it (or at the end).
 *  @return 0 on success, non-0 on failure.
 */
int
Route::add_processor (std::shared_ptr<Processor> processor, std::shared_ptr<Processor> before, ProcessorStreams* err, bool activation_allowed)
{
	assert (processor != _meter);
	assert (processor != _main_outs);

	DEBUG_TRACE (DEBUG::Processors, string_compose (
		             "%1 adding processor %2\n", name(), processor->display_name()));

	ProcessorList pl;

	pl.push_back (processor);
	int rv = add_processors (pl, before, err);

	if (rv) {
		return rv;
	}

	if (activation_allowed && (!_session.get_bypass_all_loaded_plugins () || !processor->display_to_user ())) {
		processor->activate ();
	}

	return 0;
}

void
Route::processor_selfdestruct (std::weak_ptr<Processor> wp)
{
	/* We cannot destruct the processor here (usually RT-thread
	 * with various locks held - in case of sends also io_locks).
	 * Queue for deletion in low-priority thread.
	 */
	Glib::Threads::Mutex::Lock lx (selfdestruct_lock);
	selfdestruct_sequence.push_back (wp);
}

bool
Route::add_processor_from_xml_2X (const XMLNode& node, int version)
{
	XMLProperty const * prop;

	try {
		std::shared_ptr<Processor> processor;

		/* bit of a hack: get the `placement' property from the <Redirect> tag here
		   so that we can add the processor in the right place (pre/post-fader)
		*/

		XMLNodeList const & children = node.children ();
		XMLNodeList::const_iterator i = children.begin ();

		while (i != children.end() && (*i)->name() != X_("Redirect")) {
			++i;
		}

		Placement placement = PreFader;

		if (i != children.end()) {
			if ((prop = (*i)->property (X_("placement"))) != 0) {
				placement = Placement (string_2_enum (prop->value(), placement));
			}
		}

		if (node.name() == "Insert") {

			if ((prop = node.property ("type")) != 0) {

				if (prop->value() == "ladspa" || prop->value() == "Ladspa" ||
						prop->value() == "lv2" ||
						prop->value() == "windows-vst" ||
						prop->value() == "mac-vst" ||
						prop->value() == "lxvst" ||
						prop->value() == "audiounit") {

					if (_session.get_disable_all_loaded_plugins ()) {
						processor.reset (new UnknownProcessor (_session, node, this));
					} else {
						processor.reset (new PluginInsert (_session, *this));
					}

				} else {
					processor.reset (new PortInsert (_session, _pannable, _mute_master));
				}
			}

		} else if (node.name() == "Send") {

			std::shared_ptr<Pannable> sendpan (new Pannable (_session, Temporal::TimeDomainProvider (Config->get_default_automation_time_domain())));
			processor.reset (new Send (_session, sendpan, _mute_master));

		} else {

			error << string_compose(_("unknown Processor type \"%1\"; ignored"), node.name()) << endmsg;
			return false;
		}

		processor->set_owner (this);

		if (processor->set_state (node, version)) {
			return false;
		}

		//A2 uses the "active" flag in the toplevel redirect node, not in the child plugin/IO
		if (i != children.end()) {
			if ((prop = (*i)->property (X_("active"))) != 0) {
				if (string_to<bool> (prop->value()) && (!_session.get_bypass_all_loaded_plugins () || !processor->display_to_user ()))
					processor->activate();
				else
					processor->deactivate();
			}
		}

		return (add_processor (processor, placement, 0, false) == 0);
	}

	catch (failed_constructor &err) {
		warning << _("processor could not be created. Ignored.") << endmsg;
		return false;
	}
}


inline Route::PluginSetupOptions operator|= (Route::PluginSetupOptions& a, const Route::PluginSetupOptions& b) {
	return a = static_cast<Route::PluginSetupOptions> (static_cast <int>(a) | static_cast<int> (b));
}

inline Route::PluginSetupOptions operator&= (Route::PluginSetupOptions& a, const Route::PluginSetupOptions& b) {
	return a = static_cast<Route::PluginSetupOptions> (static_cast <int>(a) & static_cast<int> (b));
}

int
Route::add_processors (const ProcessorList& others, std::shared_ptr<Processor> before, ProcessorStreams* err)
{
	ProcessorList::iterator loc;
	std::shared_ptr <PluginInsert> fanout;

	if (_pending_process_reorder.load () || _pending_listen_change.load ()) {
		/* we need to flush any pending re-order changes */
		Glib::Threads::Mutex::Lock lx (AudioEngine::instance()->process_lock ());
		apply_processor_changes_rt ();
	}

	if (before) {
		loc = find(_processors.begin(), _processors.end(), before);
		if (loc == _processors.end ()) {
			return 1;
		}
	} else {
		/* nothing specified - at end */
		loc = _processors.end ();
	}

	if (others.empty()) {
		return 0;
	}

	ProcessorList to_skip;

	// check if there's an instrument to replace or configure
	for (ProcessorList::const_iterator i = others.begin(); i != others.end(); ++i) {

		std::shared_ptr<PluginInsert> pi;
		if ((pi = std::dynamic_pointer_cast<PluginInsert>(*i)) == 0) {
			continue;
		}
		if (!pi->plugin ()->get_info ()->is_instrument ()) {
			continue;
		}
		if (std::dynamic_pointer_cast<TriggerBox>(*i)) {
			/* triggerbox looks a lot like an instrument, but it
			   isn't a replacement for an instrument
			*/
			continue;
		}
		std::shared_ptr<Processor> instrument = the_instrument ();
		ChanCount in (DataType::MIDI, 1);
		ChanCount out (DataType::AUDIO, 2); // XXX route's out?!

		PluginSetupOptions flags = None;
		if (instrument) {
			flags |= CanReplace;
			in = instrument->input_streams ();
			out = instrument->output_streams ();
		}
		if (pi->has_output_presets (in, out)) {
			flags |= MultiOut;
		}

		pi->set_strict_io (_strict_io);

		if (is_auditioner()) {
			continue;
		}

		PluginSetupOptions mask = None;
		if (Config->get_ask_replace_instrument ()) {
			mask |= CanReplace;
		}
		if (Config->get_ask_setup_instrument ()) {
			mask |= MultiOut;
		}

		flags &= mask;

		if (flags != None) {
			std::optional<int> rv = PluginSetup (std::dynamic_pointer_cast<Route>(shared_from_this ()), pi, flags);  /* EMIT SIGNAL */
			int mode = rv.value_or (0);
			switch (mode & 3) {
				case 1:
					to_skip.push_back (*i); // don't add this one;
					break;
				case 2:
					replace_processor (instrument, *i, err);
					to_skip.push_back (*i);
					break;
				default:
					break;
			}
			if ((mode & 5) == 4) {
				fanout = pi;
			}
		}
	}

	bool routing_processor_added = false;

	{
		Glib::Threads::Mutex::Lock lx (AudioEngine::instance()->process_lock ());
		Glib::Threads::RWLock::WriterLock lm (_processor_lock);
		ProcessorState pstate (this);

		for (ProcessorList::const_iterator i = others.begin(); i != others.end(); ++i) {

			std::shared_ptr<TriggerBox> tb = std::dynamic_pointer_cast<TriggerBox> (*i);
			if (tb) {
				_triggerbox = tb;
			}

			if (*i == _meter) {
				continue;
			}
			ProcessorList::iterator check = find (to_skip.begin(), to_skip.end(), *i);
			if (check != to_skip.end()) {
				continue;
			}

			std::shared_ptr<PluginInsert> pi;

			if ((pi = std::dynamic_pointer_cast<PluginInsert>(*i)) != 0) {
				PluginManager::instance().stats_use_plugin (pi->plugin()->get_info());
				pi->set_strict_io (_strict_io);
			}

			if (*i == _amp) {
				/* Ensure that only one amp is in the list at any time */
				ProcessorList::iterator check = find (_processors.begin(), _processors.end(), *i);
				if (check != _processors.end()) {
					if (before == _amp) {
						/* Already in position; all is well */
						continue;
					} else {
						_processors.erase (check);
					}
				}
			}

			assert (find (_processors.begin(), _processors.end(), *i) == _processors.end ());

			_processors.insert (loc, *i);
			(*i)->set_owner (this);

			{
				if (configure_processors_unlocked (err, &lm)) {
					pstate.restore ();
					configure_processors_unlocked (0, &lm); // it worked before we tried to add it ...
					return -1;
				}
			}

			if (pi && pi->has_sidechain ()) {
				pi->update_sidechain_name ();
				pi->sidechain_input ()->changed.connect_same_thread (*pi, std::bind (&Route::sidechain_change_handler, this, _1, _2));
			}

			if ((*i)->active()) {
				// emit ActiveChanged() and latency_changed() if needed
				(*i)->activate ();
			}

			(*i)->ActiveChanged.connect_same_thread (*this, std::bind (&Session::queue_latency_recompute, &_session));

			std::shared_ptr<Send> send;
			if ((send = std::dynamic_pointer_cast<Send> (*i))) {
				send->SelfDestruct.connect_same_thread (**i,
						std::bind (&Route::processor_selfdestruct, this, std::weak_ptr<Processor> (*i)));
				if (send->output()) {
					send->output()->changed.connect_same_thread (**i, std::bind (&Route::output_change_handler, this, _1, _2));
				}
			}

			if (std::dynamic_pointer_cast<InternalSend>(*i)) {
				routing_processor_added = true;
			}
		}

		for (ProcessorList::const_iterator i = _processors.begin(); i != _processors.end(); ++i) {
			std::shared_ptr<PluginInsert> pi;

			if ((pi = std::dynamic_pointer_cast<PluginInsert>(*i)) != 0) {
				if (pi->has_no_inputs ()) {
					_have_internal_generator = true;
					break;
				}
			}
		}
	}

	reset_instrument_info ();
	processors_changed (RouteProcessorChange (routing_processor_added ? RouteProcessorChange::SendReturnChange : RouteProcessorChange::GeneralChange, false)); /* EMIT SIGNAL */
	set_processor_positions ();

	if (fanout && fanout->configured ()
			&& fanout->output_streams().n_audio() > 2
			&& std::dynamic_pointer_cast<PluginInsert> (the_instrument ()) == fanout) {
		/* This adds new tracks or busses, and changes connections.
		 * This cannot be done here, and needs to be delegated to the GUI thread. */
		_instrument_fanned_out = true;
		FanOut (std::dynamic_pointer_cast<ARDOUR::Route>(shared_from_this())); /* EMIT SIGNAL */
	}
	return 0;
}

void
Route::placement_range(Placement p, ProcessorList::iterator& start, ProcessorList::iterator& end)
{
	if (p == PreFader) {
		start = _processors.begin();
		end = find(_processors.begin(), _processors.end(), _amp);
	} else {
		start = find(_processors.begin(), _processors.end(), _amp);
		++start;
		end = _processors.end();
	}
}

/** Turn off all processors with a given placement
 * @param p Placement of processors to disable
 */
void
Route::disable_processors (Placement p)
{
	Glib::Threads::RWLock::ReaderLock lm (_processor_lock);

	ProcessorList::iterator start, end;
	placement_range(p, start, end);

	for (ProcessorList::iterator i = start; i != end; ++i) {
		(*i)->enable (false);
	}

	_session.set_dirty ();
}

/** Turn off all redirects
 */
void
Route::disable_processors ()
{
	Glib::Threads::RWLock::ReaderLock lm (_processor_lock);

	for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
		(*i)->enable (false);
	}

	_session.set_dirty ();
}

/** Turn off all redirects with a given placement
 * @param p Placement of redirects to disable
 */
void
Route::disable_plugins (Placement p)
{
	Glib::Threads::RWLock::ReaderLock lm (_processor_lock);

	ProcessorList::iterator start, end;
	placement_range(p, start, end);

	for (ProcessorList::iterator i = start; i != end; ++i) {
		if (std::dynamic_pointer_cast<PluginInsert> (*i)) {
			(*i)->enable (false);
		}
	}

	_session.set_dirty ();
}

/** Turn off all plugins
 */
void
Route::disable_plugins ()
{
	Glib::Threads::RWLock::ReaderLock lm (_processor_lock);

	for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
		if (std::dynamic_pointer_cast<PluginInsert> (*i)) {
			(*i)->enable (false);
		}
	}

	_session.set_dirty ();
}


void
Route::ab_plugins (bool forward)
{
	Glib::Threads::RWLock::ReaderLock lm (_processor_lock);

	if (forward) {

		/* forward = turn off all active redirects, and mark them so that the next time
		   we go the other way, we will revert them
		*/

		for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
			if (!std::dynamic_pointer_cast<PluginInsert> (*i)) {
				continue;
			}
			if (!(*i)->display_to_user () || is_internal_processor (*i)) {
				continue;
			}
			if (std::dynamic_pointer_cast<PluginInsert> (*i)->is_channelstrip()) {

				continue;
			}

			if ((*i)->enabled ()) {
				(*i)->enable (false);
				(*i)->set_next_ab_is_active (true);
			} else {
				(*i)->set_next_ab_is_active (false);
			}
		}

	} else {

		/* backward = if the redirect was marked to go active on the next ab, do so */

		for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
			if (!std::dynamic_pointer_cast<PluginInsert> (*i)) {
				continue;
			}
			if (!(*i)->display_to_user () || is_internal_processor (*i)) {
				continue;
			}
			if (std::dynamic_pointer_cast<PluginInsert> (*i)->is_channelstrip()) {
				continue;
			}

			(*i)->enable ((*i)->get_next_ab_is_active ());
		}
	}

	_session.set_dirty ();
}


/** Remove processors with a given placement.
 * @param p Placement of processors to remove.
 */
void
Route::clear_processors (Placement p)
{
	if (!_session.engine().running()) {
		return;
	}

	bool already_deleting = _session.deletion_in_progress();
	if (!already_deleting) {
		_session.set_deletion_in_progress();
	}

	ProcessorList old_list = _processors;
	{
		Glib::Threads::Mutex::Lock lx (AudioEngine::instance()->process_lock ());
		Glib::Threads::RWLock::WriterLock lm (_processor_lock);
		ProcessorList new_list;
		ProcessorStreams err;
		bool seen_amp = false;

		for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {

			if (*i == _amp) {
				seen_amp = true;
			}

			if (is_internal_processor (*i)) {

				/* you can't remove these */

				new_list.push_back (*i);

			} else {
				if (seen_amp) {

					switch (p) {
					case PreFader:
						new_list.push_back (*i);
						break;
					case PostFader:
						(*i)->drop_references ();
						break;
					}

				} else {

					switch (p) {
					case PreFader:
						(*i)->drop_references ();
						break;
					case PostFader:
						new_list.push_back (*i);
						break;
					}
				}
			}
		}

		_processors = new_list;
		configure_processors_unlocked (&err, &lm); // this can't fail
	}
	/* drop references w/o process-lock (I/O procs may re-take it in ~IO() */
	old_list.clear ();

	processor_max_streams.reset();
	_have_internal_generator = false;
	reset_instrument_info ();
	set_processor_positions ();

	processors_changed (RouteProcessorChange ()); /* EMIT SIGNAL */

	if (!already_deleting) {
		_session.clear_deletion_in_progress();
	}
}

bool
Route::is_internal_processor (std::shared_ptr<Processor> p) const
{
	if (p == _amp || p == _meter || p == _main_outs || p == _delayline || p == _trim || p == _polarity || (_volume && p == _volume) || (_triggerbox && p == _triggerbox) || (_surround_return && p == _surround_return) || (_surround_send && p == _surround_send)) {
		return true;
	}
	return false;
}

int
Route::remove_processor (std::shared_ptr<Processor> processor, ProcessorStreams* err, bool need_process_lock)
{
	// TODO once the export point can be configured properly, do something smarter here
	if (processor == _capturing_processor) {
		Glib::Threads::Mutex::Lock lx (AudioEngine::instance()->process_lock (), Glib::Threads::NOT_LOCK);
		if (need_process_lock) {
			lx.acquire();
		}

		_capturing_processor.reset();

		if (need_process_lock) {
			lx.release();
		}
	}

	/* these can never be removed */

	if (is_internal_processor (processor)) {
		return 0;
	}

	if (!_session.engine().running()) {
		return 1;
	}

	bool routing_processor_removed = false;

	processor_max_streams.reset();

	{
		Glib::Threads::Mutex::Lock lx (AudioEngine::instance()->process_lock (), Glib::Threads::NOT_LOCK);
		if (need_process_lock) {
			lx.acquire();
		}

		/* Caller must hold process lock */
		assert (!AudioEngine::instance()->process_lock().trylock());

		Glib::Threads::RWLock::WriterLock lm (_processor_lock); // XXX deadlock after export

		ProcessorState pstate (this);

		ProcessorList::iterator i = find (_processors.begin(), _processors.end(), processor);
		if (i == _processors.end ()) {
			/* what? */
			return 1;
		}

		/* stop redirects that send signals to JACK ports
		 * from causing noise as a result of no longer being run
		 */
		std::shared_ptr<IOProcessor> iop = std::dynamic_pointer_cast<IOProcessor> (*i);
		std::shared_ptr<PluginInsert> pi = std::dynamic_pointer_cast<PluginInsert>(*i);

		if (pi != 0) {
			assert (iop == 0);
			iop = pi->sidechain();
		}

		if (iop != 0) {
			/* This calls Route::sidechain_change_handler -> Route::input_change_handler()
			 * When the route is implicitly soloed this further calls
			 * Route::direct_feeds_according_to_reality() which takes a
			 * ReaderLock (_processor_lock). So we need to release the lock first.
			 */
			lm.release ();
			iop->disconnect ();
			lm.acquire ();
		}

		if (std::dynamic_pointer_cast<InternalSend>(*i)) {
			routing_processor_removed = true;
		}

		_processors.erase (i);

		if (configure_processors_unlocked (err, &lm)) {
			pstate.restore ();
			/* we know this will work, because it worked before :) */
			configure_processors_unlocked (0, &lm);
			return -1;
		}

		_have_internal_generator = false;

		for (i = _processors.begin(); i != _processors.end(); ++i) {
			std::shared_ptr<PluginInsert> pi;

			if ((pi = std::dynamic_pointer_cast<PluginInsert>(*i)) != 0) {
				if (pi->has_no_inputs ()) {
					_have_internal_generator = true;
					break;
				}
			}
		}
		if (need_process_lock) {
			lx.release();
		}
	}

	reset_instrument_info ();
	processor->drop_references ();
	processors_changed (RouteProcessorChange (routing_processor_removed ? RouteProcessorChange::SendReturnChange : RouteProcessorChange::GeneralChange, false)); /* EMIT SIGNAL */
	set_processor_positions ();

	return 0;
}

int
Route::replace_processor (std::shared_ptr<Processor> old, std::shared_ptr<Processor> sub, ProcessorStreams* err)
{
	/* these can never be removed */
	if (is_internal_processor (old)) {
		return 1;
	}
	/* and can't be used as substitute, either */
	if (is_internal_processor (sub)) {
		return 1;
	}

	/* I/Os are out, too */
	if (std::dynamic_pointer_cast<IOProcessor> (old) || std::dynamic_pointer_cast<IOProcessor> (sub)) {
		return 1;
	}

	/* this function cannot be used to swap/reorder processors */
	if (find (_processors.begin(), _processors.end(), sub) != _processors.end ()) {
		return 1;
	}

	if (!AudioEngine::instance()->running() || !old || !sub) {
		return 1;
	}

	/* ensure that sub is not owned by another route */
	if (sub->owner ()) {
		return 1;
	}

	{
		Glib::Threads::Mutex::Lock lx (AudioEngine::instance()->process_lock ());
		Glib::Threads::RWLock::WriterLock lm (_processor_lock);
		ProcessorState pstate (this);

		assert (find (_processors.begin(), _processors.end(), sub) == _processors.end ());

		ProcessorList::iterator i;
		bool replaced = false;
		bool enable = old->enabled ();

		for (i = _processors.begin(); i != _processors.end(); ) {
			if (*i == old) {
				i = _processors.erase (i);
				_processors.insert (i, sub);
				sub->set_owner (this);
				replaced = true;
				break;
			} else {
				++i;
			}
		}

		if (!replaced) {
			return 1;
		}

		if (_strict_io) {
			std::shared_ptr<PluginInsert> pi;
			if ((pi = std::dynamic_pointer_cast<PluginInsert>(sub)) != 0) {
				pi->set_strict_io (true);
			}
		}

		if (configure_processors_unlocked (err, &lm)) {
			pstate.restore ();
			configure_processors_unlocked (0, &lm);
			return -1;
		}

		_have_internal_generator = false;

		for (i = _processors.begin(); i != _processors.end(); ++i) {
			std::shared_ptr<PluginInsert> pi;
			if ((pi = std::dynamic_pointer_cast<PluginInsert>(*i)) != 0) {
				if (pi->has_no_inputs ()) {
					_have_internal_generator = true;
					break;
				}
			}
		}

		if (enable) {
			sub->enable (true);
		}

		sub->ActiveChanged.connect_same_thread (*sub, std::bind (&Session::queue_latency_recompute, &_session));
	}

	reset_instrument_info ();
	old->drop_references ();
	processors_changed (RouteProcessorChange ()); /* EMIT SIGNAL */
	set_processor_positions ();
	return 0;
}

int
Route::remove_processors (const ProcessorList& to_be_deleted, ProcessorStreams* err)
{
	ProcessorList deleted;

	if (!_session.engine().running()) {
		return 1;
	}

	bool routing_processor_removed = false;

	processor_max_streams.reset();

	{
		Glib::Threads::Mutex::Lock lx (AudioEngine::instance()->process_lock ());
		Glib::Threads::RWLock::WriterLock lm (_processor_lock);
		ProcessorState pstate (this);

		ProcessorList::iterator i;
		std::shared_ptr<Processor> processor;

		for (i = _processors.begin(); i != _processors.end(); ) {

			processor = *i;

			/* these can never be removed */

			if (is_internal_processor (processor)) {
				++i;
				continue;
			}

			/* see if its in the list of processors to delete */

			if (find (to_be_deleted.begin(), to_be_deleted.end(), processor) == to_be_deleted.end()) {
				++i;
				continue;
			}

			/* stop IOProcessors that send to JACK ports
			   from causing noise as a result of no longer being
			   run.
			*/

			std::shared_ptr<IOProcessor> iop = std::dynamic_pointer_cast<IOProcessor>(processor);
			std::shared_ptr<PluginInsert> pi = std::dynamic_pointer_cast<PluginInsert>(processor);
			if (pi != 0) {
				assert (iop == 0);
				iop = pi->sidechain();
			}

			if (iop != 0) {
				iop->disconnect ();
			}

			if (std::dynamic_pointer_cast<InternalSend>(processor)) {
				routing_processor_removed = true;
			}

			deleted.push_back (processor);
			i = _processors.erase (i);
		}

		if (deleted.empty()) {
			/* none of those in the requested list were found */
			return 0;
		}

		if (configure_processors_unlocked (err, &lm)) {
			pstate.restore ();
			/* we know this will work, because it worked before :) */
			configure_processors_unlocked (0, &lm);
			return -1;
		}
		//lx.unlock();

		_have_internal_generator = false;

		for (i = _processors.begin(); i != _processors.end(); ++i) {
			std::shared_ptr<PluginInsert> pi;

			if ((pi = std::dynamic_pointer_cast<PluginInsert>(*i)) != 0) {
				if (pi->has_no_inputs ()) {
					_have_internal_generator = true;
					break;
				}
			}
		}
	}

	/* now try to do what we need to so that those that were removed will be deleted */

	for (ProcessorList::iterator i = deleted.begin(); i != deleted.end(); ++i) {
		(*i)->drop_references ();
	}

	reset_instrument_info ();
	processors_changed (RouteProcessorChange (routing_processor_removed ? RouteProcessorChange::SendReturnChange : RouteProcessorChange::GeneralChange, false)); /* EMIT SIGNAL */
	set_processor_positions ();

	return 0;
}

void
Route::stop_triggers (bool now)
{
	if (_triggerbox) {
		if (now) {
			_triggerbox->stop_all_immediately ();
		} else {
			_triggerbox->stop_all_quantized();
		}
	}
}

void
Route::reset_instrument_info ()
{
	std::shared_ptr<Processor> instr = the_instrument();
	if (!instr) {
		_instrument_fanned_out = false;
	}
	_instrument_info.set_internal_instrument (instr);
}

/** Caller must hold process lock */
int
Route::configure_processors (ProcessorStreams* err)
{
#ifndef PLATFORM_WINDOWS
	assert (!AudioEngine::instance()->process_lock().trylock());
#endif

	if (!_in_configure_processors) {
		Glib::Threads::RWLock::WriterLock lm (_processor_lock);
		return configure_processors_unlocked (err, &lm);
	}

	return 0;
}

ChanCount
Route::input_streams () const
{
	return _input->n_ports ();
}

list<pair<ChanCount, ChanCount> >
Route::try_configure_processors (ChanCount in, ProcessorStreams* err)
{
	Glib::Threads::RWLock::ReaderLock lm (_processor_lock);

	return try_configure_processors_unlocked (in, err);
}

list<pair<ChanCount, ChanCount> >
Route::try_configure_processors_unlocked (ChanCount in, ProcessorStreams* err)
{
	// Check each processor in order to see if we can configure as requested
	ChanCount out;
	list<pair<ChanCount, ChanCount> > configuration;
	uint32_t index = 0;

	DEBUG_TRACE (DEBUG::Processors, string_compose ("%1: configure processors\n", _name));
	DEBUG_TRACE (DEBUG::Processors, "{\n");

	ChanCount disk_io = in;

	for (ProcessorList::iterator p = _processors.begin(); p != _processors.end(); ++p, ++index) {

		if (std::dynamic_pointer_cast<DiskReader> (*p)) {
			/* disk-reader has the same i/o as disk-writer */
			in = max (in, disk_io);
		}

		if ((*p)->can_support_io_configuration(in, out)) {

			if (std::dynamic_pointer_cast<Delivery> (*p)
					&& std::dynamic_pointer_cast<Delivery> (*p)->role() == Delivery::Main
					&& !is_auditioner()
					&& (is_monitor() || _strict_io || Profile->get_mixbus ())) {
				/* with strict I/O the panner + output are forced to
				 * follow the last processor's output.
				 *
				 * Delivery::can_support_io_configuration() will only add ports,
				 * but not remove excess ports.
				 *
				 * This works because the delivery only requires
				 * as many outputs as there are inputs.
				 * Delivery::configure_io() will do the actual removal
				 * by calling _output->ensure_io()
				 */
				if (!is_master() && _session.master_out () && in.n_audio() > 0) {
					if (!is_monitor()) {
						/* ..but at least as many as there are master-inputs, if
						 * the delivery is dealing with audio */
						out = ChanCount::max (in, _session.master_out ()->n_inputs ());
					} else {
						/* monitor-bus follows the master-bus' output */
						out = ChanCount::max (in, _session.master_out ()->n_outputs ());
					}
				} else {
					out = in;
				}
			}

			DEBUG_TRACE (DEBUG::Processors, string_compose ("\t%1 ID=%2 in=%3 out=%4\n",(*p)->display_name(), (*p)->id(), in, out));
			configuration.push_back(make_pair(in, out));

			if (is_monitor()) {
				// restriction for Monitor Section Processors
				if (in.n_audio() != out.n_audio() || out.n_midi() > 0) {
					/* Note: The Monitor follows the master-bus and has no panner.
					 *
					 * The general idea is to only allow plugins that retain the channel-count
					 * and plugins with MIDI in (e.g VSTs with control that will remain unconnected).
					 * Then again 5.1 in, monitor stereo is a valid use-case.
					 *
					 * and worse: we only refuse adding plugins *here*.
					 *
					 * 1) stereo-master, stereo-mon, add a stereo-plugin, OK
					 * 2) change master-bus, add a channel
					 * 2a) monitor-secion follows
					 * 3) monitor processors fail to re-reconfigure (stereo plugin)
					 * 4) re-load session, monitor-processor remains unconfigured, crash.
					 */
					DEBUG_TRACE (DEBUG::Processors, "Monitor: Channel configuration change.\n");
				}
				if (std::dynamic_pointer_cast<InternalSend> (*p)) {
					// internal sends make no sense, only feedback
					DEBUG_TRACE (DEBUG::Processors, "Monitor: No Sends allowed.\n");
					return list<pair<ChanCount, ChanCount> > ();
				}
				if (std::dynamic_pointer_cast<PortInsert> (*p)) {
					/* External Sends can be problematic. one can add/remove ports
					 * there signal leaves the DAW to external monitors anyway, so there's
					 * no real use for allowing them here anyway.
					 */
					DEBUG_TRACE (DEBUG::Processors, "Monitor: No External Sends allowed.\n");
					return list<pair<ChanCount, ChanCount> > ();
				}
				if (std::dynamic_pointer_cast<Send> (*p)) {
					// ditto
					DEBUG_TRACE (DEBUG::Processors, "Monitor: No Sends allowed.\n");
					return list<pair<ChanCount, ChanCount> > ();
				}
			}

			if (std::dynamic_pointer_cast<DiskWriter> (*p)) {
				assert (in == out);
				disk_io = out;
			}


			/* next processor's in == this processor's out*/
			in = out;
		} else {
			if (err) {
				err->index = index;
				err->count = in;
			}
			DEBUG_TRACE (DEBUG::Processors, "---- CONFIGURATION FAILED.\n");
			DEBUG_TRACE (DEBUG::Processors, string_compose ("---- %1 cannot support in=%2 out=%3\n", (*p)->name(), in, out));
			DEBUG_TRACE (DEBUG::Processors, "}\n");
			return list<pair<ChanCount, ChanCount> > ();
		}
	}

	DEBUG_TRACE (DEBUG::Processors, "}\n");

	return configuration;
}

/** Set the input/output configuration of each processor in the processors list.
 *  Caller must hold process lock.
 *  Return 0 on success, otherwise configuration is impossible.
 */
int
Route::configure_processors_unlocked (ProcessorStreams* err, Glib::Threads::RWLock::WriterLock* lm)
{
#ifndef PLATFORM_WINDOWS
	assert (!AudioEngine::instance()->process_lock().trylock());
#endif

	if (_in_configure_processors) {
		return 0;
	}

	/* put invisible processors where they should be */
	setup_invisible_processors ();

	_in_configure_processors = true;

	list<pair<ChanCount, ChanCount> > configuration = try_configure_processors_unlocked (input_streams (), err);

	if (configuration.empty ()) {
		_in_configure_processors = false;
		return -1;
	}

	ChanCount out;
	bool seen_mains_out = false;
	processor_out_streams = _input->n_ports();
	processor_max_streams.reset();

	/* processor configure_io() may result in adding ports
	 * e.g. Delivery::configure_io -> ARDOUR::IO::ensure_io ()
	 *
	 * with jack2 adding ports results in a graph-order callback,
	 * which calls Session::resort_routes() and eventually
	 * Route::direct_feeds_according_to_reality()
	 * which takes a ReaderLock (_processor_lock).
	 *
	 * so we can't hold a WriterLock here until jack2 threading
	 * is fixed.
	 *
	 * NB. we still hold the process lock
	 *
	 * (ardour's own engines do call graph-order from the
	 * process-thread and hence do not have this issue; besides
	 * merely adding ports won't trigger a graph-order, only
	 * making connections does)
	 */
	lm->release ();

	// TODO check for a potential ReaderLock after ReaderLock ??
	Glib::Threads::RWLock::ReaderLock lr (_processor_lock);

	list< pair<ChanCount,ChanCount> >::iterator c = configuration.begin();
	for (ProcessorList::iterator p = _processors.begin(); p != _processors.end(); ++p, ++c) {

		if (!(*p)->configure_io(c->first, c->second)) {
			DEBUG_TRACE (DEBUG::Processors, string_compose ("%1: configuration failed\n", _name));
			_in_configure_processors = false;
			lr.release ();
			lm->acquire ();
			return -1;
		}

		processor_max_streams = ChanCount::max(processor_max_streams, c->first);
		processor_max_streams = ChanCount::max(processor_max_streams, c->second);

		std::shared_ptr<IOProcessor> iop;
		std::shared_ptr<PluginInsert> pi;
		if ((pi = std::dynamic_pointer_cast<PluginInsert>(*p)) != 0) {
			/* plugins connected via Split or Hide Match may have more channels.
			 * route/scratch buffers are needed for all of them
			 * The configuration may only be a subset (both input and output)
			 */
			processor_max_streams = ChanCount::max(processor_max_streams, pi->required_buffers());
		}
		else if ((iop = std::dynamic_pointer_cast<IOProcessor>(*p)) != 0) {
			processor_max_streams = ChanCount::max(processor_max_streams, iop->natural_input_streams());
			processor_max_streams = ChanCount::max(processor_max_streams, iop->natural_output_streams());
		}
		else if (std::dynamic_pointer_cast<TriggerBox>(*p) != 0) {
			/* TB sidechain control input */
			processor_max_streams = ChanCount::max(processor_max_streams, c->first + ChanCount (DataType::MIDI, 1));
		}

		out = c->second;

		if (std::dynamic_pointer_cast<Delivery> (*p)
				&& std::dynamic_pointer_cast<Delivery> (*p)->role() == Delivery::Main) {
			/* main delivery will increase port count to match input.
			 * the Delivery::Main is usually the last processor - followed only by
			 * 'MeterOutput'.
			 */
			seen_mains_out = true;
		}
		if (!seen_mains_out) {
			processor_out_streams = out;
		}
	}

	lr.release ();
	lm->acquire ();

	if (_meter) {
		_meter->set_max_channels (processor_max_streams);
	}

	/* make sure we have sufficient scratch buffers to cope with the new processor
	   configuration
	*/
	_session.ensure_buffers (n_process_buffers ());

	DEBUG_TRACE (DEBUG::Processors, string_compose ("%1: configuration complete\n", _name));

	_in_configure_processors = false;
	return 0;
}

/** Set all visible processors to a given active state (except Fader, whose state is not changed)
 *  @param state New active state for those processors.
 */
void
Route::all_visible_processors_active (bool state)
{
	Glib::Threads::RWLock::ReaderLock lm (_processor_lock);

	if (_processors.empty()) {
		return;
	}

	for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
		if (!(*i)->display_to_user () || is_internal_processor (*i)) {
			continue;
		}

		std::shared_ptr<PluginInsert> pi;
		if (0 != (pi = std::dynamic_pointer_cast<PluginInsert>(*i))) {
			if (pi->is_channelstrip ()) {
				continue;
			}
		}
		(*i)->enable (state);
	}

	_session.set_dirty ();
}

bool
Route::processors_reorder_needs_configure (const ProcessorList& new_order)
{
	/* check if re-order requires re-configuration of any processors
	 * -> compare channel configuration for all processors
	 */
	Glib::Threads::RWLock::ReaderLock lm (_processor_lock);
	ChanCount c = input_streams ();

	for (ProcessorList::const_iterator j = new_order.begin(); j != new_order.end(); ++j) {
		bool found = false;
		if (c != (*j)->input_streams()) {
			return true;
		}
		for (ProcessorList::const_iterator i = _processors.begin(); i != _processors.end(); ++i) {
			if (*i == *j) {
				found = true;
				if ((*i)->input_streams() != c) {
					return true;
				}
				c = (*i)->output_streams();
				break;
			}
		}
		if (!found) {
			return true;
		}
	}
	return false;
}

#ifdef __clang__
__attribute__((annotate("realtime")))
#endif
void
Route::apply_processor_order (const ProcessorList& new_order)
{
	/* need to hold processor_lock; either read or write lock
	 * and the engine process_lock.
	 * Due to r/w lock ambiguity we can only assert the latter
	 */
	assert (!AudioEngine::instance()->process_lock().trylock());


	/* "new_order" is an ordered list of processors to be positioned according to "placement".
	 * NOTE: all processors in "new_order" MUST be marked as display_to_user(). There maybe additional
	 * processors in the current actual processor list that are hidden. Any visible processors
	 * in the current list but not in "new_order" will be assumed to be deleted.
	 */

	/* "as_it_will_be" and "_processors" are lists of shared pointers.
	 * actual memory usage is small, but insert/erase is not actually rt-safe :(
	 * (note though that  ::processors_reorder_needs_configure() ensured that
	 * this function will only ever be called from the rt-thread if no processor were removed)
	 *
	 * either way, I can't proove it, but an xrun due to re-order here is less likely
	 * than an xrun-less 'ardour-silent cycle' both of which effectively "click".
	 */

	ProcessorList as_it_will_be;
	ProcessorList::iterator oiter;
	ProcessorList::const_iterator niter;

	oiter = _processors.begin();
	niter = new_order.begin();

	while (niter != new_order.end ()) {

		/* if the next processor in the old list is invisible (i.e. should not be in the new order)
		 * then append it to the temp list.
		 *
		 * Otherwise, see if the next processor in the old list is in the new list. if not,
		 * its been deleted. If its there, append it to the temp list.
		 */

		if (oiter == _processors.end()) {

			/* no more elements in the old list, so just stick the rest of
			 * the new order onto the temp list.
			 */

			as_it_will_be.insert (as_it_will_be.end(), niter, new_order.end());
			while (niter != new_order.end()) {
				++niter;
			}
			break;

		} else {

			if (!(*oiter)->display_to_user()) {

				as_it_will_be.push_back (*oiter);

			} else {

				/* visible processor: check that its in the new order */

				if (find (new_order.begin(), new_order.end(), (*oiter)) == new_order.end()) {
					/* deleted: do nothing, shared_ptr<> will clean up */
				} else {
					/* ignore this one, and add the next item from the new order instead */
					as_it_will_be.push_back (*niter);
					++niter;
				}
			}

			/* now remove from old order - its taken care of no matter what */
			oiter = _processors.erase (oiter);
		}

	}
	_processors.insert (oiter, as_it_will_be.begin(), as_it_will_be.end());

	/* If the meter is in a custom position, find it and make a rough note of its position */
	maybe_note_meter_position ();

	/* if any latent plugins were re-ordered and sends or side-chains are present
	 * in the signal-flow, a full latency-recompute is needed.
	 *
	 * The Session will be informed about the new order via
	 *  processors_changed()
	 * and test if a full latency-recompute is required by comparing
	 * _signal_latency != ::update_signal_latency();
	 *
	 * Since the route's latency itself does not initially change by
	 * re-ordering, we need to force this:
	 */
	bool need_latency_recompute = false;
	for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
		if (std::dynamic_pointer_cast<PortInsert> (*i)) {
			need_latency_recompute = true;
			break;
		} else if (std::dynamic_pointer_cast<LatentSend> (*i)) {
			need_latency_recompute = true;
			break;
		} else if (std::shared_ptr<PluginInsert> pi = std::dynamic_pointer_cast<PluginInsert> (*i)) {
			if (pi->sidechain_input ()) {
				need_latency_recompute = true;
				break;
			}
		}
	}
	if (need_latency_recompute) {
		/* force a change, the correct value will be set
		 * ::update_signal_latency() will be called via
		 *
		 * SIGNAL processors_changed () ->
		 * -> Session::route_processors_changed ()
		 * -> Session::update_latency_compensation ()
		 * -> Route::::update_signal_latency ()
		 */
	_signal_latency = 0;
	}
}

void
Route::move_instrument_down (bool postfader)
{
	Glib::Threads::RWLock::ReaderLock lm (_processor_lock);
	ProcessorList new_order;
	std::shared_ptr<Processor> instrument;
	for (ProcessorList::const_iterator i = _processors.begin(); i != _processors.end(); ++i) {
		std::shared_ptr<PluginInsert> pi = std::dynamic_pointer_cast<PluginInsert>(*i);
		if (pi && pi->plugin ()->get_info ()->is_instrument ()) {
			instrument = *i;
		} else if (instrument && *i == _amp) {
			if (postfader) {
				new_order.push_back (*i);
				new_order.push_back (instrument);
			} else {
				new_order.push_back (instrument);
				new_order.push_back (*i);
			}
		} else {
			new_order.push_back (*i);
		}
	}
	if (!instrument) {
		return;
	}
	lm.release ();
	reorder_processors (new_order, 0);
}

int
Route::reorder_processors (const ProcessorList& new_order, ProcessorStreams* err)
{
	/* If a change is already queued, wait for it
	 * (unless engine is stopped. apply immediately and proceed
	 */
	while (_pending_process_reorder.load ()) {
		if (!AudioEngine::instance()->running()) {
			DEBUG_TRACE (DEBUG::Processors, "offline apply queued processor re-order.\n");
			Glib::Threads::RWLock::WriterLock lm (_processor_lock);

			_pending_process_reorder.store (0);
			_pending_listen_change.store (0);

			apply_processor_order(_pending_processor_order);
			_pending_processor_order.clear ();
			setup_invisible_processors ();

			update_signal_latency (true);

			processors_changed (RouteProcessorChange ()); /* EMIT SIGNAL */
			set_processor_positions ();
		} else {
			// TODO rather use a semaphore or something.
			// but since ::reorder_processors() is called
			// from the GUI thread, this is fine..
			Glib::usleep(500);
		}
	}

	if (processors_reorder_needs_configure (new_order) || !AudioEngine::instance()->running()) {

		Glib::Threads::Mutex::Lock lx (AudioEngine::instance()->process_lock ());
		Glib::Threads::RWLock::WriterLock lm (_processor_lock);
		ProcessorState pstate (this);

		apply_processor_order (new_order);

		if (configure_processors_unlocked (err, &lm)) {
			pstate.restore ();
			return -1;
		}

		lm.release();

		/* update processor input/output latency (total signal_latency does not change).
		 * delaylines may changes, so the Engine Lock is required.
		 */
		update_signal_latency (true);

		lx.release();

		processors_changed (RouteProcessorChange ()); /* EMIT SIGNAL */
		set_processor_positions ();

	} else {
		DEBUG_TRACE (DEBUG::Processors, "Queue clickless processor re-order.\n");
		Glib::Threads::RWLock::ReaderLock lm (_processor_lock);

		// _pending_processor_order is protected by _processor_lock
		_pending_processor_order = new_order;
		_pending_process_reorder.store (1);
	}

	return 0;
}

bool
Route::add_remove_sidechain (std::shared_ptr<Processor> proc, bool add)
{
	if (_session.actively_recording () || _in_sidechain_setup) {
		return false;
	}

	std::shared_ptr<PluginInsert> pi;
	if ((pi = std::dynamic_pointer_cast<PluginInsert>(proc)) == 0) {
		return false;
	}

	if (pi->has_sidechain () == add) {
		return true; // ?? call failed, but result is as expected.
	}

	{
		Glib::Threads::RWLock::ReaderLock lm (_processor_lock);
		ProcessorList::iterator i = find (_processors.begin(), _processors.end(), proc);
		if (i == _processors.end ()) {
			return false;
		}
	}

	{
		PBD::Unwinder<bool> uw (_in_sidechain_setup, true);

		if (add) {
			ChanCount sc (pi->sidechain_input_pins ());
			if (sc.n_audio () == 0 && sc.n_midi () == 0) {
				sc.set (DataType::AUDIO, 1);
			}
			if (!pi->add_sidechain (sc.n_audio (), sc.n_midi ())) {
				return false;
			}
		} else {
			if (!pi->del_sidechain ()) {
				return false;
			}
		}

		Glib::Threads::Mutex::Lock lx (AudioEngine::instance()->process_lock ()); // take before Writerlock to avoid deadlock
		Glib::Threads::RWLock::WriterLock lm (_processor_lock);

		list<pair<ChanCount, ChanCount> > c = try_configure_processors_unlocked (n_inputs (), 0);

		if (c.empty()) {
			lm.release ();
			lx.release ();

			if (add) {
				pi->del_sidechain ();
			} else {
				pi->add_sidechain ();
				// TODO restore side-chain's state.
			}
			return false;
		}

		configure_processors_unlocked (0, &lm);
	}

	if (pi->has_sidechain ()) {
		pi->reset_sidechain_map ();
		pi->sidechain_input ()->changed.connect_same_thread (*pi, std::bind (&Route::sidechain_change_handler, this, _1, _2));
	}

	processors_changed (RouteProcessorChange ()); /* EMIT SIGNAL */
	_session.set_dirty ();
	return true;
}

bool
Route::plugin_preset_output (std::shared_ptr<Processor> proc, ChanCount outs)
{
	if (_session.actively_recording ()) {
		return false;
	}

	std::shared_ptr<PluginInsert> pi;
	if ((pi = std::dynamic_pointer_cast<PluginInsert>(proc)) == 0) {
		return false;
	}

	{
		Glib::Threads::RWLock::ReaderLock lm (_processor_lock);
		ProcessorList::iterator i = find (_processors.begin(), _processors.end(), proc);
		if (i == _processors.end ()) {
			return false;
		}
	}

	{
		Glib::Threads::Mutex::Lock lx (AudioEngine::instance()->process_lock ());
		Glib::Threads::RWLock::WriterLock lm (_processor_lock);

		const ChanCount& old (pi->preset_out ());
		if (!pi->set_preset_out (outs)) {
			return true; // no change, OK
		}

		list<pair<ChanCount, ChanCount> > c = try_configure_processors_unlocked (n_inputs (), 0);
		if (c.empty()) {
			/* not possible */
			pi->set_preset_out (old);
			return false;
		}
		configure_processors_unlocked (0, &lm);
	}

	processors_changed (RouteProcessorChange ()); /* EMIT SIGNAL */
	_session.set_dirty ();
	return true;
}

bool
Route::reset_plugin_insert (std::shared_ptr<Processor> proc)
{
	ChanCount unused;
	return customize_plugin_insert (proc, 0, unused, unused);
}

bool
Route::customize_plugin_insert (std::shared_ptr<Processor> proc, uint32_t count, ChanCount outs, ChanCount sinks)
{
	if (_session.actively_recording ()) {
		return false;
	}
	std::shared_ptr<PluginInsert> pi;
	if ((pi = std::dynamic_pointer_cast<PluginInsert>(proc)) == 0) {
		return false;
	}

	{
		Glib::Threads::RWLock::ReaderLock lm (_processor_lock);
		ProcessorList::iterator i = find (_processors.begin(), _processors.end(), proc);
		if (i == _processors.end ()) {
			return false;
		}
	}

	{
		Glib::Threads::Mutex::Lock lx (AudioEngine::instance()->process_lock ());
		Glib::Threads::RWLock::WriterLock lm (_processor_lock);

		bool      old_cust  = pi->custom_cfg ();
		uint32_t  old_cnt   = pi->get_count ();
		ChanCount old_chan  = pi->output_streams ();
		ChanCount old_sinks = pi->natural_input_streams ();

		if (count == 0) {
			pi->set_custom_cfg (false);
		} else {
			pi->set_custom_cfg (true);
			pi->set_count (count);
			pi->set_outputs (outs);
			pi->set_sinks (sinks);
		}

		list<pair<ChanCount, ChanCount> > c = try_configure_processors_unlocked (n_inputs (), 0);
		if (c.empty()) {
			/* not possible */

			pi->set_count (old_cnt);
			pi->set_sinks (old_sinks);
			pi->set_outputs (old_chan);
			pi->set_custom_cfg (old_cust);

			return false;
		}
		configure_processors_unlocked (0, &lm);
	}

	processors_changed (RouteProcessorChange (RouteProcessorChange::CustomPinChange, false)); /* EMIT SIGNAL */
	_session.set_dirty ();
	return true;
}

bool
Route::set_strict_io (const bool enable)
{
	Glib::Threads::Mutex::Lock lx (AudioEngine::instance()->process_lock ());

	if (_strict_io != enable) {
		_strict_io = enable;
		Glib::Threads::RWLock::ReaderLock lm (_processor_lock);
		for (ProcessorList::iterator p = _processors.begin(); p != _processors.end(); ++p) {
			std::shared_ptr<PluginInsert> pi;
			if ((pi = std::dynamic_pointer_cast<PluginInsert>(*p)) != 0) {
				pi->set_strict_io (_strict_io);
			}
		}

		list<pair<ChanCount, ChanCount> > c = try_configure_processors_unlocked (n_inputs (), 0);

		if (c.empty()) {
			// not possible
			_strict_io = !enable; // restore old value
			for (ProcessorList::iterator p = _processors.begin(); p != _processors.end(); ++p) {
				std::shared_ptr<PluginInsert> pi;
				if ((pi = std::dynamic_pointer_cast<PluginInsert>(*p)) != 0) {
					pi->set_strict_io (_strict_io);
				}
			}
			return false;
		}
		lm.release ();

		configure_processors (0);
		lx.release ();

		processors_changed (RouteProcessorChange (RouteProcessorChange::CustomPinChange, false)); /* EMIT SIGNAL */
		_session.set_dirty ();
	}
	return true;
}

XMLNode&
Route::get_state() const
{
	return state (false);
}

XMLNode&
Route::get_template()
{
	return state (true);
}

XMLNode&
Route::state (bool save_template) const
{
	if (!_session._template_state_dir.empty()) {
		Route* ncthis = const_cast<Route*> (this);
		foreach_processor (sigc::bind (sigc::mem_fun (*ncthis, &Route::set_plugin_state_dir), _session._template_state_dir));
	}

	XMLNode *node = new XMLNode("Route");
	ProcessorList::iterator i;

	if(save_template) {
		XMLNode* child = node->add_child("ProgramVersion");
		child->set_property("created-with", _session.created_with);

		std::string modified_with = string_compose ("%1 %2", PROGRAM_NAME, revision);
		child->set_property("modified-with", modified_with);
	}

	/* This is needed for templates and when duplicating routes, in which case
	 * the route-state is directly passed to new_route_from_template().
	 */
	node->set_property("version", CURRENT_SESSION_FILE_VERSION);

	node->set_property (X_("id"), id ());
	node->set_property (X_("name"), name());
	node->set_property (X_("default-type"), _default_type);
	node->set_property (X_("strict-io"), _strict_io);

	if (is_master ()) {
		node->set_property (X_("volume-applies-to-output"), _volume_applies_to_output);
	}

	node->add_child_nocopy (_presentation_info.get_state());

	node->set_property (X_("active"), _active);
	node->set_property (X_("denormal-protection"), _denormal_protection);
	node->set_property (X_("meter-point"), _meter_point);
	node->set_property (X_("disk-io-point"), _disk_io_point);

	node->set_property (X_("meter-type"), _meter->meter_type ());

	if (_route_group) {
		node->set_property (X_("route-group"), _route_group->name());
	}

	node->add_child_nocopy (_solo_control->get_state ());
	node->add_child_nocopy (_solo_isolate_control->get_state ());
	node->add_child_nocopy (_solo_safe_control->get_state ());

	node->add_child_nocopy (_input->get_state ());
	node->add_child_nocopy (_output->get_state ());
	node->add_child_nocopy (_mute_master->get_state ());

	node->add_child_nocopy (_mute_control->get_state ());
	node->add_child_nocopy (_phase_control->get_state ());

	if (_volume_control) {
		node->add_child_nocopy (_volume_control->get_state ());
	}

	if (!skip_saving_automation) {
		node->add_child_nocopy (Automatable::get_automation_xml_state ());
	}

	if (_comment.length()) {
		XMLNode *cmt = node->add_child ("Comment");
		cmt->add_content (_comment);
	}

	if (_pannable) {
		node->add_child_nocopy (_pannable->get_state ());
	}

	{
		Glib::Threads::RWLock::ReaderLock lm (_processor_lock);
		for (auto const & p : _processors) {
			if (p == _delayline) {
				continue;
			}
			if (save_template) {
				/* template save: do not include internal sends functioning as
					 aux sends because the chance of the target ID
					 in the session where this template is used
					 is not very likely.

					 similarly, do not save listen sends which connect to
					 the monitor section, because these will always be
					 added if necessary.
					 */
				std::shared_ptr<InternalSend> is;

				if ((is = std::dynamic_pointer_cast<InternalSend> (p)) != 0) {
					if (is->role() == Delivery::Listen) {
						continue;
					}
				}
			}
			node->add_child_nocopy (p->get_state ());
		}
	}

	if (_extra_xml) {
		node->add_child_copy (*_extra_xml);
	}

	if (_custom_meter_position_noted) {
		std::shared_ptr<Processor> after = _processor_after_last_custom_meter.lock ();
		if (after) {
			node->set_property (X_("processor-after-last-custom-meter"), after->id());
		}
	}

	if (!_session._template_state_dir.empty()) {
		Route* ncthis = const_cast<Route*> (this);
		foreach_processor (sigc::bind (sigc::mem_fun (*ncthis, &Route::set_plugin_state_dir), ""));
	}

	node->add_child_copy (Slavable::get_state());

	return *node;
}

int
Route::set_state (const XMLNode& node, int version)
{
	if (version < 3000) {
		return set_state_2X (node, version);
	}

	XMLNodeList nlist;
	XMLNodeConstIterator niter;
	XMLNode *child;

	if (node.name() != "Route"){
		error << string_compose(_("Bad node sent to Route::set_state() [%1]"), node.name()) << endmsg;
		return -1;
	}

	std::string route_name;
	if (node.get_property (X_("name"), route_name)) {
		set_name (route_name);
	}

	set_id (node);
	_initial_io_setup = true;

	Stripable::set_state (node, version);

	/*  "This should never happen"
	Stripable's job is to save/recall the PresentationInfo flags for bus/track audio/midi VCA etc.
	But I found a case where no "type" flag is set, so the strip never shows up on any UI.
	Since I don't know the source of the error, I have to assume that it could happen again.
	So: if a stripable doesn't have any flags set, populate them from our audio/midi track/bus identity.
	*/
	PresentationInfo::Flag file_flags = _presentation_info.flags();
	if ( !(file_flags & PresentationInfo::TypeMask) ) {
		if (dynamic_cast<AudioTrack*>(this)) {
			_presentation_info.set_flags ( PresentationInfo::Flag (file_flags | PresentationInfo::AudioTrack) );
		} else if (dynamic_cast<MidiTrack*>(this)) {
			_presentation_info.set_flags ( PresentationInfo::Flag (file_flags | PresentationInfo::MidiTrack) );
		} else {
			//no idea what this is, so let's call it an audio bus
			_presentation_info.set_flags ( PresentationInfo::Flag (file_flags | PresentationInfo::AudioBus) );
		}
	}

	node.get_property (X_("strict-io"), _strict_io);

	if (is_monitor()) {
		/* monitor bus does not get a panner, but if (re)created
		   via XML, it will already have one by the time we
		   call ::set_state(). so ... remove it.
		*/
		unpan ();
	}

	/* add all processors (except amp, which is always present) */

	nlist = node.children();
	XMLNode processor_state (X_("processor_state"));

	Stateful::save_extra_xml (node);

	for (niter = nlist.begin(); niter != nlist.end(); ++niter){

		child = *niter;

		if (child->name() == IO::state_node_name) {
			std::string direction;
			if (!child->get_property (X_("direction"), direction)) {
				continue;
			}

			if (direction == "Input") {
				_input->set_state (*child, version);
			} else if (direction == "Output") {
				_output->set_state (*child, version);
			}

		} else if (child->name() == X_("Processor")) {
			processor_state.add_child_copy (*child);
		} else if (child->name() == X_("Pannable")) {
			if (_pannable) {
				_pannable->set_state (*child, version);
			} else {
				warning << string_compose (_("Panner state mismatches for track/bus (%1)."), name()) << endmsg;
			}
		} else if (child->name() == Slavable::xml_node_name) {
			Slavable::set_state (*child, version);
		}
	}

	DiskIOPoint diop;
	if (node.get_property (X_("disk-io-point"), diop)) {
		if (_disk_writer) {
			_disk_writer->set_display_to_user (diop == DiskIOCustom);
		}
		if (_disk_reader) {
			_disk_reader->set_display_to_user (diop == DiskIOCustom);
		}
		if (_triggerbox) {
			_triggerbox->set_display_to_user (diop == DiskIOCustom);
		}
		set_disk_io_point (diop);
	}

	MeterType meter_type;
	if (node.get_property (X_("meter-type"), meter_type)) {
		set_meter_type (meter_type);
	}

	_initial_io_setup = false;

	if (is_master ()) {
		node.get_property (X_("volume-applies-to-output"), _volume_applies_to_output);
		if (_volume_applies_to_output) {
			_volume->deactivate ();
			_volume->set_display_to_user (false);
			main_outs()->set_gain_control (_volume_control);
		} else {
			_volume->set_display_to_user (true);
			_volume->activate ();
			main_outs()->set_gain_control (std::shared_ptr<GainControl> ());
		}
	}

	set_processor_state (processor_state, version);

	// this looks up the internal instrument in processors
	reset_instrument_info();

	MeterPoint mp;
	if (node.get_property (X_("meter-point"), mp)) {
		set_meter_point (mp);
		if (_meter) {
			_meter->set_display_to_user (_meter_point == MeterCustom);
		}
	}

	bool denormal_protection;
	if (node.get_property (X_("denormal-protection"), denormal_protection)) {
		set_denormal_protection (denormal_protection);
	}

	/* convert old 3001 state */
	std::string phase_invert_str;
	if (node.get_property (X_("phase-invert"), phase_invert_str)) {
		_phase_control->set_phase_invert (boost::dynamic_bitset<> (phase_invert_str));
	}

	bool is_active;
	if (node.get_property (X_("active"), is_active)) {
		set_active (is_active, this);
	}

	std::string id_string;
	if (node.get_property (X_("processor-after-last-custom-meter"), id_string)) {
		PBD::ID id (id_string);
		Glib::Threads::RWLock::ReaderLock lm (_processor_lock);
		ProcessorList::const_iterator i = _processors.begin ();
		while (i != _processors.end() && (*i)->id() != id) {
			++i;
		}

		if (i != _processors.end ()) {
			_processor_after_last_custom_meter = *i;
			_custom_meter_position_noted = true;
		}
	}

	for (niter = nlist.begin(); niter != nlist.end(); ++niter){
		child = *niter;

		if (child->name() == X_("Comment")) {

			/* XXX this is a terrible API design in libxml++ */

			XMLNode *cmt = *(child->children().begin());
			_comment = cmt->content();

		}  else if (child->name() == Controllable::xml_node_name) {
			std::string control_name;
			if (!child->get_property (X_("name"), control_name)) {
				continue;
			}

			if (control_name == _solo_control->name()) {
				_solo_control->set_state (*child, version);
			} else if (control_name == _solo_safe_control->name()) {
				_solo_safe_control->set_state (*child, version);
			} else if (control_name == _solo_isolate_control->name()) {
				_solo_isolate_control->set_state (*child, version);
			} else if (control_name == _mute_control->name()) {
				_mute_control->set_state (*child, version);
			} else if (_volume_control && control_name == _volume_control->name()) {
				_volume_control->set_state (*child, version);
			} else if (control_name == _phase_control->name()) {
				_phase_control->set_state (*child, version);
			} else if (control_name == "recenable" && version <= 3002) {
				/* ignore (now "rec-enable"), handled by Track */
			} else {
				Evoral::Parameter p = EventTypeMap::instance().from_symbol (control_name);
				if (p.type () >= MidiCCAutomation && p.type () < MidiSystemExclusiveAutomation) {
					std::shared_ptr<AutomationControl> ac = automation_control (p, true);
					if (ac) {
						ac->set_state (*child, version);
					}
				}
			}
		} else if (child->name() == MuteMaster::xml_node_name) {
			_mute_master->set_state (*child, version);

		} else if (child->name() == Automatable::xml_node_name) {
			set_automation_xml_state (*child, Evoral::Parameter(NullAutomation));
		}
	}

	if (_delayline) {
		_delayline->set_name (name ());
	}

	return 0;
}

int
Route::set_state_2X (const XMLNode& node, int version)
{
	LocaleGuard lg;
	XMLNodeList nlist;
	XMLNodeConstIterator niter;
	XMLNode *child;
	XMLProperty const * prop;

	/* 2X things which still remain to be handled:
	 * default-type
	 * automation
	 * controlouts
	 */

	if (node.name() != "Route") {
		error << string_compose(_("Bad node sent to Route::set_state() [%1]"), node.name()) << endmsg;
		return -1;
	}

	Stripable::set_state (node, version);

	if ((prop = node.property (X_("denormal-protection"))) != 0) {
		set_denormal_protection (string_to<bool> (prop->value()));
	}

	if ((prop = node.property (X_("muted"))) != 0) {

		bool first = true;
		bool muted = string_to<bool> (prop->value());

		if (muted) {

			string mute_point;

			if ((prop = node.property (X_("mute-affects-pre-fader"))) != 0) {

				if (string_to<bool> (prop->value())){
					mute_point = mute_point + "PreFader";
					first = false;
				}
			}

			if ((prop = node.property (X_("mute-affects-post-fader"))) != 0) {

				if (string_to<bool> (prop->value())){

					if (!first) {
						mute_point = mute_point + ",";
					}

					mute_point = mute_point + "PostFader";
					first = false;
				}
			}

			if ((prop = node.property (X_("mute-affects-control-outs"))) != 0) {

				if (string_to<bool> (prop->value())){

					if (!first) {
						mute_point = mute_point + ",";
					}

					mute_point = mute_point + "Listen";
					first = false;
				}
			}

			if ((prop = node.property (X_("mute-affects-main-outs"))) != 0) {

				if (string_to<bool> (prop->value())){

					if (!first) {
						mute_point = mute_point + ",";
					}

					mute_point = mute_point + "Main";
				}
			}

			_mute_master->set_mute_points (mute_point);
			_mute_master->set_muted_by_self (true);
		}
	}

	if ((prop = node.property (X_("meter-point"))) != 0) {
		_meter_point = MeterPoint (string_2_enum (prop->value (), _meter_point));
	}

	/* IOs */

	nlist = node.children ();
	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {

		child = *niter;

		if (child->name() == IO::state_node_name) {

			/* there is a note in IO::set_state_2X() about why we have to call
			   this directly.
			   */

			_input->set_state_2X (*child, version, true);
			_output->set_state_2X (*child, version, false);

			if ((prop = child->property (X_("name"))) != 0) {
				Route::set_name (prop->value ());
			}

			set_id (*child);

			if ((prop = child->property (X_("active"))) != 0) {
				bool yn = string_to<bool> (prop->value());
				_active = !yn; // force switch
				set_active (yn, this);
			}

			if ((prop = child->property (X_("gain"))) != 0) {
				gain_t val;

				if (sscanf (prop->value().c_str(), "%f", &val) == 1) {
					_amp->gain_control()->set_value (val, Controllable::NoGroup);
				}
			}

			/* Set up Panners in the IO */
			XMLNodeList io_nlist = child->children ();

			XMLNodeConstIterator io_niter;
			XMLNode *io_child;

			for (io_niter = io_nlist.begin(); io_niter != io_nlist.end(); ++io_niter) {

				io_child = *io_niter;

				if (io_child->name() == X_("Panner")) {
					_main_outs->panner_shell()->set_state(*io_child, version);
				} else if (io_child->name() == X_("Automation")) {
					/* IO's automation is for the fader */
					_amp->set_automation_xml_state (*io_child, Evoral::Parameter (GainAutomation));
				}
			}
		}
	}

	XMLNodeList redirect_nodes;

	for (niter = nlist.begin(); niter != nlist.end(); ++niter){

		child = *niter;

		if (child->name() == X_("Send") || child->name() == X_("Insert")) {
			redirect_nodes.push_back(child);
		}

	}

	set_processor_state_2X (redirect_nodes, version);

	Stateful::save_extra_xml (node);

	for (niter = nlist.begin(); niter != nlist.end(); ++niter){
		child = *niter;

		if (child->name() == X_("Comment")) {

			/* XXX this is a terrible API design in libxml++ */

			XMLNode *cmt = *(child->children().begin());
			_comment = cmt->content();

		} else if (child->name() == Controllable::xml_node_name && (prop = child->property("name")) != 0) {
			if (prop->value() == X_("solo")) {
				_solo_control->set_state (*child, version);
			} else if (prop->value() == X_("mute")) {
				_mute_control->set_state (*child, version);
			}

		}
	}

	bool phase_invert; /* yes / no - apply to all channels */
	if (node.get_property (X_("phase-invert"), phase_invert)) {
		/* phase_control is not usually configured at this point in time
		 * _phase_control->count() == 0. However in v2, polarity invert
		 * is directly after the input, so the input channel count can be used.
		 * NB. v2 busses: polarity invert was only applied to inputs. Aux-return
		 * was not affected. This is no longer the case (and may break sessions).
		 */
		uint64_t pol_cnt = std::max ((uint64_t)_input->n_ports().n_audio (), _phase_control->count ());
		for (uint64_t c = 0; c < pol_cnt; ++c) {
			_phase_control->set_phase_invert (c, phase_invert);
		}
	}

	return 0;
}

XMLNode&
Route::get_processor_state ()
{
	XMLNode* root = new XMLNode (X_("redirects"));
	for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
		root->add_child_nocopy ((*i)->get_state ());
	}

	return *root;
}

void
Route::set_processor_state_2X (XMLNodeList const & nList, int version)
{
	/* We don't bother removing existing processors not in nList, as this
	   method will only be called when creating a Route from scratch, not
	   for undo purposes.  Just put processors in at the appropriate place
	   in the list.
	*/

	for (XMLNodeConstIterator i = nList.begin(); i != nList.end(); ++i) {
		add_processor_from_xml_2X (**i, version);
	}
}

void
Route::set_processor_state (const XMLNode& node, int version)
{
	const XMLNodeList &nlist = node.children();

	if (nlist.empty()) {
		return;
	}

	XMLNodeConstIterator niter;
	ProcessorList new_order;
	bool must_configure = false;
	Temporal::TimeDomainProvider const & tdp (*this);

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {

		XMLProperty* prop = (*niter)->property ("type");

		if (prop->value() == "amp") {
			_amp->set_state (**niter, version);
			new_order.push_back (_amp);
		} else if (prop->value() == "trim") {
			_trim->set_state (**niter, version);
			new_order.push_back (_trim);
		} else if (prop->value() == "main-volume") {
			assert (is_master ());
			_volume->set_state (**niter, version);
			new_order.push_back (_volume);
#ifdef HAVE_BEATBOX
		} else if (prop->value() == "beatbox" && _beatbox) {
			_beatbox->set_state (**niter, Stateful::current_state_version);
			new_order.push_back (_beatbox);
#endif
		} else if (prop->value() == "meter") {
			_meter->set_state (**niter, version);
			new_order.push_back (_meter);
		} else if (prop->value() == "polarity") {
			_polarity->set_state (**niter, version);
			new_order.push_back (_polarity);
		} else if (prop->value() == "delay") {
			// skip -- internal
		} else if (prop->value() == "main-outs") {
			_main_outs->set_state (**niter, version);
		} else if (prop->value() == "monreturn") {
			if (!_intreturn) {
				_intreturn.reset (new MonitorReturn (_session, tdp));
				must_configure = true;
			}
			_intreturn->set_state (**niter, version);
		} else if (prop->value() == "intreturn") {
			if (!_intreturn) {

				if (is_monitor ()) {
					_intreturn.reset (new MonitorReturn (_session, tdp));
				} else {
					_intreturn.reset (new InternalReturn (_session, tdp));
				}

				must_configure = true;
			}
			_intreturn->set_state (**niter, version);
		} else if ((is_monitor() || is_surround_master ()) && prop->value() == "monitor") {
			if (!_monitor_control) {
				_monitor_control.reset (new MonitorProcessor (_session));
				must_configure = true;
			}
			_monitor_control->set_state (**niter, version);
		} else if (prop->value() == "capture") {
			/* CapturingProcessor should never be restored, it's always
			   added explicitly when needed */
		} else if (prop->value() == "diskreader" && _disk_reader) {
			_disk_reader->set_state (**niter, version);
			new_order.push_back (_disk_reader);
		} else if (prop->value() == "diskwriter" && _disk_writer) {
			_disk_writer->set_state (**niter, version);
			new_order.push_back (_disk_writer);
		} else if (prop->value() == "triggerbox") {
			if (!_triggerbox) {
				_triggerbox.reset (new TriggerBox (_session, _default_type));
				_triggerbox->set_owner (this);
			}
			_triggerbox->set_state (**niter, version);

			new_order.push_back (_triggerbox);
		} else {
			set_processor_state (**niter, version, prop, new_order, must_configure);
		}
	}

	ProcessorList old_list = _processors; // keep a copy
	{
		Glib::Threads::Mutex::Lock lx (AudioEngine::instance()->process_lock ());
		Glib::Threads::RWLock::WriterLock lm (_processor_lock);
		/* re-assign _processors w/o process-lock.
		 * if there's an IO-processor present in _processors but
		 * not in new_order, it will be deleted and ~IO takes
		 * a process lock.
		 */
		_processors = new_order;

		/* When a required/existing internal processor is not in the list, it needs to
		 * be added via configure_processors() -> setup_invisible_processors()
		 */
		if (_monitor_control) {
			must_configure |= find (_processors.begin(), _processors.end(), _monitor_control) == _processors.end ();
		}
		if (_main_outs) {
			must_configure |= find (_processors.begin(), _processors.end(), _main_outs) == _processors.end ();
		}
		if (_delayline) {
			must_configure |= find (_processors.begin(), _processors.end(), _delayline) == _processors.end ();
		}
		if (_intreturn) {
			must_configure |= find (_processors.begin(), _processors.end(), _intreturn) == _processors.end ();
		}

		if (must_configure && !_session.loading()) {
			configure_processors_unlocked (0, &lm);
		}

		for (ProcessorList::const_iterator i = _processors.begin(); i != _processors.end(); ++i) {

			(*i)->set_owner (this);
			(*i)->ActiveChanged.connect_same_thread (**i, std::bind (&Session::queue_latency_recompute, &_session));

			std::shared_ptr<PluginInsert> pi;

			if ((pi = std::dynamic_pointer_cast<PluginInsert>(*i)) != 0) {
				if (pi->has_no_inputs ()) {
					_have_internal_generator = true;
					break;
				}
			}
		}
	}
	/* drop references w/o process-lock (I/O procs may re-take it in ~IO() */
	old_list.clear ();

	reset_instrument_info ();
	processors_changed (RouteProcessorChange ()); /* EMIT SIGNAL */
	set_processor_positions ();
}

bool
Route::set_processor_state (XMLNode const& node, int version, XMLProperty const* prop, ProcessorList& new_order, bool& must_configure)
{
	ProcessorList::iterator o;
	Temporal::TimeDomainProvider const & tdp (*this);

	for (o = _processors.begin(); o != _processors.end(); ++o) {
		XMLProperty const * id_prop = node.property(X_("id"));
		if (id_prop && (*o)->id() == id_prop->value()) {
			(*o)->set_state (node, version);
			new_order.push_back (*o);
			break;
		}
	}

	// If the processor (node) is not on the route then create it

	if (o == _processors.end()) {

		std::shared_ptr<Processor> processor;

		if (prop->value() == "intsend") {

			processor.reset (new InternalSend (_session, _pannable, _mute_master, std::dynamic_pointer_cast<ARDOUR::Route>(shared_from_this()), std::shared_ptr<Route>(), Delivery::Aux, true));

		} else if (prop->value() == "ladspa" || prop->value() == "Ladspa" ||
		           prop->value() == "lv2" ||
		           prop->value() == "windows-vst" ||
		           prop->value() == "mac-vst" ||
		           prop->value() == "lxvst" ||
		           prop->value() == "luaproc" ||
		           prop->value() == "vst3" ||
		           prop->value() == "audiounit") {

			if (_session.get_disable_all_loaded_plugins ()) {
				processor.reset (new UnknownProcessor (_session, node, this));
			} else {
				processor.reset (new PluginInsert (_session, tdp));
				processor->set_owner (this);
			}
		} else if (prop->value() == "port") {

			processor.reset (new PortInsert (_session, _pannable, _mute_master));
			processor->set_owner (this);

		} else if (prop->value() == "send") {
#ifndef LIVETRAX
			Delivery::Role role;
			if (Delivery::role_from_xml (node, role) && role == Delivery::DirectOuts) {
				return true;
			}
#endif

			processor.reset (new Send (_session, _pannable, _mute_master, Delivery::Send, true));
			std::shared_ptr<Send> send = std::dynamic_pointer_cast<Send> (processor);
			send->SelfDestruct.connect_same_thread (*send, std::bind (&Route::processor_selfdestruct, this, std::weak_ptr<Processor> (processor)));
			if (send->output()) {
				send->output()->changed.connect_same_thread (*send, std::bind (&Route::output_change_handler, this, _1, _2));
			}

		} else if (prop->value() == "sursend") {
			_surround_send.reset (new SurroundSend (_session, _mute_master));
			_surround_send->set_owner (this);
			processor = _surround_send;
		} else if (prop->value() == "surreturn") {
			if (_surround_return) {
				_surround_return->set_state (node, version);
			}
			return true;
		} else {
			warning << string_compose(_("unknown Processor type \"%1\"; ignored"), prop->value()) << endmsg;
			return false;
		}

		if (processor->set_state (node, version) != 0) {
			/* This processor could not be configured.  Turn it into a UnknownProcessor */
			processor.reset (new UnknownProcessor (_session, node, this));
		}

		/* set strict I/O only after loading plugin state, because
		 * individual plugins may override this */
		std::shared_ptr<PluginInsert> pi = std::dynamic_pointer_cast<PluginInsert> (processor);
		if (pi && _strict_io) {
			pi->set_strict_io (true);
		}

		/* subscribe to Sidechain IO changes */
		if (pi && pi->has_sidechain ()) {
			pi->sidechain_input ()->changed.connect_same_thread (*pi, std::bind (&Route::sidechain_change_handler, this, _1, _2));
		}

		/* we have to note the monitor send here, otherwise a new one will be created
		   and the state of this one will be lost.
		*/
		std::shared_ptr<InternalSend> isend = std::dynamic_pointer_cast<InternalSend> (processor);
		if (isend && isend->role() == Delivery::Listen) {
			_monitor_send = isend;
		}

		/* it doesn't matter if invisible processors are added here, as they
		   will be sorted out by setup_invisible_processors () shortly.
		*/

		new_order.push_back (processor);
		must_configure = true;
	}
	return true;
}

int
Route::silence (pframes_t nframes)
{
	Glib::Threads::RWLock::ReaderLock lm (_processor_lock, Glib::Threads::TRY_LOCK);
	if (!lm.locked()) {
		return 1;
	}

	silence_unlocked (nframes);
	return 0;
}

void
Route::silence_unlocked (pframes_t nframes)
{
	/* Must be called with the processor lock held */

	const samplepos_t now = _session.transport_sample ();

	_output->silence (nframes);

	// update owned automated controllables
	automation_run (now, nframes);
	if (_pannable) {
		_pannable->automation_run (now, nframes);
	}

	for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
		std::shared_ptr<PluginInsert> pi;

		if (!_active && (pi = std::dynamic_pointer_cast<PluginInsert> (*i)) != 0) {
			/* evaluate automated automation controls */
			pi->automation_run (now, nframes);
			/* skip plugins, they don't need anything when we're not active */
			continue;
		}

		(*i)->silence (nframes, now);
	}
}

void
Route::add_internal_return ()
{
	if (!_intreturn) {
		_intreturn.reset (new InternalReturn (_session, *this));
		add_processor (_intreturn, PreFader);
	}
}

void
Route::add_send_to_internal_return (InternalSend* send)
{
	Glib::Threads::RWLock::ReaderLock rm (_processor_lock);

	for (ProcessorList::const_iterator x = _processors.begin(); x != _processors.end(); ++x) {
		std::shared_ptr<InternalReturn> d = std::dynamic_pointer_cast<InternalReturn>(*x);

		if (d) {
			return d->add_send (send);
		}
	}
}

void
Route::remove_send_from_internal_return (InternalSend* send)
{
	Glib::Threads::RWLock::ReaderLock rm (_processor_lock);

	for (ProcessorList::const_iterator x = _processors.begin(); x != _processors.end(); ++x) {
		std::shared_ptr<InternalReturn> d = std::dynamic_pointer_cast<InternalReturn>(*x);

		if (d) {
			return d->remove_send (send);
		}
	}
}

void
Route::enable_monitor_send ()
{
	/* Caller must hold process lock */
	assert (!AudioEngine::instance()->process_lock().trylock());

	/* master never sends to monitor section via the normal mechanism */
	assert (!is_master ());
	assert (!is_monitor ());
	assert (!is_surround_master ());

	/* make sure we have one */
	if (!_monitor_send) {
		_monitor_send.reset (new InternalSend (_session, _pannable, _mute_master, std::dynamic_pointer_cast<ARDOUR::Route>(shared_from_this()), _session.monitor_out(), Delivery::Listen));
		_monitor_send->set_display_to_user (false);
	}

	/* set it up */
	configure_processors (0);
}

/** Add an aux send to a route.
 *  @param route route to send to.
 *  @param before Processor to insert before, or 0 to insert at the end.
 */
int
Route::add_aux_send (std::shared_ptr<Route> route, std::shared_ptr<Processor> before)
{
	assert (route != _session.monitor_out ());

	{
		Glib::Threads::RWLock::ReaderLock rm (_processor_lock);

		for (ProcessorList::iterator x = _processors.begin(); x != _processors.end(); ++x) {

			std::shared_ptr<InternalSend> d = std::dynamic_pointer_cast<InternalSend> (*x);

			if (d && d->target_route() == route) {
				/* already listening via the specified IO: do nothing */
				return 0;
			}
		}
	}

	try {

		std::shared_ptr<InternalSend> listener;

		{
			Glib::Threads::Mutex::Lock lm (AudioEngine::instance()->process_lock ());
			listener.reset (new InternalSend (_session, _pannable, _mute_master, std::dynamic_pointer_cast<ARDOUR::Route>(shared_from_this()), route, Delivery::Aux));
		}

		add_processor (listener, before);

	} catch (failed_constructor& err) {
		return -1;
	}

	return 0;
}

int
Route::add_foldback_send (std::shared_ptr<Route> route, bool post_fader)
{
	assert (route != _session.monitor_out ());
	std::shared_ptr<Processor> before;
	if (post_fader) {
		before = before_processor_for_placement (PostFader);
	} else {
		before = before_processor_for_placement (PreFader);
	}

	{
		Glib::Threads::RWLock::ReaderLock rm (_processor_lock);

		for (ProcessorList::iterator x = _processors.begin(); x != _processors.end(); ++x) {

			std::shared_ptr<InternalSend> d = std::dynamic_pointer_cast<InternalSend> (*x);

			if (d && d->target_route() == route) {
				/* already listening via the specified IO: do nothing */
				return 0;
			}
		}
	}

	try {

		std::shared_ptr<InternalSend> listener;

		{
			Glib::Threads::Mutex::Lock lm (AudioEngine::instance()->process_lock ());
			listener.reset (new InternalSend (_session, _pannable, _mute_master, std::dynamic_pointer_cast<ARDOUR::Route>(shared_from_this()), route, Delivery::Foldback));
		}

		listener->panner_shell()->set_linked_to_route (false);
		listener->set_pre_fader (!post_fader);
		add_processor (listener, before);

	} catch (failed_constructor& err) {
		return -1;
	}
	_session.FBSendsChanged ();

	return 0;
}

void
Route::remove_monitor_send ()
{
	/* caller needs to hold process lock */
	if (!_monitor_send) {
		return;
	}
	ProcessorStreams err;
	remove_processor (_monitor_send, &err, false);
	_monitor_send.reset ();
}

void
Route::set_comment (string cmt, void *src)
{
	_comment = cmt;
	comment_changed ();
	_session.set_dirty ();
}

IOVector
Route::all_inputs () const
{
	/* TODO, if this works as expected,
	 * cache the IOVector and maintain it via
	 * input_change_handler(), sidechain_change_handler() etc
	 */
	IOVector ios;
	ios.push_back (_input);

	Glib::Threads::RWLock::ReaderLock lm (_processor_lock);
	for (ProcessorList::const_iterator r = _processors.begin(); r != _processors.end(); ++r) {

		std::shared_ptr<IOProcessor> iop = std::dynamic_pointer_cast<IOProcessor>(*r);
		std::shared_ptr<PluginInsert> pi = std::dynamic_pointer_cast<PluginInsert>(*r);
		if (pi != 0) {
			assert (iop == 0);
			iop = pi->sidechain();
		}

		if (iop != 0 && iop->input()) {
			ios.push_back (iop->input());
		}
	}
	return ios;
}

IOVector
Route::all_outputs () const
{
	IOVector ios;
	// _output is included via Delivery
	Glib::Threads::RWLock::ReaderLock lm (_processor_lock);
	for (ProcessorList::const_iterator r = _processors.begin(); r != _processors.end(); ++r) {
		std::shared_ptr<IOProcessor> iop = std::dynamic_pointer_cast<IOProcessor>(*r);
		if (iop != 0 && iop->output()) {
			ios.push_back (iop->output());
		}
	}
	return ios;
}

bool
Route::direct_feeds_according_to_reality (std::shared_ptr<GraphNode> node, bool* via_send_only)
{
	std::shared_ptr<Route> other (std::dynamic_pointer_cast<Route> (node));
	assert (other);

	IOVector const& other_inputs (other->all_inputs());

	DEBUG_TRACE (DEBUG::Graph, string_compose ("Feeds from %1 (-> %2)?\n", _name, other->name()));
	if (other_inputs.fed_by (_output)) {
		DEBUG_TRACE (DEBUG::Graph, string_compose ("\tdirect FEEDS to %1\n", other->name()));
		if (via_send_only) {
			*via_send_only = false;
		}

		return true;
	}

	Glib::Threads::RWLock::ReaderLock lm (_processor_lock);

	/* our surround send always feeds the surround master */
	if (other->is_surround_master () && _surround_send) {
		if (via_send_only) {
			*via_send_only = true;
		}
		return true;
	}

	for (ProcessorList::iterator r = _processors.begin(); r != _processors.end(); ++r) {

		std::shared_ptr<IOProcessor> iop = std::dynamic_pointer_cast<IOProcessor>(*r);
		std::shared_ptr<PluginInsert> pi = std::dynamic_pointer_cast<PluginInsert>(*r);
		if (pi != 0) {
			assert (iop == 0);
			iop = pi->sidechain();
		}

		if (iop != 0) {
			std::shared_ptr<const IO> iop_out = iop->output();
			if (other.get() == this && iop_out && iop->input() && iop_out->connected_to (iop->input())) {
				// TODO this needs a delaylines in the Insert to align connections (!)
				DEBUG_TRACE (DEBUG::Graph,  string_compose ("\tIOP %1 does feed its own return (%2)\n", iop->name(), other->name()));
				continue;
			}
			if (iop->feeds (other) || (iop_out && other_inputs.fed_by (iop_out))) {
				DEBUG_TRACE (DEBUG::Graph,  string_compose ("\tIOP %1 does feed %2\n", iop->name(), other->name()));
				if (via_send_only) {
					*via_send_only = true;
				}
				return true;
			} else {
				DEBUG_TRACE (DEBUG::Graph,  string_compose ("\tIOP %1 does NOT feed %2\n", iop->name(), other->name()));
			}
		}
	}

	DEBUG_TRACE (DEBUG::Graph,  string_compose ("\tdoes NOT feed %1\n", other->name()));
	return false;
}

bool
Route::direct_feeds_according_to_graph (std::shared_ptr<Route> other, bool* via_send_only)
{
	return _session._current_route_graph.has (std::dynamic_pointer_cast<Route> (shared_from_this ()), other, via_send_only);
}

bool
Route::feeds (std::shared_ptr<Route> other)
{
	return _session._current_route_graph.feeds (std::dynamic_pointer_cast<Route> (shared_from_this ()), other);
}

std::set<std::shared_ptr<Route>>
Route::signal_sources (bool via_sends_only)
{
	std::set<std::shared_ptr<Route>> rv;
	for (auto const& i : _session._current_route_graph.to (std::dynamic_pointer_cast<Route> (shared_from_this ()), via_sends_only)) {
		std::shared_ptr<Route> r = std::dynamic_pointer_cast<Route> (i);
		if (r) {
			rv.insert (r);
		}
	}
	return rv;
}

bool
Route::output_effectively_connected () const
{
	_connection_cache.clear ();
	return output_effectively_connected_real ();
}

bool
Route::output_effectively_connected_real () const
{
	if (!_output->connected ()) {
		return false;
	}

	if (is_master() || is_monitor ()) {
		return _output->connected ();
	}

	if (_output->physically_connected ()) {
		return true;
	}

	/* now follow connections downstream */
	std::shared_ptr<RouteList const> routes = _session.get_routes ();
	for (auto const& i : *routes) {
		Route* rp = i.get();
		if (rp == this) {
			continue;
		}
		if (!i->input()->connected_to (_output)) {
			continue;
		}
		if (_connection_cache.find (rp) != _connection_cache.end ()) {
			return _connection_cache[rp];
		}
		/* First mark node a traversed to prevent endless recursion.
		 * Otherwise graph loops A -> B -> A will cause a stack overflow.
		 */
		_connection_cache[rp] = false;

		/* recurse downstream, check connected route */
		bool rv = i->output_effectively_connected_real ();
		_connection_cache[rp] = rv;
		if (rv) {
			return true;
		}
	}
	return false;
}

/** Called from the (non-realtime) butler thread when the transport is stopped */
void
Route::non_realtime_transport_stop (samplepos_t now, bool flush)
{
	Glib::Threads::RWLock::ReaderLock lm (_processor_lock);

	Automatable::non_realtime_transport_stop (now, flush);

	for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {

		if (!_have_internal_generator && (Config->get_plugins_stop_with_transport() && flush)) {
			(*i)->flush ();
		}

		(*i)->non_realtime_transport_stop (now, flush);
	}
}

void
Route::realtime_handle_transport_stopped ()
{
	Glib::Threads::RWLock::ReaderLock lm (_processor_lock);

	/* currently only by Plugin, queue note-off events */
	for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
		(*i)->realtime_handle_transport_stopped ();
	}
}


void
Route::input_change_handler (IOChange change, void * /*src*/)
{
	if (_session.loading ()) {
		return;
	}

	if ((change.type & IOChange::ConfigurationChanged)) {
		/* This is called with the process lock held if change
		   contains ConfigurationChanged
		*/
		configure_processors (0);
		io_changed (); /* EMIT SIGNAL */
	}

	if (_initial_io_setup) {
		return;
	}

	if (_solo_control->soloed_by_others_upstream() || _solo_isolate_control->solo_isolated_by_upstream()) {
		int sbou = 0;
		int ibou = 0;
		std::shared_ptr<RouteList const> routes = _session.get_routes ();
		if (_input->connected()) {
			for (auto const& i : *routes) {
				if (i.get() == this || i->is_master() || i->is_monitor() || i->is_auditioner()) {
					continue;
				}
				if (i->direct_feeds_according_to_reality (std::dynamic_pointer_cast<Route> (shared_from_this()))) {
					if (i->soloed()) {
						++sbou;
					}
					if (i->solo_isolate_control()->solo_isolated()) {
						++ibou;
					}
				}
			}
		}

		int delta  = sbou - _solo_control->soloed_by_others_upstream();
		int idelta = ibou - _solo_isolate_control->solo_isolated_by_upstream();

		if (idelta < -1) {
			PBD::warning << string_compose (
			                _("Invalid Solo-Isolate propagation: from:%1 new:%2 - old:%3 = delta:%4"),
			                _name, ibou, _solo_isolate_control->solo_isolated_by_upstream(), idelta)
			             << endmsg;

		}

		if (_solo_control->soloed_by_others_upstream()) {
			// ignore new connections (they're not propagated)
			if (delta <= 0) {
				_solo_control->mod_solo_by_others_upstream (delta);
			}
		}

		if (_solo_isolate_control->solo_isolated_by_upstream()) {
			// solo-isolate currently only propagates downstream
			if (idelta < 0) {
				_solo_isolate_control->mod_solo_isolated_by_upstream (1);
			}
			//_solo_isolated_by_upstream = ibou;
		}

		// Session::route_solo_changed  does not propagate indirect solo-changes
		// propagate downstream to tracks
		for (auto const& i : *routes) {
			if (i.get() == this || i->is_master() || i->is_monitor() || i->is_auditioner()) {
				continue;
			}
			bool does_feed = feeds (i);
			if (delta <= 0 && does_feed) {
				i->solo_control()->mod_solo_by_others_upstream (delta);
			}

			if (idelta < 0 && does_feed) {
				i->solo_isolate_control()->mod_solo_isolated_by_upstream (-1);
			}
		}
	}
}

void
Route::output_change_handler (IOChange change, void * /*src*/)
{
	if (_initial_io_setup) {
		return;
	}

	if ((change.type & IOChange::ConfigurationChanged)) {
		/* This is called with the process lock held if change
		   contains ConfigurationChanged
		*/
		configure_processors (0);

		if (is_master()) {
			_session.reset_monitor_section();
		}

		io_changed (); /* EMIT SIGNAL */
	}

	if (_session.loading ()) {
		return;
	}

	if ((change.type & IOChange::ConnectionsChanged)) {

		/* do this ONLY if connections have changed. Configuration
		 * changes do not, by themselves alter solo upstream or
		 * downstream status.
		 */

		if (_solo_control->soloed_by_others_downstream()) {
			int sbod = 0;
			/* checking all all downstream routes for
			 * explicit of implict solo is a rather drastic measure,
			 * ideally the input_change_handler() of the other route
			 * would propagate the change to us.
			 */
			std::shared_ptr<RouteList const> routes = _session.get_routes ();
			if (_output->connected()) {
				for (auto const& i : *routes) {
					if (i.get() == this || i->is_master() || i->is_monitor() || i->is_auditioner()) {
						continue;
					}
					if (direct_feeds_according_to_reality (i)) {
						if (i->soloed()) {
							++sbod;
							break;
						}
					}
				}
			}

			int delta = sbod - _solo_control->soloed_by_others_downstream();
			if (delta <= 0) {
				// do not allow new connections to change implicit solo (no propagation)
				_solo_control->mod_solo_by_others_downstream (delta);
				// Session::route_solo_changed() does not propagate indirect solo-changes
				// propagate upstream to tracks
				std::shared_ptr<Route> shared_this = std::dynamic_pointer_cast<Route> (shared_from_this());
				for (auto const& i : *routes) {
					if (i.get() == this || !can_solo()) {
						continue;
					}
					if (delta != 0 && i->feeds (shared_this)) {
						i->solo_control()->mod_solo_by_others_downstream (delta);
					}
				}

			}
		}
	}
}

void
Route::sidechain_change_handler (IOChange change, void* src)
{
	if (_initial_io_setup || _in_sidechain_setup) {
		return;
	}

	input_change_handler (change, src);
}

uint32_t
Route::pans_required () const
{
	if (n_outputs().n_audio() < 2) {
		return 0;
	}

	return max (n_inputs ().n_audio(), processor_max_streams.n_audio());
}

void
Route::flush_processor_buffers_locked (samplecnt_t nframes)
{
	for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
		std::shared_ptr<Delivery> d = std::dynamic_pointer_cast<Delivery> (*i);
		if (d) {
			d->flush_buffers (nframes);
		} else {
			std::shared_ptr<PortInsert> p = std::dynamic_pointer_cast<PortInsert> (*i);
			if (p) {
				p->flush_buffers (nframes);
			}
		}
	}
}

void
Route::flush_processors ()
{
	Glib::Threads::RWLock::ReaderLock lm (_processor_lock);

	for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
		(*i)->flush ();
	}
}

samplecnt_t
Route::playback_latency (bool incl_downstream) const
{
	samplecnt_t rv;
	if (_disk_reader) {
		rv = _disk_reader->output_latency ();
	} else {
		rv = _signal_latency;
	}
	if (incl_downstream) {
		rv += _output->connected_latency (true);
	} else {
		rv += _output_latency;
	}
	return rv;
}

pframes_t
Route::latency_preroll (pframes_t nframes, samplepos_t& start_sample, samplepos_t& end_sample)
{
	samplecnt_t latency_preroll = _session.remaining_latency_preroll ();
	if (latency_preroll == 0) {
		return nframes;
	}
	if (!_disk_reader) {
		if (_session.transport_speed (true) < 0) {
			start_sample += latency_preroll;
			end_sample   += latency_preroll;
		} else {
			start_sample -= latency_preroll;
			end_sample   -= latency_preroll;
		}
		return nframes;
	}

	if (latency_preroll > playback_latency ()) {
		no_roll_unlocked (nframes, start_sample - latency_preroll, end_sample - latency_preroll, false);
		return 0;
	}

	if (_session.transport_speed (true) < 0) {
		start_sample += latency_preroll;
		end_sample   += latency_preroll;
	} else {
		start_sample -= latency_preroll;
		end_sample -= latency_preroll;
	}
	return nframes;
}

int
Route::roll (pframes_t nframes, samplepos_t start_sample, samplepos_t end_sample, bool& need_butler)
{
	Glib::Threads::RWLock::ReaderLock lm (_processor_lock, Glib::Threads::TRY_LOCK);

	if (!lm.locked()) {
		return 0;
	}

	if (!_active) {
		silence_unlocked (nframes);
		_meter->reset();
		return 0;
	}

	if ((nframes = latency_preroll (nframes, start_sample, end_sample)) == 0) {
		return 0;
	}

	run_route (start_sample, end_sample, nframes, (!_disk_writer || !_disk_writer->record_enabled()) && _session.transport_rolling(), true);

	if ((_disk_reader && _disk_reader->need_butler()) || (_disk_writer && _disk_writer->need_butler())) {
		need_butler = true;
	}
	return 0;
}

int
Route::no_roll (pframes_t nframes, samplepos_t start_sample, samplepos_t end_sample, bool session_state_changing)
{
	Glib::Threads::RWLock::ReaderLock lm (_processor_lock, Glib::Threads::TRY_LOCK);

	if (!lm.locked()) {
		return 0;
	}

	return no_roll_unlocked (nframes, start_sample, end_sample, session_state_changing);
}

int
Route::no_roll_unlocked (pframes_t nframes, samplepos_t start_sample, samplepos_t end_sample, bool session_state_changing)
{
	/* Must be called with the processor lock held */

	if (!_active) {
		silence_unlocked (nframes);
		_meter->reset();
		return 0;
	}

	if (session_state_changing) {
		if (_session.transport_speed() != 0.0f) {
			/* we're rolling but some state is changing (e.g. our diskstream contents)
			   so we cannot use them. Be silent till this is over.

			   XXX note the absurdity of ::no_roll() being called when we ARE rolling!
			*/
			silence_unlocked (nframes);
			_meter->reset();
			return 0;
		}
		/* we're really not rolling, so we're either delivery silence or actually
		   monitoring, both of which are safe to do while session_state_changing is true.
		*/
	}

	run_route (start_sample, end_sample, nframes, false, false);
	return 0;
}

void
Route::update_send_delaylines ()
{
	Glib::Threads::RWLock::ReaderLock lm (_processor_lock);
	for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
		if (std::shared_ptr<LatentSend> snd = std::dynamic_pointer_cast<LatentSend> (*i)) {
			snd->update_delaylines (true);
		}
	}
}

#ifdef __clang__
__attribute__((annotate("realtime")))
#endif
bool
Route::apply_processor_changes_rt ()
{
	int emissions = EmitNone;

	if (_pending_meter_point != _meter_point) {
		Glib::Threads::RWLock::WriterLock pwl (_processor_lock, Glib::Threads::TRY_LOCK);
		if (pwl.locked()) {
			/* meters always have buffers for 'processor_max_streams'
			 * they can be re-positioned without re-allocation */
			if (set_meter_point_unlocked()) {
				emissions |= EmitMeterChanged | EmitMeterVisibilityChange;;
			} else {
				emissions |= EmitMeterChanged;
			}
		}
	}

	bool changed = false;

	if (_pending_process_reorder.load ()) {
		Glib::Threads::RWLock::WriterLock pwl (_processor_lock, Glib::Threads::TRY_LOCK);
		if (pwl.locked()) {
			_pending_process_reorder.store (0);
			_pending_listen_change.store (0);
			apply_processor_order (_pending_processor_order);
			_pending_processor_order.clear ();
			setup_invisible_processors ();
			changed = true;
			emissions |= EmitRtProcessorChange;
		}
	}

	if (_pending_listen_change.load ()) {
		Glib::Threads::RWLock::WriterLock pwl (_processor_lock, Glib::Threads::TRY_LOCK);
		if (pwl.locked()) {
			_pending_listen_change.store (0);
			setup_invisible_processors ();
			changed = true;
			emissions |= EmitRtProcessorChange;
		}
	}

	if (changed) {
		set_processor_positions ();
		/* update processor input/output latency
		 * (total signal_latency does not change)
		 */
		update_signal_latency (true);
	}

	if (_pending_surround_send.load ()) {
		Glib::Threads::RWLock::WriterLock pwl (_processor_lock, Glib::Threads::TRY_LOCK);
		if (pwl.locked()) {
			_pending_surround_send.store (0);
			emissions |= EmitSendReturnChange;
		}
	}

	if (emissions != 0) {
		_pending_signals.store (emissions);
		return true;
	}
	return (!selfdestruct_sequence.empty ());
}

void
Route::emit_pending_signals ()
{
	int sig = _pending_signals.fetch_and (0);
	if (sig & EmitMeterChanged) {
		_meter->emit_configuration_changed();
		meter_change (); /* EMIT SIGNAL */
		if (sig & EmitMeterVisibilityChange) {
		processors_changed (RouteProcessorChange (RouteProcessorChange::MeterPointChange, true)); /* EMIT SIGNAL */
		} else {
		processors_changed (RouteProcessorChange (RouteProcessorChange::MeterPointChange, false)); /* EMIT SIGNAL */
		}
	}
	if (sig & EmitRtProcessorChange) {
		processors_changed (RouteProcessorChange (RouteProcessorChange::RealTimeChange)); /* EMIT SIGNAL */
	}
	if (sig & EmitSendReturnChange) {
		processors_changed (RouteProcessorChange (RouteProcessorChange::SendReturnChange, false)); /* EMIT SIGNAL */
	}

	/* this would be a job for the butler.
	 * Conceptually we should not take processe/processor locks here.
	 * OTOH its more efficient (less overhead for summoning the butler and
	 * telling her what do do) and signal emission is called
	 * directly after the process callback, which decreases the chance
	 * of xruns when taking the locks.
	 */
	while (!selfdestruct_sequence.empty ()) {
		Glib::Threads::Mutex::Lock lx (selfdestruct_lock);
		if (selfdestruct_sequence.empty ()) { break; } // re-check with lock
		std::shared_ptr<Processor> proc = selfdestruct_sequence.back ().lock ();
		selfdestruct_sequence.pop_back ();
		lx.release ();
		if (proc) {
			remove_processor (proc);
		}
	}
}

void
Route::set_meter_point (MeterPoint p)
{
	if (_pending_meter_point == p) {
		return;
	}

	if (!AudioEngine::instance()->running()) {
		bool meter_visibly_changed = false;
		{
			Glib::Threads::Mutex::Lock lx (AudioEngine::instance()->process_lock ());
			Glib::Threads::RWLock::WriterLock lm (_processor_lock);
			_pending_meter_point = p;
			if (set_meter_point_unlocked ()) {
				meter_visibly_changed = true;
			}
		}
		_meter->emit_configuration_changed();
		meter_change (); /* EMIT SIGNAL */
		processors_changed (RouteProcessorChange (RouteProcessorChange::MeterPointChange, meter_visibly_changed)); /* EMIT SIGNAL */
	} else {
		_pending_meter_point = p;
	}
}


#ifdef __clang__
__attribute__((annotate("realtime")))
#endif
bool
Route::set_meter_point_unlocked ()
{
#ifndef NDEBUG
	/* Caller must hold process and processor write lock */
	assert (!AudioEngine::instance()->process_lock().trylock());
	Glib::Threads::RWLock::WriterLock lm (_processor_lock, Glib::Threads::TRY_LOCK);
	assert (!lm.locked ());
#endif

	_meter_point = _pending_meter_point;

	bool meter_was_visible_to_user = _meter->display_to_user ();

	if (!_custom_meter_position_noted) {
		maybe_note_meter_position ();
	}

	if (_meter_point != MeterCustom) {

		_meter->set_display_to_user (false);

		setup_invisible_processors ();

	} else {
		_meter->set_display_to_user (true);

		/* If we have a previous position for the custom meter, try to put it there */
		std::shared_ptr<Processor> after = _processor_after_last_custom_meter.lock ();
		if (after) {
			ProcessorList::iterator i = find (_processors.begin(), _processors.end(), after);
			if (i != _processors.end ()) {
				_processors.remove (_meter);
				_processors.insert (i, _meter);
			}
		} else {// at end, right before the mains_out/panner
			_processors.remove (_meter);
			ProcessorList::iterator main = _processors.end();
			_processors.insert (--main, _meter);
		}
	}

	/* Set up the meter for its new position */

	ProcessorList::iterator loc = find (_processors.begin(), _processors.end(), _meter);

	ChanCount m_in;

	if (loc == _processors.begin()) {
		m_in = _input->n_ports();
	} else {
		ProcessorList::iterator before = loc;
		--before;
		m_in = (*before)->output_streams ();
	}

	_meter->reflect_inputs (m_in);

	/* we do not need to reconfigure the processors, because the meter
	   (a) is always ready to handle processor_max_streams
	   (b) is always an N-in/N-out processor, and thus moving
	   it doesn't require any changes to the other processors.
	*/

	/* these should really be done after releasing the lock
	 * but all those signals are subscribed to with gui_thread()
	 * so we're safe.
	 */
	 return (_meter->display_to_user() != meter_was_visible_to_user);
}

void
Route::listen_position_changed ()
{
	if (!_monitor_send) {
		return;
	}
	/* check if re-order can be done in realtime */
	ChanCount c;

	switch (Config->get_listen_position ()) {
		case PreFaderListen:
			switch (Config->get_pfl_position ()) {
				case PFLFromBeforeProcessors:
					c = input_streams ();
					break;
				case PFLFromAfterProcessors:
					c = _amp->input_streams ();
					break;
			}
			break;
		case AfterFaderListen:
			switch (Config->get_afl_position ()) {
				case AFLFromBeforeProcessors:
					c = _amp->output_streams ();
					break;
				case AFLFromAfterProcessors:
					c = _main_outs->input_streams ();
					break;
			}
			break;
	}

	if (c == _monitor_send->input_streams () && AudioEngine::instance()->running()) {
		Glib::Threads::RWLock::ReaderLock lm (_processor_lock); // XXX is this needed?
		_pending_listen_change.store (1);
		return;
	}

	{
		Glib::Threads::Mutex::Lock lx (AudioEngine::instance()->process_lock ());
		Glib::Threads::RWLock::WriterLock lm (_processor_lock);
		ProcessorState pstate (this);

		if (configure_processors_unlocked (0, &lm)) {
			DEBUG_TRACE (DEBUG::Processors, "---- CONFIGURATION FAILED.\n");
			pstate.restore ();
			configure_processors_unlocked (0, &lm); // it worked before we tried to add it ...
			return;
		}
	}

	processors_changed (RouteProcessorChange ()); /* EMIT SIGNAL */
	_session.set_dirty ();
}

std::shared_ptr<CapturingProcessor>
Route::add_export_point()
{
	assert (!_capturing_processor);

	Glib::Threads::Mutex::Lock lx (AudioEngine::instance()->process_lock ());
	Glib::Threads::RWLock::WriterLock lw (_processor_lock);

	/* Align all tracks for stem-export w/o processing.
	 * Compensate for all plugins between the this route's disk-reader
	 * and the common final downstream output (ie alignment point for playback).
	 */
	_capturing_processor.reset (new CapturingProcessor (_session, playback_latency (true)));
	configure_processors_unlocked (0, &lw);
	_capturing_processor->activate ();

	return _capturing_processor;
}

samplecnt_t
Route::update_signal_latency (bool apply_to_delayline, bool* delayline_update_needed)
{
	if (!active()) {
		_signal_latency = 0;
		/* mark all send are inactive, set internal-return "delay-out" to zero. */
		Glib::Threads::RWLock::ReaderLock lm (_processor_lock);
		for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
			if (std::shared_ptr<LatentSend> snd = std::dynamic_pointer_cast<LatentSend> (*i)) {
				snd->set_delay_in (0);
			}
			if (std::shared_ptr<InternalReturn> rtn = std::dynamic_pointer_cast<InternalReturn> (*i)) {
				rtn->set_playback_offset (0);
			}
			if (std::shared_ptr<SurroundReturn> rtn = std::dynamic_pointer_cast<SurroundReturn> (*i)) {
				rtn->set_playback_offset (0);
			}
			// TODO sidechain inputs?!
		}
		return 0;
	}

	samplecnt_t capt_lat_in  = _input->connected_latency (false);
	samplecnt_t play_lat_out = _output->connected_latency (true);
	samplecnt_t in_latency   = _input->latency ();

	/* When the track's output is not connected, align it to master-bus
	 * (as if it would be connected to master-in).
	 * Effectively we want to configure all latent-sends to not introduce any latency.
	 * Since the output is not used, Send::_thru_delay is not relevant, and
	 * Send->effective_latency () should return zero.
	 */
	if (is_master() || is_monitor () || output_effectively_connected ()) {
		_output_latency = _output->latency ();
	} else if (_session.master_out ()) {
		/* We cannot use `_session.master_out()->input()->latency()` because that would return capture latency.
		 * Also in case of Mixbus, the master-bus does not have any inputs.
		 * So we propagate playback latency from master-out as Route::update_port_latencies() would do.
		 */
		_output_latency = _session.master_out()->output ()->latency () + _session.master_out()->signal_latency ();
	} else {
		_output_latency = 0;
	}

	Glib::Threads::RWLock::ReaderLock lm (_processor_lock);

	samplecnt_t l_in  = 0;
	samplecnt_t l_out = 0;
	for (ProcessorList::reverse_iterator i = _processors.rbegin(); i != _processors.rend(); ++i) {
		if (std::shared_ptr<LatentSend> snd = std::dynamic_pointer_cast<LatentSend> (*i)) {
			snd->set_delay_in (l_out + _output_latency);
		}

		if (std::shared_ptr<PluginInsert> pi = std::dynamic_pointer_cast<PluginInsert> (*i)) {
			if (std::shared_ptr<IO> pio = pi->sidechain_input ()) {
				samplecnt_t lat = l_out + _output_latency;
				pio->set_private_port_latencies (lat, true);
				pio->set_public_port_latencies (lat, true);
			}
		}
		(*i)->set_output_latency (l_out);
		if ((*i)->active ()) { // XXX
			l_out += (*i)->effective_latency ();
		}
	}

	DEBUG_TRACE (DEBUG::LatencyRoute, string_compose ("%1: internal signal latency = %2\n", _name, l_out));

	_signal_latency = l_out;

	for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {

		/* set sidechain, send and insert port latencies */
		if (std::shared_ptr<PortInsert> pi = std::dynamic_pointer_cast<PortInsert> (*i)) {
			if (pi->input ()) {
				/* propagate playback latency from output to input */
				pi->input ()->set_private_port_latencies (play_lat_out + l_in, true);
			}
			if (pi->output ()) {
				/* propagate capture latency from input to output */
				pi->output ()->set_private_port_latencies (capt_lat_in + l_in, false);
			}

		} else if (std::shared_ptr<Send> snd = std::dynamic_pointer_cast<Send> (*i)) {
			if (snd->output ()) {
				/* set capture latency */
				snd->output ()->set_private_port_latencies (capt_lat_in + l_in, false);
				/* take send-target's playback latency into account */
				const samplecnt_t snd_lat = snd->output ()->connected_latency (true);
				if (apply_to_delayline) {
					/* DelayLine::set_delay requires process-lock */
					snd->set_delay_out (snd_lat);
				} else if (delayline_update_needed && snd->get_delay_out () != snd_lat) {
					*delayline_update_needed = true;
				}
			}
		} else if (!apply_to_delayline && (std::dynamic_pointer_cast<InternalReturn> (*i) || std::dynamic_pointer_cast<SurroundReturn> (*i))) {
			/* InternalReturn::set_playback_offset() calls set_delay_out(), requires process lock */
			const samplecnt_t poff = _signal_latency + _output_latency;
			if (delayline_update_needed && (*i)->playback_offset () != poff) {
				*delayline_update_needed = true;
			}
		} else {
			(*i)->set_playback_offset (_signal_latency + _output_latency);
		}

		(*i)->set_input_latency (l_in);
		(*i)->set_capture_offset (in_latency);
		if ((*i)->active ()) {
			l_in += (*i)->effective_latency ();
		}
	}

	lm.release ();

	if (apply_to_delayline) {
		/* see also Session::post_playback_latency() */
		apply_latency_compensation ();
	} else if (delayline_update_needed && _delayline) {
		samplecnt_t play_lat_in = _input->connected_latency (true);
		samplecnt_t latcomp = play_lat_in - play_lat_out - _signal_latency;
		if (latcomp < 0) {
			latcomp = 0;
		}
		if (_delayline->delay () != latcomp) {
			*delayline_update_needed = true;
		}
	}

	return _signal_latency;
}

void
Route::apply_latency_compensation ()
{
	if (!_delayline) {
		return;
	}

	samplecnt_t play_lat_in = _input->connected_latency (true);
	samplecnt_t play_lat_out = _output->connected_latency (true);
	samplecnt_t latcomp = play_lat_in - play_lat_out - _signal_latency;

#if 0 // DEBUG
	samplecnt_t capt_lat_in = _input->connected_latency (false);
	samplecnt_t capt_lat_out = _output->connected_latency (false);
	samplecnt_t latcomp_capt = capt_lat_out - capt_lat_in - _signal_latency;

	cout << "ROUTE " << name() << " delay for " << latcomp << " (c: " << latcomp_capt << ")" << endl;
#endif

	if (_delayline->set_delay (latcomp > 0 ? latcomp : 0)) {
		DEBUG_TRACE (DEBUG::LatencyRoute, string_compose ("%1: delay changed to %2\n", _name, latcomp));
		/* public port latency update is needed,
		 * Session::update_latency() calls this->set_public_port_latencies()
		 */
	}
}

void
Route::set_block_size (pframes_t nframes)
{
	Glib::Threads::RWLock::ReaderLock lm (_processor_lock);
	for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
		(*i)->set_block_size (nframes);
	}
	lm.release ();

	_session.ensure_buffers (n_process_buffers ());
}

void
Route::protect_automation ()
{
	Glib::Threads::RWLock::ReaderLock lm (_processor_lock);
	for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
		(*i)->protect_automation();
	}
}

/** Shift automation forwards from a particular place, thereby inserting time.
 *  Adds undo commands for any shifts that are performed.
 *
 * @param pos Position to start shifting from.
 * @param samples Amount to shift forwards by.
 */

void
Route::shift (timepos_t const & pos, timecnt_t const & distance)
{
	ControllableSet acs;
	automatables (acs);
	for (auto& ec : acs) {
		std::shared_ptr<AutomationControl> ac = std::dynamic_pointer_cast<AutomationControl> (ec);
		if (!ac) {
			continue;
		}
		std::shared_ptr<AutomationList> al = ac->alist();
		if (!al || al->empty ()) {
			continue;
		}

		XMLNode &before = al->get_state ();
		al->shift (pos, timecnt_t (distance));
		XMLNode& after = al->get_state ();
		_session.add_command (new MementoCommand<AutomationList> (*al.get(), &before, &after));
	}
}

void
Route::cut_copy_section (timepos_t const& start, timepos_t const& end, timepos_t const& to, SectionOperation const op)
{
	ControllableSet acs;
	automatables (acs);
	for (auto& ec : acs) {
		std::shared_ptr<AutomationControl> ac = std::dynamic_pointer_cast<AutomationControl> (ec);
		if (!ac) {
			continue;
		}
		std::shared_ptr<AutomationList> al = ac->alist();
		if (!al || al->empty ()) {
			continue;
		}

		XMLNode &before = al->get_state ();
		std::shared_ptr<Evoral::ControlList> cl;
		if (op == CopyPasteSection) {
			cl = al->copy (start, end);
		} else if (op == CutPasteSection) {
			cl = al->cut (start, end);
		} else if (op == DeleteSection) {
			al->clear (start, end);
		}

		if (op == CutPasteSection || op == DeleteSection) {
			/* remove time (negative distance), ripple */
			al->shift (start, end.distance (start));
		}

		if (op != DeleteSection) {
			/* make space at the inserion point */
			al->shift (to, start.distance (end));
		}
		if (op == CopyPasteSection || op == CutPasteSection) {
			al->paste (*cl, to);
		}

		XMLNode &after = al->get_state ();
		_session.add_command (new MementoCommand<AutomationList> (*al.get(), &before, &after));
	}
}

void
Route::set_plugin_state_dir (std::weak_ptr<Processor> p, const std::string& d)
{
	std::shared_ptr<Processor> processor (p.lock ());
	std::shared_ptr<PluginInsert> pi  = std::dynamic_pointer_cast<PluginInsert> (processor);
	if (!pi) {
		return;
	}
	pi->set_state_dir (d);
}

int
Route::save_as_template (const string& path, const string& name, const string& description)
{
	std::string state_dir = path.substr (0, path.find_last_of ('.')); // strip template_suffix
	PBD::Unwinder<std::string> uw (_session._template_state_dir, state_dir);

	XMLNode& node (state (true));
	node.set_property (X_("name"), name);

	node.remove_nodes (X_("description"));
	if (!description.empty()) {
		XMLNode* desc = new XMLNode(X_("description"));
		XMLNode* desc_cont = new XMLNode(X_("content"), description);
		desc->add_child_nocopy (*desc_cont);

		node.add_child_nocopy (*desc);
	}

	XMLTree tree;

	IO::set_name_in_state (*node.children().front(), name);

	tree.set_root (&node);

	/* return zero on success, non-zero otherwise */
	return !tree.write (path.c_str());
}


bool
Route::set_name (const string& str)
{
	if (str.empty ()) {
		return false;
	}

	if (str == name()) {
		return true;
	}

	string newname = Route::ensure_track_or_route_name (str);

	if (newname == name()) {
		return true;
	}

	SessionObject::set_name (newname);

	for (uint32_t n = 0 ; ; ++n) {
		std::shared_ptr<PluginInsert> pi = std::static_pointer_cast<PluginInsert> (nth_plugin (n));
		if (!pi) {
			break;
		}
		pi->update_sidechain_name ();
	}

	bool ret = (_input->set_name(newname) && _output->set_name(newname));

	if (ret) {
		/* rename the main outs. Leave other IO processors
		 * with whatever name they already have, because its
		 * just fine as it is (it will not contain the route
		 * name if its a port insert, port send or port return).
		 */

		if (_main_outs) {
			if (!_main_outs->set_name (newname)) {
				/* XXX returning false here is stupid because
				   we already changed the route name.
				*/
				return false;
			}
		}
	}

	return ret;
}

/** Set the name of a route in an XML description.
 *  @param node XML <Route> node to set the name in.
 *  @param name New name.
 */
void
Route::set_name_in_state (XMLNode& node, string const & name)
{
	node.set_property (X_("name"), name);

	XMLNodeList children = node.children();
	for (XMLNodeIterator i = children.begin(); i != children.end(); ++i) {

		if ((*i)->name() == X_("IO")) {

			IO::set_name_in_state (**i, name);

		} else if ((*i)->name() == X_("Processor")) {

			std::string str;
			if ((*i)->get_property (X_("role"), str) && str == X_("Main")) {
				(*i)->set_property (X_("name"), name);
			}
		}
	}
}

std::shared_ptr<Send>
Route::internal_send_for (std::shared_ptr<const Route> target) const
{
	Glib::Threads::RWLock::ReaderLock lm (_processor_lock);

	for (ProcessorList::const_iterator i = _processors.begin(); i != _processors.end(); ++i) {
		std::shared_ptr<InternalSend> send;

		if ((send = std::dynamic_pointer_cast<InternalSend>(*i)) != 0) {
			if (send->target_route() == target) {
				return send;
			}
		}
	}

	return std::shared_ptr<Send>();
}

void
Route::set_denormal_protection (bool yn)
{
	if (_denormal_protection != yn) {
		_denormal_protection = yn;
		denormal_protection_changed (); /* EMIT SIGNAL */
	}
}

bool
Route::denormal_protection () const
{
	return _denormal_protection;
}

void
Route::set_active (bool yn, void* src)
{
	if (_session.transport_rolling()) {
		return;
	}

	if (_route_group && src != _route_group && _route_group->is_active() && _route_group->is_route_active()) {
		_route_group->foreach_route (std::bind (&Route::set_active, _1, yn, _route_group));
		return;
	}

	if (_active != yn) {
		_active = yn;
		_input->set_active (yn);
		_output->set_active (yn);
		flush_processors ();
		if (_active || _signal_latency > 0) {
			processor_latency_changed (); /* EMIT SIGNAL */
		}
		active_changed (); // EMIT SIGNAL
		_session.set_dirty ();
	}
}

std::shared_ptr<Pannable>
Route::pannable() const
{
	return _pannable;
}

std::shared_ptr<Panner>
Route::panner() const
{
	/* may be null ! */
	return _main_outs->panner_shell()->panner();
}

std::shared_ptr<PannerShell>
Route::panner_shell() const
{
	return _main_outs->panner_shell();
}

std::shared_ptr<GainControl>
Route::gain_control() const
{
	return _gain_control;
}

std::shared_ptr<GainControl>
Route::trim_control() const
{
	return _trim_control;
}

std::shared_ptr<GainControl>
Route::volume_control() const
{
	return _volume_control;
}

std::shared_ptr<PhaseControl>
Route::phase_control() const
{
	return _phase_control;
}

void
Route::set_volume_applies_to_output (bool en)
{
	if (!is_master () || _volume_applies_to_output == en) {
		return;
	}
	if (en) {
		_volume->deactivate ();
		_volume->set_display_to_user (false);
		main_outs()->set_gain_control (_volume_control);
		{
			/* remove hidden processor */
			Glib::Threads::Mutex::Lock lx (AudioEngine::instance()->process_lock ());
			configure_processors (NULL);
		}
		processors_changed (RouteProcessorChange ()); /* EMIT SIGNAL */
	} else {
		_volume->set_display_to_user (true);
		add_processor (_volume, PostFader, NULL, true);
		_volume->activate ();
		main_outs()->set_gain_control (std::shared_ptr<GainControl> ());
	}
	_volume_applies_to_output = en;
	_session.set_dirty ();
}

std::shared_ptr<AutomationControl>
Route::get_control (const Evoral::Parameter& param)
{
	/* either we own the control or .... */

	std::shared_ptr<AutomationControl> c = std::dynamic_pointer_cast<AutomationControl>(control (param));

	if (!c) {

		/* maybe one of our processors does or ... */

		Glib::Threads::RWLock::ReaderLock rm (_processor_lock);
		for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
			if ((c = std::dynamic_pointer_cast<AutomationControl>((*i)->control (param))) != 0) {
				break;
			}
		}
	}

	if (!c) {

		/* nobody does so we'll make a new one */

		c = std::dynamic_pointer_cast<AutomationControl>(control_factory(param));
		add_control(c);
	}

	return c;
}

std::shared_ptr<Processor>
Route::nth_plugin (uint32_t n) const
{
	Glib::Threads::RWLock::ReaderLock lm (_processor_lock);
	ProcessorList::const_iterator i;

	for (i = _processors.begin(); i != _processors.end(); ++i) {
		if (std::dynamic_pointer_cast<PluginInsert> (*i)) {
			if (n-- == 0) {
				return *i;
			}
		}
	}

	return std::shared_ptr<Processor> ();
}

std::shared_ptr<Processor>
Route::nth_send (uint32_t n) const
{
	Glib::Threads::RWLock::ReaderLock lm (_processor_lock);
	ProcessorList::const_iterator i;

	for (i = _processors.begin(); i != _processors.end(); ++i) {
		if (std::dynamic_pointer_cast<Send> (*i)) {

			if ((*i) == _monitor_send) {
				/* send to monitor section is not considered
				 * to be an accessible send.
				 */
				continue;
			}

			if (n-- == 0) {
				return *i;
			}
		}
	}

	return std::shared_ptr<Processor> ();
}

bool
Route::has_io_processor_named (const string& name)
{
	Glib::Threads::RWLock::ReaderLock lm (_processor_lock);
	ProcessorList::iterator i;

	for (i = _processors.begin(); i != _processors.end(); ++i) {
		if (std::dynamic_pointer_cast<IOProcessor> (*i)) {
			if (0 == PBD::natcmp ((*i)->name().c_str(), name.c_str())) {
				return true;
			}
		}
	}

	return false;
}

void
Route::set_processor_positions ()
{
	Glib::Threads::RWLock::ReaderLock lm (_processor_lock);

	bool had_amp = false;
	for (ProcessorList::const_iterator i = _processors.begin(); i != _processors.end(); ++i) {
		(*i)->set_pre_fader (!had_amp);
		if (*i == _amp) {
			had_amp = true;
		}
	}
}

/** Called when there is a proposed change to the input port count */
int
Route::input_port_count_changing (ChanCount to)
{
	list<pair<ChanCount, ChanCount> > c = try_configure_processors (to, 0);
	if (c.empty()) {
		/* The processors cannot be configured with the new input arrangement, so
		   block the change.
		*/
		return 1;
	}

	/* The change is ok */
	return 0;
}

/** Called when there is a proposed change to the output port count */
int
Route::output_port_count_changing (ChanCount to)
{
	if (_strict_io && !_in_configure_processors) {
		return true;
	}
	for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
		if (processor_out_streams.get(*t) > to.get(*t)) {
			return 1;
		}
	}
	/* The change is ok */
	return 0;
}

list<string>
Route::unknown_processors () const
{
	list<string> p;

	if (_session.get_disable_all_loaded_plugins ()) {
		// Do not list "missing plugins" if they are explicitly disabled
		return p;
	}

	Glib::Threads::RWLock::ReaderLock lm (_processor_lock);
	for (ProcessorList::const_iterator i = _processors.begin(); i != _processors.end(); ++i) {
		if (std::dynamic_pointer_cast<UnknownProcessor const> (*i)) {
			p.push_back ((*i)->name ());
		}
	}

	return p;
}


samplecnt_t
Route::update_port_latencies (PortSet& from, PortSet& to, bool playback, samplecnt_t our_latency) const
{
	/* we assume that all our input ports feed all our output ports. its not
	   universally true, but the alternative is way too corner-case to worry about.
	*/

	LatencyRange all_connections;
	bool connected = false;

	if (from.empty()) {
		all_connections.min = 0;
		all_connections.max = 0;
	} else {
		all_connections.min = ~((pframes_t) 0);
		all_connections.max = 0;

		/* iterate over all "from" ports and determine the latency range for all of their
		 * connections to the "outside" (outside of this Route).
		 */

		for (PortSet::iterator p = from.begin(); p != from.end(); ++p) {

			if (!p->connected ()) {
				/* ignore latency of unconnected ports, not not assume "0", they can float freely */
				continue;
			}

			LatencyRange range;
			p->get_connected_latency_range (range, playback);

			all_connections.min = min (all_connections.min, range.min);
			all_connections.max = max (all_connections.max, range.max);
			connected = true;
		}

		if (all_connections.min == ~((pframes_t) 0)) {
			all_connections.min = 0;
		}
	}

	/* if the output is not connected, its latency is not relevant,
	 * however stem-export uses the private latency of the port as alignment.
	 * see PortExportChannel::common_port_playback_latency()
	 * and PortExportChannel::prepare_export()
	 *
	 * So for correct alignment we use the Delivery's playback latency.
	 */
	/* routes without latency delayline (master, monitor) always use the
	 * actual connected latency.
	 */
	if (playback && _main_outs && !connected && _delayline) {
		all_connections.min = _main_outs->playback_offset () - _main_outs->input_latency ();
		all_connections.max = _main_outs->playback_offset () - _main_outs->input_latency ();
	}

	/* set the "from" port latencies to the max/min range of all their connections */

	for (PortSet::iterator p = from.begin(); p != from.end(); ++p) {
		p->set_private_latency_range (all_connections, playback);
	}

	DEBUG_TRACE (DEBUG::LatencyRoute, string_compose ("%1: priv. port L(%2) = (%3, %4) + %5\n", _name, playback ? "playback" : "capture", all_connections.min, all_connections.max, our_latency));

	/* set the ports "in the direction of the flow" to the same value as above plus our own signal latency */

	all_connections.min += our_latency;
	all_connections.max += our_latency;

	for (PortSet::iterator p = to.begin(); p != to.end(); ++p) {
		p->set_private_latency_range (all_connections, playback);
	}

	return all_connections.max;
}

samplecnt_t
Route::set_private_port_latencies (bool playback) const
{
	samplecnt_t own_latency = 0;

	/* Processor list not protected by lock: MUST BE CALLED FROM PROCESS THREAD
	   OR LATENCY CALLBACK.

	   This is called (early) from the latency callback. It computes the REAL
	   latency associated with each port and stores the result as the "private"
	   latency of the port. A later call to Route::set_public_port_latencies()
	   sets all ports to the same value to reflect the fact that we do latency
	   compensation and so all signals are delayed by the same amount as they
	   flow through ardour.
	*/

	for (ProcessorList::const_iterator i = _processors.begin(); i != _processors.end(); ++i) {

		if ((*i)->active ()) {
			own_latency += (*i)->effective_latency ();
		}
	}

	if (playback) {
		/* playback: propagate latency from "outside the route" to outputs to inputs */
		return update_port_latencies (*_output->ports (), *_input->ports (), true, own_latency);
	} else {
		/* capture: propagate latency from "outside the route" to inputs to outputs */
		return update_port_latencies (*_input->ports (), *_output->ports (), false, own_latency);
	}
}

void
Route::set_public_port_latencies (samplecnt_t value, bool playback, bool with_latcomp) const
{
	/* publish private latencies */
	Glib::Threads::RWLock::ReaderLock lm (_processor_lock);
	for (ProcessorList::const_iterator i = _processors.begin(); i != _processors.end(); ++i) {
		std::shared_ptr<IOProcessor> iop = std::dynamic_pointer_cast<IOProcessor>(*i);
		if (!iop) {
			continue;
		}
		if (iop->input ()) {
			assert (iop->input () != _input); // no delivery for Input
			iop->input ()->set_public_port_latencies (iop->input()->latency(), true);
		}
		if (iop->output () && iop->output () != _output) {
			iop->output ()->set_public_port_latencies (iop->output()->latency(), false);
		}
	}

	/* this is called to set the JACK-visible port latencies, which take
	 * latency compensation into account.
	 */
	if (playback) {
		_output->set_public_port_latency_from_connections ();
		if (_delayline && with_latcomp) {
			value += _delayline->delay ();
		}
		_input->set_public_port_latencies (value, true);
	} else {
		_input->set_public_port_latency_from_connections ();
		if (_delayline && with_latcomp) {
			value += _delayline->delay ();
		}
		_output->set_public_port_latencies (value, false);
	}

}

/** Put the invisible processors in the right place in _processors.
 *  Must be called with a writer lock on _processor_lock held.
 */
#ifdef __clang__
__attribute__((annotate("realtime")))
#endif
void
Route::setup_invisible_processors ()
{
#ifndef NDEBUG
	Glib::Threads::RWLock::WriterLock lm (_processor_lock, Glib::Threads::TRY_LOCK);
	assert (!lm.locked ());
#endif

	if (!_main_outs) {
		/* too early to be doing this stuff */
		return;
	}

	/* we'll build this new list here and then use it
	 *
	 * TODO put the ProcessorList is on the stack for RT-safety.
	 */

	ProcessorList new_processors;
	ProcessorList foldback_sends;

	/* find visible processors */

	for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
		std::shared_ptr<Send> auxsnd = std::dynamic_pointer_cast<Send> (*i);

		if (std::dynamic_pointer_cast<SurroundSend> (*i)) {
			continue;
		}

#ifdef HAVE_BEATBOX
		/* XXX temporary hack while we decide on visibility */
		if (std::dynamic_pointer_cast<BeatBox> (*i)) {
			continue;
		}
#endif
		if ((*i)->display_to_user ()) {
			new_processors.push_back (*i);
		}
		else if (auxsnd && auxsnd->is_foldback ()) {
			foldback_sends.push_back (*i);
		}
	}

	/* find the amp */

	ProcessorList::iterator amp = find (new_processors.begin(), new_processors.end(), _amp);

	if (amp == new_processors.end ()) {
		error << string_compose (_("Amp/Fader on track/bus '%1' was missing."), name()) << endmsg;
		new_processors.push_front (_amp);
		amp = find (new_processors.begin(), new_processors.end(), _amp);
	}


	/* Pre-fader METER */

	if (_meter && _meter_point == MeterPreFader) {
		/* add meter just before the fader */
		assert (!_meter->display_to_user ());
		new_processors.insert (amp, _meter);
	}

	/* SURROUND SEND */
	if (_surround_send) {
		new_processors.push_back (_surround_send);
	}

	/* MAIN OUTS */

	assert (_main_outs);
	assert (!_main_outs->display_to_user ());
	new_processors.push_back (_main_outs);

	/* iterator for the main outs */
	ProcessorList::iterator main = new_processors.end();
	--main;

	/* OUTPUT METERING */

	if (_meter && (_meter_point == MeterOutput || _meter_point == MeterPostFader)) {
		assert (!_meter->display_to_user ());
		/* add the processor just before or just after the main outs */
		ProcessorList::iterator meter_point = main;
		if (_meter_point == MeterOutput) {
			++meter_point;
		}
		new_processors.insert (meter_point, _meter);
	}

	/* Foldback Sends */

	for (ProcessorList::iterator i = foldback_sends.begin(); i != foldback_sends.end(); ++i) {
		if ((*i)->get_pre_fader ()) {
			new_processors.insert (amp, (*i));
		} else {
			new_processors.insert (main, (*i));
		}
	}

	/* MONITOR SEND */

	if (_monitor_send && !is_monitor ()) {
		ProcessorList::iterator after_amp = amp;
		++after_amp;

		assert (!_monitor_send->display_to_user ());
		switch (Config->get_listen_position ()) {
		case PreFaderListen:
			switch (Config->get_pfl_position ()) {
			case PFLFromBeforeProcessors:
				new_processors.push_front (_monitor_send);
				break;
			case PFLFromAfterProcessors:
				new_processors.insert (amp, _monitor_send);
				break;
			}
			_monitor_send->set_can_pan (false);
			break;
		case AfterFaderListen:
			switch (Config->get_afl_position ()) {
			case AFLFromBeforeProcessors:
				new_processors.insert (after_amp, _monitor_send);
				break;
			case AFLFromAfterProcessors:
				new_processors.insert (main, _monitor_send);
				break;
			}
			_monitor_send->set_can_pan (true);
			break;
		}
	}

	/* MONITOR CONTROL */

	if (_monitor_control && is_monitor ()) {
		assert (!_monitor_control->display_to_user ());
		new_processors.insert (amp, _monitor_control);
	}

	/* TRIM CONTROL */

	ProcessorList::iterator trim = new_processors.end();

	if (_trim->active()) {
		assert (!_trim->display_to_user ());
		new_processors.push_front (_trim);
		trim = new_processors.begin();
	}

#ifdef HAVE_BEATBOX
	/* BEATBOX (for MIDI) */

	if (_beatbox) {
		ProcessorList::iterator insert_pos = trim;
		++insert_pos;
		new_processors.insert (insert_pos, _beatbox);
	}
#endif
	/* INTERNAL RETURN */

	/* doing this here means that any monitor control will come after
	   the return and trim.
	*/

	if (_intreturn) {
		assert (!_intreturn->display_to_user ());
		new_processors.push_front (_intreturn);
	}

	/* SURROUND RETURN */
	if (_surround_return) {
		assert (_surround_return && is_surround_master ());
		new_processors.push_front (_monitor_control);

		assert (!_surround_return->display_to_user ());
		new_processors.push_front (_surround_return);
	}

	/* DISK READER & WRITER (for Track objects) */

	if (_disk_reader || _disk_writer) {
		switch (_disk_io_point) {
		case DiskIOPreFader:
			if (trim != new_processors.end()) {
				/* insert BEFORE TRIM */
				if (_disk_writer) {
					new_processors.insert (trim, _disk_writer);
				}
				if (_disk_reader) {
					new_processors.insert (trim, _disk_reader);
				}
			} else {
				if (_disk_writer) {
					new_processors.push_front (_disk_writer);
				}
				if (_disk_reader) {
					new_processors.push_front (_disk_reader);
				}
			}
			break;
		case DiskIOPostFader:
			/* insert BEFORE main outs */
			if (_disk_writer) {
				new_processors.insert (main, _disk_writer);
			}
			if (_disk_reader) {
				new_processors.insert (main, _disk_reader);
			}
			break;
		case DiskIOCustom:
			/* reader and writer are visible under this condition, so they
			 * are not invisible and thus not handled here.
			 */
			break;
		}
	}

	/* ensure disk-writer is before disk-reader */
	if (_disk_reader && _disk_writer) {
		ProcessorList::iterator reader_pos = find (new_processors.begin(), new_processors.end(), _disk_reader);
		ProcessorList::iterator writer_pos = find (new_processors.begin(), new_processors.end(), _disk_writer);
		assert (reader_pos != new_processors.end ());
		assert (writer_pos != new_processors.end ());
		if (std::distance (new_processors.begin(), reader_pos) < std::distance (new_processors.begin(), writer_pos)) {
			new_processors.erase (reader_pos);
			assert (writer_pos == find (new_processors.begin(), new_processors.end(), _disk_writer));
			reader_pos = new_processors.insert (++writer_pos, _disk_reader);
		}
	}

	/* Polarity Invert */
	if (_polarity->active ()) {
		ProcessorList::iterator reader_pos = find (new_processors.begin(), new_processors.end(), _disk_reader);
		ProcessorList::iterator polarity_pos;
		if (reader_pos != new_processors.end()) {
			/* insert after disk-reader */
			polarity_pos = new_processors.insert (++reader_pos, _polarity);
		} else {
			ProcessorList::iterator return_pos = find (new_processors.begin(), new_processors.end(), _intreturn);
			/* insert after return */
			if (return_pos != new_processors.end()) {
				polarity_pos = new_processors.insert (++return_pos, _polarity);
			} else {
				new_processors.push_front (_polarity);
				polarity_pos = new_processors.begin();
			}
		}

		if (_triggerbox && (_disk_io_point != DiskIOCustom)) {
			/* BEFORE polarity */
			new_processors.insert (polarity_pos, _triggerbox);
		}
	}

	/* EXPORT PROCESSOR */
	if (_capturing_processor) {
		assert (!_capturing_processor->display_to_user ());
		ProcessorList::iterator capture_pos;
		if (_triggerbox && (capture_pos = find (new_processors.begin(), new_processors.end(), _triggerbox)) != new_processors.end ()) {
			/* insert after triggerbox (which is just after disk-reader) */
			new_processors.insert (++capture_pos, _capturing_processor);
		} else if ((capture_pos = find (new_processors.begin(), new_processors.end(), _disk_reader)) != new_processors.end ()) {
			/* insert after disk-reader */
			new_processors.insert (++capture_pos, _capturing_processor);
		} else if ((capture_pos = find (new_processors.begin(), new_processors.end(), _intreturn)) != new_processors.end ()) {
			/* insert after return (busses) */
				new_processors.insert (++capture_pos, _capturing_processor);
		} else {
			new_processors.push_front (_capturing_processor);
		}
	}

	/* Input meter */
	if (_meter && _meter_point == MeterInput) {
		/* add meter just before the disk-writer (if any)
		 * otherwise at the top, but after the latency delayline
		 * (perhaps it should also be after intreturn on busses ??)
		 */
		assert (!_meter->display_to_user ());
		ProcessorList::iterator writer_pos = find (new_processors.begin(), new_processors.end(), _disk_writer);
		if (writer_pos != new_processors.end()) {
			/* insert before disk-writer */
			new_processors.insert (writer_pos, _meter);
		} else {
			ProcessorList::iterator return_pos = find (new_processors.begin(), new_processors.end(), _intreturn);
			/* insert after return */
			if (return_pos != new_processors.end()) {
				new_processors.insert (++return_pos, _meter);
			} else {
				new_processors.push_front (_meter);
			}
		}
	}

	if (!is_singleton () && !is_auditioner()) {
		ProcessorList::iterator reader_pos = find (new_processors.begin(), new_processors.end(), _disk_reader);
		if (reader_pos != new_processors.end()) {
			/* insert before disk-reader */
			new_processors.insert (reader_pos, _delayline);
		} else {
			new_processors.push_front (_delayline);
		}
	}

	_processors = new_processors;

	for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
		if (!(*i)->display_to_user () && !(*i)->enabled () && (*i) != _monitor_send && (*i) != _surround_send) {
			(*i)->enable (true);
		}
	}

	DEBUG_TRACE (DEBUG::Processors, string_compose ("%1: setup_invisible_processors\n", _name));
	for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
		DEBUG_TRACE (DEBUG::Processors, string_compose ("\t%1\n", (*i)->display_name ()));
	}
}

void
Route::unpan ()
{
	Glib::Threads::Mutex::Lock lm (AudioEngine::instance()->process_lock ());
	Glib::Threads::RWLock::ReaderLock lp (_processor_lock);

	_pannable.reset ();

	for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
		std::shared_ptr<Delivery> d = std::dynamic_pointer_cast<Delivery>(*i);
		if (d) {
			d->unpan ();
		}
	}
}

/** If the meter point is `Custom', make a note of where the meter is.
 *  This is so that if the meter point is subsequently set to something else,
 *  and then back to custom, we can put the meter back where it was last time
 *  custom was enabled.
 *
 *  Must be called with the _processor_lock held.
 */
void
Route::maybe_note_meter_position ()
{
	if (_meter_point != MeterCustom) {
		return;
	}

	_custom_meter_position_noted = true;
	/* custom meter points range from after trim to before panner/main_outs
	 * this is a limitation by the current processor UI
	 */
	bool seen_trim = false;
	_processor_after_last_custom_meter.reset();
	for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
		if ((*i) == _trim) {
			seen_trim = true;
		}
		if ((*i) == _main_outs) {
			_processor_after_last_custom_meter = *i;
			break;
		}
		if (std::dynamic_pointer_cast<PeakMeter> (*i)) {
			if (!seen_trim) {
				_processor_after_last_custom_meter = _trim;
			} else {
				ProcessorList::iterator j = i;
				++j;
				assert(j != _processors.end ()); // main_outs should be before
				_processor_after_last_custom_meter = *j;
			}
			break;
		}
	}
	assert(_processor_after_last_custom_meter.lock());
}

std::shared_ptr<Processor>
Route::processor_by_id (PBD::ID id) const
{
	Glib::Threads::RWLock::ReaderLock lm (_processor_lock);
	for (ProcessorList::const_iterator i = _processors.begin(); i != _processors.end(); ++i) {
		if ((*i)->id() == id) {
			return *i;
		}
	}

	return std::shared_ptr<Processor> ();
}

bool
Route::can_freeze_processor (std::shared_ptr<Processor> p, bool allow_routing) const
{
	/* ignore inactive processors and obviously ignore the main
	 * outs since everything has them and we don't care.
	 */
	if (!p->active()) {
		return true;
	}

	if (p != _main_outs && p->does_routing()) {
		return allow_routing;
	}

	if (std::dynamic_pointer_cast<PortInsert>(p)) {
		return false;
	}

	std::shared_ptr<PluginInsert> pi = std::dynamic_pointer_cast<PluginInsert>(p);
	if (pi && pi->has_sidechain () && pi->sidechain_input () && pi->sidechain_input ()->connected()) {
		return false;
	}

	return true;
}

bool
Route::has_external_redirects () const
{
	Glib::Threads::RWLock::ReaderLock lm (_processor_lock);
	for (ProcessorList::const_iterator i = _processors.begin(); i != _processors.end(); ++i) {
		if (!can_freeze_processor (*i)) {
			return true;
		}
	}
	return false;
}

std::shared_ptr<Processor>
Route::the_instrument () const
{
	Glib::Threads::RWLock::ReaderLock lm (_processor_lock);
	return the_instrument_unlocked ();
}

std::shared_ptr<Processor>
Route::the_instrument_unlocked () const
{
	for (ProcessorList::const_iterator i = _processors.begin(); i != _processors.end(); ++i) {
		std::shared_ptr<PluginInsert> pi = std::dynamic_pointer_cast<PluginInsert>(*i);
		if (pi && pi->plugin ()->get_info ()->is_instrument ()) {
			return (*i);
		}
	}
	return std::shared_ptr<Processor>();
}

bool
Route::is_track()
{
	return dynamic_cast<Track*>(this) != 0;
}

void
Route::non_realtime_locate (samplepos_t pos)
{
	Automatable::non_realtime_locate (pos);

	if (_pannable) {
		_pannable->non_realtime_locate (pos);
	}

#if 0 // XXX mayhaps clear delayline here (and at stop?)
	if (_delayline) {
		_delayline->flush ();
	}
#endif

	{
		//Glib::Threads::Mutex::Lock lx (AudioEngine::instance()->process_lock ());
		Glib::Threads::RWLock::ReaderLock lm (_processor_lock);

		for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
			(*i)->non_realtime_locate (pos);
		}
	}
}

void
Route::fill_buffers_with_input (BufferSet& bufs, std::shared_ptr<IO> io, pframes_t nframes)
{
	size_t n_buffers;
	size_t i;

	/* MIDI
	 *
	 * We don't currently mix MIDI input together, so we don't need the
	 * complex logic of the audio case.
	 */

	n_buffers = bufs.count().n_midi ();

	for (i = 0; i < n_buffers; ++i) {

		std::shared_ptr<MidiPort> source_port = io->midi (i);
		MidiBuffer& buf (bufs.get_midi (i));

		if (source_port) {
			buf.copy (source_port->get_midi_buffer(nframes));
		} else {
			buf.silence (nframes);
		}
	}

	/* AUDIO */

	n_buffers = bufs.count().n_audio();

	size_t n_ports = io->n_ports().n_audio();
	float scaling = 1.0f;

	if (n_ports > n_buffers) {
		scaling = ((float) n_buffers) / n_ports;
	}

	for (i = 0; i < n_ports; ++i) {

		/* if there are more ports than buffers, map them onto buffers
		 * in a round-robin fashion
		 */

		std::shared_ptr<AudioPort> source_port = io->audio (i);
		AudioBuffer& buf (bufs.get_audio (i%n_buffers));

		if (i < n_buffers) {

			/* first time through just copy a channel into
			   the output buffer.
			*/

			buf.read_from (source_port->get_audio_buffer (nframes), nframes);

			if (scaling != 1.0f) {
				buf.apply_gain (scaling, nframes);
			}

		} else {

			/* on subsequent times around, merge data from
			 * the port with what is already there
			 */

			if (scaling != 1.0f) {
				buf.accumulate_with_gain_from (source_port->get_audio_buffer (nframes), nframes, 0, scaling);
			} else {
				buf.accumulate_from (source_port->get_audio_buffer (nframes), nframes);
			}
		}
	}

	/* silence any remaining buffers */

	for (; i < n_buffers; ++i) {
		AudioBuffer& buf (bufs.get_audio (i));
		buf.silence (nframes);
	}

	/* establish the initial setup of the buffer set, reflecting what was
	   copied into it. unless, of course, we are the auditioner, in which
	   case nothing was fed into it from the inputs at all.
	*/

	if (!is_auditioner()) {
		bufs.set_count (io->n_ports());
	}
}

std::shared_ptr<AutomationControl>
Route::pan_azimuth_control() const
{
	if (!_pannable || !panner()) {
		return std::shared_ptr<AutomationControl>();
	}
	return _pannable->pan_azimuth_control;
}

std::shared_ptr<AutomationControl>
Route::pan_elevation_control() const
{
	if (Profile->get_mixbus() || !_pannable || !panner()) {
		return std::shared_ptr<AutomationControl>();
	}

	set<Evoral::Parameter> c = pannable()->what_can_be_automated ();

	if (c.find (PanElevationAutomation) != c.end()) {
		return _pannable->pan_elevation_control;
	} else {
		return std::shared_ptr<AutomationControl>();
	}
}
std::shared_ptr<AutomationControl>
Route::pan_width_control() const
{
	if (Profile->get_mixbus() || !_pannable || !panner()) {
		return std::shared_ptr<AutomationControl>();
	}

	set<Evoral::Parameter> c = pannable()->what_can_be_automated ();

	if (c.find (PanWidthAutomation) != c.end()) {
		return _pannable->pan_width_control;
	} else {
		return std::shared_ptr<AutomationControl>();
	}
}
std::shared_ptr<AutomationControl>
Route::pan_frontback_control() const
{
	if (Profile->get_mixbus() || !_pannable || !panner()) {
		return std::shared_ptr<AutomationControl>();
	}

	set<Evoral::Parameter> c = pannable()->what_can_be_automated ();

	if (c.find (PanFrontBackAutomation) != c.end()) {
		return _pannable->pan_frontback_control;
	} else {
		return std::shared_ptr<AutomationControl>();
	}
}
std::shared_ptr<AutomationControl>
Route::pan_lfe_control() const
{
	if (Profile->get_mixbus() || !_pannable || !panner()) {
		return std::shared_ptr<AutomationControl>();
	}

	set<Evoral::Parameter> c = pannable()->what_can_be_automated ();

	if (c.find (PanLFEAutomation) != c.end()) {
		return _pannable->pan_lfe_control;
	} else {
		return std::shared_ptr<AutomationControl>();
	}
}

void
Route::add_well_known_ctrl (WellKnownCtrl which)
{
	_well_known_map[which].push_back (std::weak_ptr<ARDOUR::AutomationControl> ());
}

void
Route::add_well_known_ctrl (WellKnownCtrl which, std::shared_ptr<PluginInsert> pi, int param)
{
	_well_known_map[which].push_back (std::dynamic_pointer_cast<ARDOUR::AutomationControl> (pi->control (Evoral::Parameter (ARDOUR::PluginAutomation, 0, param))));
}

std::shared_ptr<AutomationControl>
Route::mapped_control (enum WellKnownCtrl which, uint32_t band) const
{
	auto it = _well_known_map.find (which);
	if (it == _well_known_map.end () || it->second.size () <= band) {
		return std::shared_ptr<AutomationControl> ();
	}
	return it->second[band].lock();
}

std::shared_ptr<ReadOnlyControl>
Route::mapped_output (enum WellKnownData which) const
{
	return std::shared_ptr<ReadOnlyControl>();
}

uint32_t
Route::eq_band_cnt () const
{
	/* Ardour has no well-known EQ object */
	return 0;
}

string
Route::eq_band_name (uint32_t band) const
{
	return string ();
}

std::shared_ptr<AutomationControl>
Route::send_pan_azimuth_controllable (uint32_t n) const
{
	return std::shared_ptr<AutomationControl>();
}

std::shared_ptr<AutomationControl>
Route::send_level_controllable (uint32_t n, bool locked) const
{
	if (locked) {
		/* calling thread has a WriterLock (_processor_lock)
		 * we cannot call nth_send()
		 */
		return std::shared_ptr<AutomationControl>();
	}

	std::shared_ptr<Send> s = std::dynamic_pointer_cast<Send>(nth_send (n));
	if (s) {
		return s->gain_control ();
	}
	return std::shared_ptr<AutomationControl>();
}

std::shared_ptr<AutomationControl>
Route::send_enable_controllable (uint32_t n) const
{
	/* although Ardour sends have enable/disable as part of the Processor
	 * API, it is not exposed as a controllable.
	 *
	 * XXX: we should fix this (make it click-free, automatable enable-control)
	 */
	return std::shared_ptr<AutomationControl>();
}

std::shared_ptr<AutomationControl>
Route::send_pan_azimuth_enable_controllable (uint32_t n) const
{
	return std::shared_ptr<AutomationControl>();
}

string
Route::send_name (uint32_t n) const
{
	std::shared_ptr<Processor> p = nth_send (n);
	if (p) {
		return p->name();
	}
	return string();
}

std::shared_ptr<AutomationControl>
Route::master_send_enable_controllable () const
{
	return std::shared_ptr<AutomationControl>();
}

bool
Route::slaved () const
{
	if (!_gain_control) {
		return false;
	}
	/* just test one particular control, not all of them */
	return _gain_control->slaved ();
}

bool
Route::slaved_to (std::shared_ptr<VCA> vca) const
{
	if (!vca || !_gain_control) {
		return false;
	}

	/* just test one particular control, not all of them */

	return _gain_control->slaved_to (vca->gain_control());
}

bool
Route::muted_by_others_soloing () const
{
	if (!can_be_muted_by_others ()) {
		return false;
	}

	return  _session.soloing() && !_solo_control->soloed() && !_solo_isolate_control->solo_isolated();
}

void
Route::clear_all_solo_state ()
{
	_solo_control->clear_all_solo_state ();
}

std::shared_ptr<AutomationControl>
Route::automation_control_recurse (PBD::ID const & id) const
{
	std::shared_ptr<AutomationControl> ac = Automatable::automation_control (id);

	if (ac) {
		return ac;
	}

	if  (_pannable) {
		if ((ac = _pannable->automation_control (id))) {
			return ac;
		}
	}

	Glib::Threads::RWLock::ReaderLock lm (_processor_lock);

	for (ProcessorList::const_iterator i = _processors.begin(); i != _processors.end(); ++i) {
		if ((ac = (*i)->automation_control (id))) {
			return ac;
		}
	}

	return std::shared_ptr<AutomationControl> ();
}

void
Route::automatables (PBD::ControllableSet& s) const
{
	Automatable::automatables (s);
	if  (_pannable) {
		_pannable->automatables (s);
	}
	Glib::Threads::RWLock::ReaderLock lm (_processor_lock);
	for (auto const& i : _processors) {
		i->automatables (s);
	}
}

SlavableAutomationControlList
Route::slavables () const
{
	SlavableAutomationControlList rv;
	rv.push_back (_gain_control);
	rv.push_back (_mute_control);
	rv.push_back (_solo_control);
	return rv;
}

void
Route::set_meter_type (MeterType t)
{
	_meter->set_meter_type (t);
	_session.set_dirty ();
}

MeterType
Route::meter_type () const
{
	return _meter->meter_type ();
}

void
Route::set_disk_io_point (DiskIOPoint diop)
{
	bool display = false;

	switch (diop) {
	case DiskIOCustom:
		display = true;
		break;
	default:
		display = false;
	}

	if (_disk_writer) {
		_disk_writer->set_display_to_user (display);
	}

	if (_disk_reader) {
		_disk_reader->set_display_to_user (display);
	}

	if (_triggerbox) {
		_triggerbox->set_display_to_user (display);
	}

	const bool changed = (diop != _disk_io_point);

	_disk_io_point = diop;

	if (_initial_io_setup) {
		return;
	}

	if (changed) {
		Glib::Threads::Mutex::Lock lx (AudioEngine::instance()->process_lock ());
		configure_processors (0);
	}

	if (changed) {
		processors_changed (RouteProcessorChange ()); /* EMIT SIGNAL */
	}
}

void
Route::set_loop (Location* l)
{
	_loop_location = l;
	Glib::Threads::RWLock::ReaderLock lm (_processor_lock);
	for (ProcessorList::const_iterator i = _processors.begin(); i != _processors.end(); ++i) {
		(*i)->set_loop (l);
	}
}

static inline MonitorState
operator| (const MonitorState& a, const MonitorState& b) {
	return static_cast<MonitorState> (static_cast <int>(a) | static_cast<int> (b));
}

static inline MonitorState
operator& (const MonitorState& a, const MonitorState& b) {
	return static_cast<MonitorState> (static_cast <int>(a) & static_cast<int> (b));
}

MonitorState
Route::monitoring_state () const
{
	if (!_disk_reader || !_monitoring_control) {
		return MonitoringInput;
	}

	/* Explicit requests */
	MonitorChoice m (_monitoring_control->monitoring_choice());

	if (m != MonitorAuto) {

		MonitorState ms = MonitoringSilence;

		if (m & MonitorInput) {
			ms = MonitorState (ms | MonitoringInput);
		}

		if (m & MonitorDisk) {
			ms = MonitorState (ms | MonitoringDisk);
		}

		return ms;
	}

	/* When TriggerBox is not empty, do not implicitly monitor Disk */
	bool const tod = _session.config.get_triggerbox_overrides_disk_monitoring ();
	MonitorState auto_monitor_disk = !tod || (_triggerbox && _triggerbox->empty ()) ? MonitoringDisk : MonitoringSilence;
	MonitorState auto_monitor_mask = !tod || (_triggerbox && _triggerbox->empty ()) ? MonitoringCue : MonitoringInput;

	switch (_session.config.get_session_monitoring ()) {
		case MonitorDisk:
			return auto_monitor_disk;
			break;
		case MonitorInput:
			return MonitoringInput;
			break;
		case MonitorCue:
			return MonitoringCue;
		default:
			/* MonitorAuto */
			break;
	}

	/* This is an implementation of the truth table at https://manual.ardour.org/appendix/monitor-modes/
	 * I don't think it's ever going to be too pretty too look at.
	 */

	bool session_rec       = _session.get_record_enabled ();
	bool const roll        = _session.transport_state_rolling ();
	bool const auto_input  = _session.config.get_auto_input ();
	bool const clip_rec    = _triggerbox && _triggerbox->record_enabled() == Recording && !session_rec;
	bool const track_rec   = _disk_writer->record_enabled ();

	bool const auto_input_does_talkback = Config->get_auto_input_does_talkback ();

	/* I suspect that just use actively_recording() is good enough all the
	 * time, but just to keep the semantics the same as they were before
	 * sept 26th 2012, we differentiate between the cases where punch is
	 * enabled and those where it is not.
	 *
	 * rg: sept 30 2017: Above is not the case: punch-in/out location is
	 * global session playhead position.
	 * When this method is called from process_output_buffers() we need
	 * to use delay-compensated route's process-position.
	 *
	 * NB. Disk reader/writer may also be offset by a same amount of time.
	 *
	 * Also keep in mind that _session.transport_rolling() is false during
	 * pre-roll but the disk already produces output.
	 *
	 * TODO: FIXME
	 */

	if ((_session.config.get_punch_in() || _session.config.get_punch_out()) && 0 != _session.locations()->auto_punch_location ()) {
		session_rec = _session.actively_recording ();
	}

	if (track_rec || clip_rec) {

		/* either record to the timeline or into a clip */
		assert (track_rec != clip_rec);

		if (clip_rec) {
			/* actively recording into a slot */
			return MonitoringInput;
		}

		if (!roll) {
			/*  9, 10, 25, 26 -> MonitoringInput; 41, 42, 57, 58 -> HW passthrough
			 * 13, 14, 29, 30 -> MonitoringInput; 46, 47, 61, 62 -> HW passthrough */
			return get_input_monitoring_state (true, false) & auto_monitor_mask;
		}

		if (!session_rec) {
			if (auto_input) {
				/* 11, 27, 43, 59 -> MonitoringDisk */
				return auto_monitor_disk | get_input_monitoring_state (false, false);
			} else {
				/* 12, 28 -> MonitoringInput;  44, 60 -> HW passthru */
				return get_input_monitoring_state (true, false) & auto_monitor_mask;
			}
		}

		/* actively recording
		 * 15, 16, 31, 32 -> MonitoringInput, 47, 48, 63, 64 -> HW passthrough */
		const samplecnt_t prtl = _session.preroll_record_trim_len ();
		if (session_rec && roll && prtl > 0 && _disk_writer->get_captured_samples () < prtl) {
			/* CUE monitor during pre-roll */
			return auto_monitor_disk | (get_input_monitoring_state (true, false) & auto_monitor_mask);
		}
		return get_input_monitoring_state (true, false) & auto_monitor_mask;

	} else {

		if (auto_input_does_talkback) {
			/* 1 - 8 ; 33 - 40 */
			if (!roll && auto_input) {
				/* 1, 5 -> MonitoringInput */
				return get_input_monitoring_state (false, true) & auto_monitor_mask;
			} else {
				return auto_monitor_disk | get_input_monitoring_state (false, false);
			}

		} else {
			/* tape-machine-mode
			 * 17 - 24; 49 - 56 -> MonitoringDisk */
			return auto_monitor_disk | get_input_monitoring_state (false, false);
		}
	}

	abort(); /* NOTREACHED */
	return MonitoringSilence;
}

void
Route::tempo_map_changed ()
{
	if (_triggerbox) {
		_triggerbox->tempo_map_changed ();
	}
}

void
Route::enable_surround_send ()
{
	if (is_main_bus ()) {
		/* no surround sends for you */
		return;
	}

	/* Caller must hold process lock */
	assert (!AudioEngine::instance()->process_lock().trylock());

	/* make sure we have one */
	if (!_surround_send) {
		_surround_send.reset (new SurroundSend (_session, _mute_master));
		_surround_send->set_owner (this);
		_surround_send->activate ();
	}

	Glib::Threads::RWLock::WriterLock lm (_processor_lock);
	configure_processors_unlocked (0, &lm);
	/* We cannot emit `processors_changed` while holing the `process lock`
	 * This can lead to deadlock in ARDOUR::Session::route_processors_changed
	 */
	_pending_surround_send.store (1);
}

void
Route::remove_surround_send ()
{
	/* Caller must hold process lock */
	assert (!AudioEngine::instance()->process_lock().trylock());

	if (!_surround_send) {
		return;
	}

	_surround_send.reset ();

	Glib::Threads::RWLock::WriterLock lm (_processor_lock);
	configure_processors_unlocked (0, &lm);
	/* We cannot emit `processors_changed` while holing the `process lock`
	 * This can lead to deadlock in ARDOUR::Session::route_processors_changed
	 */
	_pending_surround_send.store (1);
}
