/*
 * Copyright (C) 2026 by the Widelands Development Team
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

#ifndef WL_UI_GAME_BUILDING_INFO_TABS_H
#define WL_UI_GAME_BUILDING_INFO_TABS_H

#include <vector>

#include "logic/map_objects/tribes/building.h"
#include "logic/map_objects/tribes/productionsite.h"
#include "logic/widelands.h"
#include "ui/basic/box.h"
#include "ui/basic/icon.h"
#include "ui/basic/multilinetextarea.h"
#include "ui/basic/textarea.h"

class InteractiveBase;

/// Width of the content of the "Production" and "Upgrade" tabs.
constexpr int kBuildingInfoTabWidth = 400;

/**
 * Tab content listing everything a production site produces, one row per
 * ware or worker type, together with how its stock compares to the economy target, followed by
 * the building's latest production message (the text shown when hovering over it on the map).
 *
 * Every row is a separate widget so that a per-output control (e.g. off /
 * balanced by economy / always on) can be added to it later.
 */
class BuildingOutputsDisplay : public UI::Box {
public:
	BuildingOutputsDisplay(UI::Panel* parent,
	                       InteractiveBase& ibase,
	                       Widelands::ProductionSite& site);

	/// Whether the site has any outputs at all, i.e. whether the tab is worth showing.
	static bool has_outputs(const Widelands::ProductionSiteDescr& descr);

	void think() override;

private:
	struct Row {
		Widelands::WareWorker type;
		Widelands::DescriptionIndex index;
		UI::Textarea* stock;  ///< "stock/target", coloured like the stock menu
		UI::Icon* status;     ///< Stopped / produce indefinitely indicator
	};

	void add_section(const std::string& title);
	void add_row(Widelands::WareWorker type, Widelands::DescriptionIndex index);
	void update_rows();

	InteractiveBase& ibase_;
	Widelands::OPtr<Widelands::ProductionSite> site_;
	std::vector<Row> rows_;
	UI::MultilineTextarea* footer_{nullptr};
	uint32_t last_update_{0U};
};

/**
 * Builds the richtext for the "Upgrade" tab: the upgrade chain around 'current' (previous level,
 * this building, next level) and what changes in costs, production and requirements when it is
 * enhanced. 'current' must be enhanceable, see building_can_be_enhanced().
 */
std::string building_upgrade_richtext(const Widelands::Player& player,
                                      const Widelands::BuildingDescr& current);

/// Whether 'descr' has a next level, i.e. whether the "Upgrade" tab is worth showing.
bool building_can_be_enhanced(const Widelands::TribeDescr& tribe,
                              const Widelands::BuildingDescr& descr);

#endif  // end of include guard: WL_UI_GAME_BUILDING_INFO_TABS_H
