/*
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

#pragma once

#include <ytkmm/checkbutton.h>
#include <ytkmm/comboboxtext.h>
#include <ytkmm/entry.h>
#include <ytkmm/filechooserbutton.h>
#include <ytkmm/progressbar.h>

#include "pbd/file_archive.h"
#include "ardour/session.h"

#include "ardour_dialog.h"
#include "progress_reporter.h"

class SessionArchiveDialog : public ArdourDialog, public ProgressReporter
{
public:
	SessionArchiveDialog ();

	std::string target_folder () const;
	std::string name () const;
	ARDOUR::Session::ArchiveEncode encode_option () const;
	PBD::FileArchive::CompressionLevel compression_level () const;
	bool only_used_sources () const;

	void set_name (const std::string&);
	void set_target_folder (const std::string&);
	void set_encode_option (ARDOUR::Session::ArchiveEncode);
	void set_compression_level (PBD::FileArchive::CompressionLevel);
	void set_only_used_sources (bool);

protected:
	void on_response (int);

private:
	Gtk::FileChooserButton target_folder_selector;
	Gtk::Entry             name_entry;
	Gtk::Label             format_Label;
	Gtk::ComboBoxText      encode_selector;
	Gtk::ComboBoxText      compression_selector;
	Gtk::CheckButton       only_used_checkbox;

	Gtk::ProgressBar progress_bar;

	void name_entry_changed ();
	void update_progress_gui (float);
};

