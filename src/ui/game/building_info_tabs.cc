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

#include "ui/game/building_info_tabs.h"

#include <map>
#include <memory>

#include <SDL_timer.h>

#include "base/i18n.h"
#include "base/macros.h"
#include "base/string.h"
#include "economy/economy.h"
#include "graphic/style_manager.h"
#include "graphic/text_layout.h"
#include "logic/map_objects/tribes/tribe_descr.h"
#include "logic/map_objects/tribes/ware_descr.h"
#include "logic/map_objects/tribes/warehouse.h"
#include "logic/map_objects/tribes/worker_descr.h"
#include "ui/wui/interactive_base.h"

namespace {

constexpr int kRowIconSize = 32;
constexpr int kRowSpacing = 4;
constexpr int kStockWidth = 90;
constexpr int kStatusIconSize = 24;
constexpr const char* const kImgStopped = "images/ui_basic/stop.png";
constexpr const char* const kImgInfinite = "images/wui/menus/infinity.png";
constexpr uint32_t kUpdateIntervalMs = 1000;

const UI::BuildingStatisticsStyleInfo& colors() {
	return g_style_manager->building_statistics_style();
}

/// Textarea::set_style_override() keeps a pointer, so the styles need to outlive the widgets.
const UI::FontStyleInfo& colored_font(UI::FontStyle base, const RGBColor& color) {
	static std::map<std::pair<UI::FontStyle, std::string>, std::unique_ptr<UI::FontStyleInfo>> cache;
	std::unique_ptr<UI::FontStyleInfo>& entry = cache[std::make_pair(base, color.hex_value())];
	if (entry == nullptr) {
		entry = std::make_unique<UI::FontStyleInfo>(g_style_manager->font_style(base));
		entry->set_color(color);
	}
	return *entry;
}

/// Amount stored in the economy's warehouses, the same count the economy targets refer to.
Widelands::Quantity warehouse_stock(const Widelands::Economy& economy,
                                    Widelands::WareWorker type,
                                    Widelands::DescriptionIndex index) {
	Widelands::Quantity result = 0;
	for (const Widelands::Warehouse* wh : economy.warehouses()) {
		result +=
		   type == Widelands::wwWARE ? wh->get_wares().stock(index) : wh->get_workers().stock(index);
	}
	return result;
}

}  // namespace

/*
 * ========================================================================
 *                              Outputs tab
 * ========================================================================
 */

bool BuildingOutputsDisplay::has_outputs(const Widelands::ProductionSiteDescr& descr) {
	return !descr.output_ware_types().empty() || !descr.output_worker_types().empty();
}

BuildingOutputsDisplay::BuildingOutputsDisplay(UI::Panel* parent,
                                               InteractiveBase& ibase,
                                               Widelands::ProductionSite& site)
   : UI::Box(parent, UI::PanelStyle::kWui, "outputs_box", 0, 0, UI::Box::Vertical),
     ibase_(ibase),
     site_(&site) {
	set_inner_spacing(kRowSpacing);

	const Widelands::ProductionSiteDescr& descr = site.descr();
	const Widelands::TribeDescr& tribe = site.owner().tribe();

	// Keep the tribe's display order so that the list matches the stock menus.
	if (!descr.output_ware_types().empty()) {
		add_section(_("Wares"));
		for (const auto& column : tribe.wares_order()) {
			for (Widelands::DescriptionIndex di : column) {
				if (descr.output_ware_types().count(di) != 0u) {
					add_row(Widelands::wwWARE, di);
				}
			}
		}
	}
	if (!descr.output_worker_types().empty()) {
		add_section(_("Workers"));
		for (const auto& column : tribe.workers_order()) {
			for (Widelands::DescriptionIndex di : column) {
				if (descr.output_worker_types().count(di) != 0u) {
					add_row(Widelands::wwWORKER, di);
				}
			}
		}
	}

	footer_ = new UI::MultilineTextarea(this, "outputs_footer", 0, 0, kBuildingInfoTabWidth, 0,
	                                    UI::PanelStyle::kWui, "", UI::Align::kLeft,
	                                    UI::MultilineTextarea::ScrollMode::kNoScrolling);
	add_space(kRowSpacing);
	add(footer_, UI::Box::Resizing::kFullSize);
	add_space(kRowSpacing * 2);
	footer_->set_visible(false);  // Shown by update_rows() when there is something to report

	update_rows();
	set_thinks(true);
}

void BuildingOutputsDisplay::add_section(const std::string& title) {
	UI::Textarea* heading = new UI::Textarea(
	   this, UI::PanelStyle::kWui, "section_heading", UI::FontStyle::kWuiInfoPanelHeading, title);
	if (get_nritems() > 0) {
		add_space(kRowSpacing * 2);
	}
	add(heading, UI::Box::Resizing::kAlign);
}

void BuildingOutputsDisplay::add_row(Widelands::WareWorker type,
                                     Widelands::DescriptionIndex index) {
	Widelands::ProductionSite* site = site_.get(ibase_.egbase());
	assert(site != nullptr);
	const Widelands::TribeDescr& tribe = site->owner().tribe();
	const Widelands::MapObjectDescr* descr =
	   type == Widelands::wwWARE ?
	      static_cast<const Widelands::MapObjectDescr*>(tribe.get_ware_descr(index)) :
	      static_cast<const Widelands::MapObjectDescr*>(tribe.get_worker_descr(index));

	UI::Box* row = new UI::Box(
	   this, UI::PanelStyle::kWui, format("output_row_%u", index), 0, 0, UI::Box::Horizontal);

	UI::Icon* icon = new UI::Icon(
	   row, UI::PanelStyle::kWui, "icon", 0, 0, kRowIconSize, kRowIconSize, descr->icon());
	icon->set_frame(g_style_manager->ware_info_style(UI::WareInfoStyle::kNormal).icon_background());
	icon->set_tooltip(descr->descname());

	UI::Textarea* name = new UI::Textarea(
	   row, UI::PanelStyle::kWui, "name", UI::FontStyle::kWuiLabel, descr->descname());

	// "stock/target", coloured like the stock menu's "Evaluate" mode
	UI::Textarea* stock =
	   new UI::Textarea(row, UI::PanelStyle::kWui, "stock", UI::FontStyle::kWuiLabel, 0, 0,
		                 kStockWidth, kRowIconSize, "", UI::Align::kRight);
	stock->set_fixed_width(kStockWidth);
	stock->set_handle_mouse(true);  // for the tooltip

	// Production mode indicator (stopped / produce indefinitely). Reserved for a future
	// per-output production policy control (off / balanced by economy / always on).
	UI::Icon* status = new UI::Icon(
	   row, UI::PanelStyle::kWui, "status", 0, 0, kStatusIconSize, kStatusIconSize, nullptr);

	row->add(icon, UI::Box::Resizing::kAlign, UI::Align::kCenter);
	row->add_space(8);
	row->add(name, UI::Box::Resizing::kAlign, UI::Align::kCenter);
	row->add_inf_space();
	row->add(stock, UI::Box::Resizing::kAlign, UI::Align::kCenter);
	row->add_space(8);
	row->add(status, UI::Box::Resizing::kAlign, UI::Align::kCenter);
	row->add_space(8);  // Keep the status off the window edge
	row->set_desired_size(kBuildingInfoTabWidth, kRowIconSize);

	add(row, UI::Box::Resizing::kFullSize);
	rows_.push_back(Row{type, index, stock, status});
}

void BuildingOutputsDisplay::think() {
	const uint32_t now = SDL_GetTicks();
	if (now - last_update_ >= kUpdateIntervalMs) {
		update_rows();
	}
	UI::Box::think();
}

void BuildingOutputsDisplay::update_rows() {
	last_update_ = SDL_GetTicks();
	Widelands::ProductionSite* site = site_.get(ibase_.egbase());
	if (site == nullptr) {
		return;
	}
	const bool infinite = site->infinite_production();
	const bool stopped = site->is_stopped();

	for (Row& row : rows_) {
		// Production mode: same icons and tooltips as the stop/infinite buttons in the bottom bar
		if (stopped) {
			row.status->set_icon(g_image_cache->get(kImgStopped));
			row.status->set_tooltip(_("Stopped"));
		} else if (infinite) {
			row.status->set_icon(g_image_cache->get(kImgInfinite));
			row.status->set_tooltip(_("Produce indefinitely regardless of the economy’s needs"));
		} else {
			row.status->set_icon(nullptr);
			row.status->set_tooltip("");
		}

		// Stock against the economy target
		Widelands::Economy* economy = site->get_economy(row.type);
		if (economy == nullptr || economy->warehouses().empty()) {
			// Not connected to a warehouse: there is no stock to show
			row.stock->set_text("");
			row.stock->set_tooltip("");
			continue;
		}
		const Widelands::Quantity stock = warehouse_stock(*economy, row.type, row.index);
		const Widelands::Quantity target = economy->target_quantity(row.index).permanent;
		if (target == 0 || target == Widelands::kInvalidWare) {
			// No economy target for this ware (kInvalidWare = target is never checked)
			row.stock->set_text(std::to_string(stock));
			row.stock->set_style_override(
			   colored_font(UI::FontStyle::kWuiLabel,
				             g_style_manager->font_style(UI::FontStyle::kWuiLabel).color()));
			row.stock->set_tooltip("");
			continue;
		}
		const bool infinite_target = target == Widelands::kEconomyTargetInfinity;
		row.stock->set_text(
		   format(_("%1%/%2%"), stock, infinite_target ? std::string("∞") : std::to_string(target)));
		// Same colours and explanations as the stock menu's "Evaluate" mode
		if (infinite_target || stock < target) {
			row.stock->set_style_override(
			   colored_font(UI::FontStyle::kWuiLabel, colors().alternative_low_color()));
			row.stock->set_tooltip(_("Stock is lower than the target"));
		} else if (stock == target) {
			row.stock->set_style_override(
			   colored_font(UI::FontStyle::kWuiLabel, colors().alternative_medium_color()));
			row.stock->set_tooltip(_("Stock is equal to the target"));
		} else {
			row.stock->set_style_override(
			   colored_font(UI::FontStyle::kWuiLabel, colors().alternative_high_color()));
			row.stock->set_tooltip(_("Stock is higher than the target"));
		}
	}

	// What the building did last: the same (already translated) text that is shown when hovering
	// over it on the map, e.g. "Skipped mining coal because the economy doesn’t need the ware
	// ‘Coal’". The game only sets it when there is something to report, so it is often empty.
	// While the building is stopped the text is stale, and the rows already say "Stopped".
	const std::string& result = site->production_result();
	const std::string footer =
	   stopped || result.empty() ?
	      std::string() :
	      as_richtext(
	         as_paragraph_style(UI::ParagraphStyle::kWuiText,
				                   g_style_manager->font_style(UI::FontStyle::kWuiInfoPanelParagraph)
				                      .as_font_tag(richtext_escape(result))));
	if (footer != footer_->get_text()) {
		footer_->set_text(footer);
		footer_->set_visible(!footer.empty());
	}
}
