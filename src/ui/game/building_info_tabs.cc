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
#include <set>

#include <SDL_timer.h>

#include "base/i18n.h"
#include "base/macros.h"
#include "base/string.h"
#include "economy/economy.h"
#include "graphic/style_manager.h"
#include "graphic/text_layout.h"
#include "logic/map_objects/tribes/militarysite.h"
#include "logic/map_objects/tribes/tribe_descr.h"
#include "logic/map_objects/tribes/ware_descr.h"
#include "logic/map_objects/tribes/warehouse.h"
#include "logic/map_objects/tribes/worker_descr.h"
#include "logic/player.h"
#include "ui/game/waresdisplay.h"
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

/*
 * ========================================================================
 *                              Upgrade tab
 * ========================================================================
 */

namespace {

using Counts = std::map<Widelands::DescriptionIndex, Widelands::Quantity>;

/// The changes between two neighbouring levels of an upgrade chain.
struct LevelDiff {
	std::set<Widelands::DescriptionIndex> new_ware_outputs;
	std::set<Widelands::DescriptionIndex> new_worker_outputs;
	std::set<Widelands::DescriptionIndex> lost_ware_outputs;
	std::set<Widelands::DescriptionIndex> lost_worker_outputs;
	std::set<Widelands::DescriptionIndex> new_inputs;
	std::set<Widelands::DescriptionIndex> dropped_inputs;
	Counts additional_workers;
	// Military
	Widelands::Quantity soldiers_from{0U};
	Widelands::Quantity soldiers_to{0U};
	uint32_t conquers_from{0U};
	uint32_t conquers_to{0U};
};

template <typename Set>
std::set<Widelands::DescriptionIndex> set_difference(const Set& a, const Set& b) {
	std::set<Widelands::DescriptionIndex> result;
	for (Widelands::DescriptionIndex i : a) {
		if (b.count(i) == 0u) {
			result.insert(i);
		}
	}
	return result;
}

std::set<Widelands::DescriptionIndex> keys(const Widelands::BillOfMaterials& bom) {
	std::set<Widelands::DescriptionIndex> result;
	for (const auto& entry : bom) {
		result.insert(entry.first);
	}
	return result;
}

Counts counts(const Widelands::BillOfMaterials& bom) {
	Counts result;
	for (const auto& entry : bom) {
		result[entry.first] += entry.second;
	}
	return result;
}

LevelDiff compute_diff(const Widelands::BuildingDescr& from, const Widelands::BuildingDescr& to) {
	LevelDiff diff;
	upcast(const Widelands::ProductionSiteDescr, ps_from, &from);
	upcast(const Widelands::ProductionSiteDescr, ps_to, &to);
	static const Widelands::ProductionSiteDescr::Output kNoOutput;
	static const Widelands::BillOfMaterials kNoMaterials;

	const auto& from_wares = ps_from != nullptr ? ps_from->output_ware_types() : kNoOutput;
	const auto& to_wares = ps_to != nullptr ? ps_to->output_ware_types() : kNoOutput;
	const auto& from_workers = ps_from != nullptr ? ps_from->output_worker_types() : kNoOutput;
	const auto& to_workers = ps_to != nullptr ? ps_to->output_worker_types() : kNoOutput;
	diff.new_ware_outputs = set_difference(to_wares, from_wares);
	diff.lost_ware_outputs = set_difference(from_wares, to_wares);
	diff.new_worker_outputs = set_difference(to_workers, from_workers);
	diff.lost_worker_outputs = set_difference(from_workers, to_workers);

	const auto from_inputs = keys(ps_from != nullptr ? ps_from->input_wares() : kNoMaterials);
	const auto to_inputs = keys(ps_to != nullptr ? ps_to->input_wares() : kNoMaterials);
	diff.new_inputs = set_difference(to_inputs, from_inputs);
	diff.dropped_inputs = set_difference(from_inputs, to_inputs);

	const Counts from_positions =
	   counts(ps_from != nullptr ? ps_from->working_positions() : kNoMaterials);
	for (const auto& [index, amount] :
	     counts(ps_to != nullptr ? ps_to->working_positions() : kNoMaterials)) {
		const auto it = from_positions.find(index);
		const Widelands::Quantity before = it == from_positions.end() ? 0 : it->second;
		if (amount > before) {
			diff.additional_workers[index] = amount - before;
		}
	}

	upcast(const Widelands::MilitarySiteDescr, ms_from, &from);
	upcast(const Widelands::MilitarySiteDescr, ms_to, &to);
	if (ms_from != nullptr && ms_to != nullptr) {
		diff.soldiers_from = ms_from->get_max_number_of_soldiers();
		diff.soldiers_to = ms_to->get_max_number_of_soldiers();
	}
	diff.conquers_from = from.get_conquers();
	diff.conquers_to = to.get_conquers();
	return diff;
}

std::string font(UI::FontStyle style, const std::string& txt) {
	return g_style_manager->font_style(style).as_font_tag(txt);
}

/// An icon followed by its name, for use inside a paragraph.
std::string inline_item(const std::string& icon_filename, const std::string& label) {
	return format("<img src=\"%s\" width=22><space gap=4>%s", icon_filename,
	              font(UI::FontStyle::kWuiLabel, label));
}

std::string ware_item(const Widelands::TribeDescr& tribe, Widelands::DescriptionIndex i) {
	const Widelands::WareDescr* w = tribe.get_ware_descr(i);
	return inline_item(w->icon_filename(), richtext_escape(w->descname()));
}

std::string worker_item(const Widelands::TribeDescr& tribe,
                        Widelands::DescriptionIndex i,
                        Widelands::Quantity amount = 1) {
	const Widelands::WorkerDescr* w = tribe.get_worker_descr(i);
	// Plain digits and the multiplication sign need no translation
	return inline_item(
	   w->icon_filename(), (amount > 1 ? std::to_string(amount) + "×<space gap=2>" : std::string()) +
	                          richtext_escape(w->descname()));
}

/// "Conquer range:" as already translated for the building help in the encyclopedia. Looked up
/// in that text domain instead of adding a new string to the "widelands" catalog. Called via
/// i18n::translate rather than _() so that xgettext does not copy it into "widelands".
std::string conquer_range_label() {
	i18n::Textdomain td("tribes_encyclopedia");
	return i18n::translate("Conquer range:");
}

/// Green "+" for something gained, red "−" for something lost.
std::string sign(bool added) {
	return font(UI::FontStyle::kWuiLabel,
	            StyleManager::color_tag(
	               added ? "+" : "−", added ? colors().high_color() : colors().low_color())) +
	       "<space gap=4>";
}

/// A labelled line: the label in a narrow left column, the content to its right.
std::string labelled_line(const std::string& label, const std::string& content) {
	return format("<div width=100%%><div width=36%%><p><vspace gap=4>%s</p></div>"
	              "<div width=64%%><p valign=center spacing=4>%s</p></div></div>",
	              font(UI::FontStyle::kWuiInfoPanelParagraph, richtext_escape(label)), content);
}

/// Like labelled_line(), but 'block' is a sequence of divs (e.g. ware tiles), not inline content.
/// 'label_gap' vertically aligns the label with the first row of the block.
std::string labelled_block(const std::string& label, const std::string& block, int label_gap = 12) {
	return format("<div width=100%%><div width=36%%><p><vspace gap=%d>%s</p></div>"
	              "<div width=64%%>%s</div></div>",
	              label_gap, font(UI::FontStyle::kWuiInfoPanelParagraph, richtext_escape(label)),
	              block);
}

/// Lays out items in a two-column grid so that no item is split across lines.
std::string labelled_items(const std::string& label, const std::vector<std::string>& items) {
	const char* const cell_width = items.size() == 1 ? "100%" : "50%";
	std::string grid;
	for (const std::string& item : items) {
		grid += format("<div width=%s><p valign=center>%s</p></div>", cell_width, item);
	}
	return labelled_block(label, grid, 4);
}

std::string section_heading(const std::string& title) {
	return format("<p><vspace gap=10>%s</p><p><vspace gap=2></p>",
	              font(UI::FontStyle::kWuiInfoPanelHeading, richtext_escape(title)));
}

std::string plain(const std::string& txt) {
	return font(UI::FontStyle::kWuiLabel, txt);
}

/// "2 → 5", with the new value green if it grew and red if it shrank. Numbers only, no text.
std::string change_item(uint32_t from, uint32_t to) {
	const RGBColor& color = to > from ? colors().high_color() : colors().low_color();
	return plain(std::to_string(from) + "<space gap=6>→<space gap=6>" +
	             StyleManager::color_tag(std::to_string(to), color));
}

/// Renders what changes when going from one level to the next. Empty if nothing changes.
std::string diff_richtext(const Widelands::TribeDescr& tribe, const LevelDiff& diff) {
	std::string result;

	std::vector<std::string> outputs;
	for (Widelands::DescriptionIndex i : diff.new_ware_outputs) {
		outputs.push_back(sign(true) + ware_item(tribe, i));
	}
	for (Widelands::DescriptionIndex i : diff.new_worker_outputs) {
		outputs.push_back(sign(true) + worker_item(tribe, i));
	}
	for (Widelands::DescriptionIndex i : diff.lost_ware_outputs) {
		outputs.push_back(sign(false) + ware_item(tribe, i));
	}
	for (Widelands::DescriptionIndex i : diff.lost_worker_outputs) {
		outputs.push_back(sign(false) + worker_item(tribe, i));
	}
	if (!outputs.empty()) {
		result += labelled_items(_("Production"), outputs);
	}

	std::vector<std::string> inputs;
	if (diff.new_inputs.size() == 1 && diff.dropped_inputs.size() == 1) {
		// A straight replacement, e.g. "Snack → Meal"
		inputs.push_back(ware_item(tribe, *diff.dropped_inputs.begin()) + "<space gap=6>" +
		                 plain("→") + "<space gap=6>" + ware_item(tribe, *diff.new_inputs.begin()));
	} else {
		for (Widelands::DescriptionIndex i : diff.new_inputs) {
			inputs.push_back(sign(true) + ware_item(tribe, i));
		}
		for (Widelands::DescriptionIndex i : diff.dropped_inputs) {
			inputs.push_back(sign(false) + ware_item(tribe, i));
		}
	}
	if (!inputs.empty()) {
		// Same name as the inputs tab of the building window
		result += labelled_items(_("Wares"), inputs);
	}

	if (!diff.additional_workers.empty()) {
		std::vector<std::string> workers;
		for (const auto& [index, amount] : diff.additional_workers) {
			workers.push_back(sign(true) + worker_item(tribe, index, amount));
		}
		result += labelled_items(_("Workers"), workers);
	}

	if (diff.soldiers_from != diff.soldiers_to) {
		result += labelled_line(_("Soldiers"), change_item(diff.soldiers_from, diff.soldiers_to));
	}
	if (diff.conquers_from != diff.conquers_to) {
		result +=
		   labelled_line(conquer_range_label(), change_item(diff.conquers_from, diff.conquers_to));
	}
	return result;
}

/// One cell of the chain strip at the top of the tab. The current building is shown in the
/// regular (yellow) label colour, its neighbours in the muted grey of the section headings.
std::string chain_cell(const Widelands::BuildingDescr& descr, bool highlighted) {
	return format("<div width=30%% padding=4><p align=center><img src=\"%s\" width=48>"
	              "</p><p align=center>%s</p></div>",
	              descr.icon_filename(),
	              font(highlighted ? UI::FontStyle::kWuiLabel : UI::FontStyle::kWuiInfoPanelHeading,
	                   richtext_escape(descr.descname())));
}

std::string chain_arrow() {
	return "<div width=5%><p align=center><vspace gap=22>"
	       "<img src=\"images/ui_basic/scrollbar_right.png\"></p></div>";
}

std::string empty_cell() {
	return "<div width=30%><p></p></div>";
}

const Widelands::BuildingDescr* level(const Widelands::TribeDescr& tribe,
                                      Widelands::DescriptionIndex index) {
	if (index == Widelands::INVALID_INDEX || !tribe.has_building(index)) {
		return nullptr;
	}
	return tribe.get_building_descr(index);
}

}  // namespace

bool building_can_be_enhanced(const Widelands::TribeDescr& tribe,
                              const Widelands::BuildingDescr& descr) {
	return level(tribe, descr.enhancement()) != nullptr;
}

std::string building_upgrade_richtext(const Widelands::Player& player,
                                      const Widelands::BuildingDescr& current) {
	const Widelands::TribeDescr& tribe = player.tribe();
	const Widelands::BuildingDescr* previous = level(tribe, current.enhanced_from());
	const Widelands::BuildingDescr* next = level(tribe, current.enhancement());
	assert(next != nullptr);

	std::string result;

	// Chain strip: previous → current → next
	result += "<div width=100%>";
	result += previous != nullptr ? chain_cell(*previous, false) : empty_cell();
	result += previous != nullptr ? chain_arrow() : "<div width=5%><p></p></div>";
	result += chain_cell(current, true);
	result += chain_arrow();
	result += chain_cell(*next, false);
	result += "</div>";

	// What changes with the enhancement. Same heading as the enhance button's tooltip.
	result += section_heading(format(_("Enhance to %s"), next->descname()));
	result += labelled_block(
	   _("Construction costs:"), waremap_to_richtext(tribe, next->enhancement_cost()));
	result += diff_richtext(tribe, compute_diff(current, *next));

	result += "<p><vspace gap=8></p>";

	return as_richtext(result);
}
