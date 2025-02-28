/*
 * Copyright (C) 2006-2025 by the Widelands Development Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses/>.
 *
 */

#ifndef WL_EDITOR_UI_MENUS_CATEGORIZED_ITEM_SELECTION_MENU_H
#define WL_EDITOR_UI_MENUS_CATEGORIZED_ITEM_SELECTION_MENU_H

#include <memory>

#include "base/i18n.h"
#include "base/string.h"
#include "editor/editor_category.h"
#include "logic/map_objects/description_maintainer.h"
#include "ui_basic/box.h"
#include "ui_basic/checkbox.h"
#include "ui_basic/multilinetextarea.h"
#include "ui_basic/panel.h"
#include "ui_basic/tabpanel.h"

template <typename DescriptionType, typename ToolType>
class CategorizedItemSelectionMenu : public UI::Box {
public:
	// Creates a box with a tab panel for each category in 'categories' and
	// populates them with the 'descriptions' ordered by the category by calling
	// 'create_checkbox' for each of the descriptions. Calls
	// 'select_correct_tool' whenever a selection has been made, also keeps a
	// text label updated and updates the 'tool' with current selections. Does
	// not take ownership.
	CategorizedItemSelectionMenu(
	   UI::Panel* parent,
	   const std::string& name,
	   const std::vector<std::unique_ptr<EditorCategory>>& categories,
	   const Widelands::DescriptionMaintainer<DescriptionType>& descriptions,
	   std::function<UI::Checkbox*(UI::Panel* parent, const DescriptionType& descr)> create_checkbox,
	   std::function<void()> select_correct_tool,
	   ToolType* tool,
	   std::map<int32_t, std::string> descname_overrides = {});

	// Updates selection to match the tool settings
	void update_selection();

	UI::TabPanel& tabs() {
		return tab_panel_;
	}

	// Update the label with the currently selected object names.
	void update_label();
	void set_descname_override(int32_t index, std::string name) {
		descname_overrides_.emplace(index, name);
	}

private:
	// Called when an item was selected.
	void selected(int32_t, bool);

	const Widelands::DescriptionMaintainer<DescriptionType>& descriptions_;
	std::map<int32_t, std::string> descname_overrides_;
	std::function<void()> select_correct_tool_;
	bool protect_against_recursive_select_{false};
	UI::TabPanel tab_panel_;
	UI::MultilineTextarea current_selection_names_;
	std::map<int, UI::Checkbox*> checkboxes_;
	ToolType* const tool_;  // not owned
};

template <typename DescriptionType, typename ToolType>
CategorizedItemSelectionMenu<DescriptionType, ToolType>::CategorizedItemSelectionMenu(
   UI::Panel* parent,
   const std::string& name,
   const std::vector<std::unique_ptr<EditorCategory>>& categories,
   const Widelands::DescriptionMaintainer<DescriptionType>& descriptions,
   const std::function<UI::Checkbox*(UI::Panel* parent, const DescriptionType& descr)>
      create_checkbox,
   const std::function<void()> select_correct_tool,
   ToolType* const tool,
   std::map<int32_t, std::string> descname_overrides)
   : UI::Box(parent, UI::PanelStyle::kWui, name, 0, 0, UI::Box::Vertical),
     descriptions_(descriptions),
     descname_overrides_(descname_overrides),
     select_correct_tool_(select_correct_tool),

     tab_panel_(this, UI::TabPanelStyle::kWuiLight, "categories"),
     current_selection_names_(this,
                              "current_selection_names",
                              0,
                              0,
                              20,
                              20,
                              UI::PanelStyle::kWui,
                              "",
                              UI::Align::kCenter,
                              UI::MultilineTextarea::ScrollMode::kNoScrolling),
     tool_(tool) {
	add(&tab_panel_);

	for (const auto& category : categories) {
		UI::Box* vertical = new UI::Box(&tab_panel_, UI::PanelStyle::kWui,
		                                format("vbox_%s", category->name()), 0, 0, UI::Box::Vertical);
		const int kSpacing = 5;
		vertical->add_space(kSpacing);

		int nitems_handled = 0;
		UI::Box* horizontal = nullptr;
		for (const int i : category->items()) {
			if (nitems_handled % category->items_per_row() == 0) {
				horizontal =
				   new UI::Box(vertical, UI::PanelStyle::kWui,
				               format("hbox_%s_%d", category->name(), i), 0, 0, UI::Box::Horizontal);
				horizontal->add_space(kSpacing);

				vertical->add(horizontal);
				vertical->add_space(kSpacing);
			}
			assert(horizontal != nullptr);

			UI::Checkbox* cb = create_checkbox(horizontal, descriptions_.get(i));
			cb->set_state(tool_->is_enabled(i));
			cb->changedto.connect([this, i](bool b) { selected(i, b); });
			checkboxes_[i] = cb;
			horizontal->add(cb, UI::Box::Resizing::kAlign, UI::Align::kBottom);
			horizontal->add_space(kSpacing);
			++nitems_handled;
		}
		tab_panel_.add(category->name(), category->picture(), vertical, category->descname());
	}
	add(&current_selection_names_, UI::Box::Resizing::kFullSize);
	tab_panel_.sigclicked.connect([this]() { update_label(); });
	update_label();
}

template <typename DescriptionType, typename ToolType>
void CategorizedItemSelectionMenu<DescriptionType, ToolType>::selected(const int32_t n,
                                                                       const bool t) {
	if (protect_against_recursive_select_) {
		return;
	}

	//  TODO(unknown): This code is erroneous. It checks the current key state. What it
	//  needs is the key state at the time the mouse was clicked. See the
	//  usage comment for get_key_state.
	const bool multiselect = (SDL_GetModState() & KMOD_CTRL) != 0;
	if (!t && (!multiselect || tool_->get_nr_enabled() == 1)) {
		checkboxes_[n]->set_state(true);
	} else {
		if (!multiselect) {
			tool_->disable_all();
			//  disable all checkboxes
			protect_against_recursive_select_ = true;
			const int32_t size = checkboxes_.size();
			for (int32_t i = 0; i < size; ++i) {
				if (i != n) {
					checkboxes_[i]->set_state(false);
				}
			}
			protect_against_recursive_select_ = false;
		}

		tool_->enable(n, t);
		select_correct_tool_();
		update_label();
	}
}

template <typename DescriptionType, typename ToolType>
void CategorizedItemSelectionMenu<DescriptionType, ToolType>::update_label() {
	current_selection_names_.set_size(tab_panel_.get_inner_w(), 20);
	std::string buf;
	constexpr int max_string_size = 100;
	for (int index : tool_->get_enabled()) {
		if (!buf.empty()) {
			buf += " • ";
		}
		const auto it = descname_overrides_.find(index);
		buf += it != descname_overrides_.end() ? it->second : descriptions_.get(index).descname();
	}
	if (buf.size() > max_string_size) {
		/** TRANSLATORS: %s are the currently selected items in an editor tool */
		buf = format(_("Current: %s …"), buf);
	} else if (buf.empty()) {
		/** TRANSLATORS: Help text in an editor tool */
		buf = _("Click to select an item. Use the Ctrl key to select multiple items.");
	} else {
		/** TRANSLATORS: %s are the currently selected items in an editor tool */
		buf = format(_("Current: %s"), buf);
	}
	current_selection_names_.set_text(buf);
}

template <typename DescriptionType, typename ToolType>
void CategorizedItemSelectionMenu<DescriptionType, ToolType>::update_selection() {
	protect_against_recursive_select_ = true;

	const int32_t size = checkboxes_.size();
	for (int32_t i = 0; i < size; i++) {
		checkboxes_[i]->set_state(tool_->is_enabled(i));
	}

	update_label();

	protect_against_recursive_select_ = false;
}

#endif  // end of include guard: WL_EDITOR_UI_MENUS_CATEGORIZED_ITEM_SELECTION_MENU_H
