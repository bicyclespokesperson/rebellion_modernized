// The command HUD's popups (tactical_combat.md §5.2, §5.3, §5.4, §5.7): a doctrine setting's options, the target
// priority editor, presets, options, the fleet-withdrawal confirmation, and the ghost of a roster drag.
#include <algorithm>

#include "battle_ui/command_hud.h"
#include "battle_ui/hud_draw.h"
#include "battle_ui/selection.h"

namespace reb::battle_ui
{

using battle::UnitIndex;

namespace
{

constexpr float kSlotRow = 24.0f;
constexpr float kTagHeight = 20.0f;
constexpr float kPriorityDragThreshold = 4.0f;

std::string PresetSummary(const DoctrinePreset& preset)
{
  std::string summary;
  for (DoctrineField field : preset.flight ? FlightFields() : ShipFields()) {
    if (!(preset.fields & FieldMask(field, preset.flight))) {
      continue;
    }
    std::string value
        = field == DoctrineField::Priorities
              ? PriorityText(preset.flight ? preset.flightDoctrine.priorities : preset.ship.priorities)
              : std::string(Option(field, GetOption(field, preset.ship, preset.flightDoctrine)).shortName);
    summary += (summary.empty() ? "" : ", ") + value;
  }
  return summary;
}

struct TagPlacement {
  battle::TargetTag tag;
  Rect rect; // relative to the palette's origin
};

// Flows the tag palette into rows under a caption per tag group; returns the palette's height.
float LayoutPalette(Ui& ui, float width, std::vector<TagPlacement>& out)
{
  const float caption = ui.S(46.0f);
  const float gap = ui.S(3.0f);
  float y = 0.0f;
  TagGroup group = TagGroup::Size;
  float x = caption;
  for (int i = 0; i < kTargetTagCount; ++i) {
    const auto tag = static_cast<battle::TargetTag>(i);
    const float w = ui.TextWidth(Describe(tag).shortName, 11.0f, FontWeight::Bold) + ui.S(14.0f);
    if (i > 0 && GroupOf(tag) != group) {
      group = GroupOf(tag);
      y += ui.S(kTagHeight) + ui.S(5.0f);
      x = caption;
    } else if (x + w > width) {
      y += ui.S(kTagHeight) + gap;
      x = caption;
    }
    out.push_back({tag, {x, y, w, ui.S(kTagHeight)}});
    x += w + gap;
  }
  return y + ui.S(kTagHeight);
}

} // namespace

void CommandHud::DrawPopup(Ui& ui)
{
  if (m_popup.kind == PopupKind::None) {
    return;
  }
  const bool doctrinePopup
      = m_popup.kind == PopupKind::Field || m_popup.kind == PopupKind::Priorities || m_popup.kind == PopupKind::Presets;
  if (doctrinePopup && (m_subject.Empty() || !(m_subject == m_popup.subject))) {
    ClosePopup();
    return;
  }
  if (m_popup.kind != PopupKind::ConfirmWithdraw && ui.ActiveId() != 0 && !m_popup.rect.Empty()
      && !m_popup.rect.Contains(ui.PressPoint()) && !m_popup.anchor.Contains(ui.PressPoint())) {
    ClosePopup();
    return;
  }
  const CardDoctrine doctrine = ReadCardDoctrine();
  switch (m_popup.kind) {
    case PopupKind::Field:
      DrawFieldPopup(ui, doctrine);
      break;
    case PopupKind::Priorities:
      DrawPriorityPopup(ui, doctrine);
      break;
    case PopupKind::Presets:
      DrawPresetPopup(ui, doctrine);
      break;
    case PopupKind::Options:
      DrawOptionsPopup(ui);
      break;
    case PopupKind::ConfirmWithdraw:
      DrawConfirmWithdraw(ui);
      break;
    case PopupKind::None:
      break;
  }
}

void CommandHud::DrawFieldPopup(Ui& ui, const CardDoctrine& doctrine)
{
  const Theme& theme = ui.GetTheme();
  const DoctrineField field = m_popup.field;
  const battle::DoctrineOverrideMask mask = FieldMask(field, m_subject.flight);
  const int count = OptionCount(field);
  const int current = (doctrine.mixed & mask) ? -1 : GetOption(field, doctrine.ship, doctrine.flight);
  const bool canFollow = !m_subject.IsGroup() && doctrine.hasGroup && (doctrine.overrides & mask);
  const float width = ui.S(330.0f);
  const float textWidth = width - ui.S(16.0f) - ui.S(14.0f);
  const float helpLine = ui.Text().LineHeight(ui.S(10.5f)) + ui.S(1.0f);
  std::vector<std::vector<std::string>> helps;
  float height = ui.S(22.0f + 11.0f);
  for (int i = 0; i < count; ++i) {
    helps.push_back(WrapText(ui.Text(), Option(field, i).help, ui.S(10.5f), textWidth, 2));
    height += ui.S(24.0f) + helpLine * static_cast<float>(helps.back().size()) + ui.S(2.0f);
  }
  if (canFollow) {
    height += ui.S(30.0f);
  }
  const std::string title
      = std::string(Describe(field).label) + (m_subject.IsGroup() ? ": group doctrine" : ": override");
  const Rect rect = PlacePopup(ui, width, height);
  m_popup.rect = rect;
  const Rect content = hud::PopupFrame(ui, Id("hud.popup.field"), rect, title);
  float y = content.y;
  for (int i = 0; i < count; ++i) {
    const OptionText& option = Option(field, i);
    const float rowHeight = ui.S(24.0f) + helpLine * static_cast<float>(helps[i].size());
    const Rect row{content.x - ui.S(4.0f), y, content.w + ui.S(8.0f), rowHeight};
    const WidgetId id = hud_id::FieldOption(i);
    const bool clicked = ui.Region(id, row);
    Record(id, row);
    const bool on = i == current;
    if (on) {
      ui.Draw().FillRect(row, theme.rowSelected);
    } else if (ui.Hovered(id)) {
      ui.Draw().FillRect(row, theme.rowHover);
    }
    if (on) {
      ui.Draw().FillRect({row.x, row.y, ui.S(3.0f), row.h}, theme.accent);
    }
    ui.Label({content.x + ui.S(6.0f), y + ui.S(3.0f), textWidth, ui.S(17.0f)}, option.name,
             {12.0f, theme.text, Align::Start, VAlign::Middle, on ? FontWeight::Bold : FontWeight::Regular});
    float hy = y + ui.S(20.0f);
    for (const std::string& line : helps[i]) {
      ui.Label({content.x + ui.S(6.0f), hy, textWidth, helpLine}, line, {10.5f, theme.textDim});
      hy += helpLine;
    }
    if (clicked) {
      CardDoctrine edited = doctrine;
      SetOption(field, i, edited.ship, edited.flight);
      SubmitDoctrine(edited, mask);
      ClosePopup();
      const bool protects
          = (field == DoctrineField::Role && i == static_cast<int>(battle::Role::Screen))
            || (field == DoctrineField::Mission && i == static_cast<int>(battle::FlightMission::Escort));
      if (protects) {
        StartPick(PickMode::Protect);
      }
      return;
    }
    y += rowHeight + ui.S(2.0f);
  }
  if (canFollow) {
    const std::string groupValue
        = std::string(Option(field, GetOption(field, doctrine.groupShip, doctrine.groupFlight)).name);
    const Rect row{content.x, y + ui.S(4.0f), content.w, ui.S(24.0f)};
    if (hud::Button(ui, hud_id::FollowGroup(), row, Icon::Close, "Follow group: " + groupValue)) {
      ClearOverrides(mask);
      ClosePopup();
      return;
    }
    ui.Tooltip(hud_id::FollowGroup(), "Drop this override so the selection follows its group's setting again.");
    Record(hud_id::FollowGroup(), row);
  }
}

void CommandHud::DrawPriorityPopup(Ui& ui, const CardDoctrine& doctrine)
{
  const Theme& theme = ui.GetTheme();
  const battle::DoctrineOverrideMask mask = FieldMask(DoctrineField::Priorities, m_subject.flight);
  const battle::TargetPriorities& priorities = m_subject.flight ? doctrine.flight.priorities : doctrine.ship.priorities;
  const bool canFollow = !m_subject.IsGroup() && doctrine.hasGroup && (doctrine.overrides & mask);
  const float width = ui.S(380.0f);
  const float inner = width - ui.S(16.0f);
  std::vector<TagPlacement> palette;
  const float paletteHeight = LayoutPalette(ui, inner, palette);
  const int slots = std::min<int>(priorities.count + 1, battle::TargetPriorities::kMaxTags);
  const std::vector<std::string> intro = WrapText(
      ui.Text(),
      "The best-ranked tag a target matches scores 8, then 4, 2 and 1. Drag a slot to reorder; click a slot, "
      "then a tag, to replace it; click a listed tag to remove it.",
      ui.S(10.0f), inner, 3);
  const float introLine = ui.Text().LineHeight(ui.S(10.0f)) + ui.S(1.0f);
  const float introHeight = introLine * static_cast<float>(intro.size()) + ui.S(4.0f);
  const float height = ui.S(22.0f + 11.0f) + introHeight + static_cast<float>(slots) * ui.S(kSlotRow) + ui.S(28.0f)
                       + ui.S(16.0f) + paletteHeight + (canFollow ? ui.S(32.0f) : 0.0f);
  const Rect rect = PlacePopup(ui, width, height);
  m_popup.rect = rect;
  const Rect content
      = hud::PopupFrame(ui, Id("hud.popup.priorities"), rect,
                        m_subject.IsGroup() ? "TARGET PRIORITIES: group doctrine" : "TARGET PRIORITIES: override");
  float y = content.y;
  for (const std::string& line : intro) {
    ui.Label({content.x, y, content.w, introLine}, line, {10.0f, theme.textDim});
    y += introLine;
  }
  y += ui.S(4.0f);

  // The list: drag a slot to reorder, click to select it for replacement, x to remove.
  const float slotsTop = y;
  const auto slotAt = [&](float mouseY) {
    return std::clamp(static_cast<int>((mouseY - slotsTop) / ui.S(kSlotRow)), 0, std::max(0, priorities.count - 1));
  };
  for (int i = 0; i < priorities.count; ++i) {
    const WidgetId id = hud_id::PrioritySlot(i);
    if (ui.ActiveId() == id && m_priorityDrag < 0 && !m_pressAbandoned
        && Distance(ui.Mouse(), ui.PressPoint()) > ui.S(kPriorityDragThreshold)) {
      m_priorityDrag = i;
    }
  }
  const bool wasDragging = m_priorityDrag >= 0;
  if (m_priorityDrag >= 0 && ui.ActiveId() != hud_id::PrioritySlot(m_priorityDrag)) {
    const int to = slotAt(ui.Mouse().y);
    if (to != m_priorityDrag && ui.ReleasedId() == hud_id::PrioritySlot(m_priorityDrag)) {
      SubmitPriorities(doctrine, MovePriority(priorities, m_priorityDrag, to));
    }
    m_priorityDrag = -1;
    m_prioritySlot = -1;
  }
  for (int i = 0; i < priorities.count; ++i) {
    const WidgetId id = hud_id::PrioritySlot(i);
    const Rect row{content.x, y + static_cast<float>(i) * ui.S(kSlotRow), content.w, ui.S(kSlotRow) - ui.S(2.0f)};
    const Rect remove{row.Right() - row.h, row.y, row.h, row.h};
    const bool removeClicked = ui.Region(hud_id::PriorityRemove(i), remove, priorities.count > 1);
    const bool clicked = ui.Region(id, {row.x, row.y, row.w - row.h, row.h});
    Record(id, row);
    Record(hud_id::PriorityRemove(i), remove);
    const bool dragging = m_priorityDrag == i;
    const bool selected = m_prioritySlot == i;
    ui.Draw().FillRect(row, selected ? theme.buttonOn : ui.Hovered(id) ? theme.buttonHover : theme.buttonFace);
    ui.Draw().StrokeRect(row, selected ? theme.accent : theme.buttonBorder, ui.S(1.0f));
    ui.Label({row.x + ui.S(6.0f), row.y, ui.S(16.0f), row.h}, std::to_string(i + 1),
             {12.0f, theme.title, Align::Start, VAlign::Middle, FontWeight::Bold});
    DrawIcon(ui.Draw(), Icon::Grip, {row.x + ui.S(22.0f), row.y + ui.S(5.0f), ui.S(12.0f), row.h - ui.S(10.0f)},
             theme.textDim, ui.Scale());
    const battle::TargetTag tag = priorities.tags[i];
    ui.Label({row.x + ui.S(42.0f), row.y, row.w - ui.S(70.0f), row.h}, Describe(tag).name,
             {12.0f, dragging ? theme.textDim : theme.text, Align::Start, VAlign::Middle, FontWeight::Bold});
    const float weightWidth = ui.S(40.0f);
    ui.Label({row.Right() - row.h - weightWidth, row.y, weightWidth, row.h}, "x" + std::to_string(8 >> i),
             {10.0f, theme.textDim, Align::End});
    if (priorities.count > 1) {
      DrawIcon(ui.Draw(), Icon::Close, remove.Inset(row.h * 0.3f),
               ui.Hovered(hud_id::PriorityRemove(i)) ? theme.hullPoor : theme.textDim, ui.Scale());
      ui.Tooltip(hud_id::PriorityRemove(i), "Remove this tag from the list.");
    }
    ui.Tooltip(id, std::string(Describe(tag).name) + ": " + std::string(Describe(tag).help)
                       + " Drag to reorder; click to select it, then pick a tag below to replace it.");
    if (removeClicked) {
      SubmitPriorities(doctrine, RemovePriority(priorities, i));
      m_prioritySlot = -1;
      return;
    }
    if (clicked && !wasDragging && m_priorityDrag < 0) {
      m_prioritySlot = selected ? -1 : i;
    }
  }
  if (m_priorityDrag >= 0) {
    const int to = slotAt(ui.Mouse().y);
    const float lineY = slotsTop + static_cast<float>(to > m_priorityDrag ? to + 1 : to) * ui.S(kSlotRow) - ui.S(2.0f);
    ui.Draw().FillRect({content.x, lineY, content.w, ui.S(2.0f)}, theme.accent);
    const std::string_view name = Describe(priorities.tags[m_priorityDrag]).name;
    const Rect ghost{ui.Mouse().x + ui.S(12.0f), ui.Mouse().y - ui.S(10.0f),
                     ui.TextWidth(name, 12.0f, FontWeight::Bold) + ui.S(16.0f), ui.S(20.0f)};
    ui.Draw().FillRect(ghost, theme.buttonOn.WithAlpha(235));
    ui.Draw().StrokeRect(ghost, theme.accent, ui.S(1.0f));
    ui.Label(ghost, name, {12.0f, theme.text, Align::Center, VAlign::Middle, FontWeight::Bold});
  }
  y += static_cast<float>(priorities.count) * ui.S(kSlotRow);
  if (priorities.count < battle::TargetPriorities::kMaxTags) {
    ui.Label({content.x + ui.S(42.0f), y, content.w, ui.S(kSlotRow) - ui.S(2.0f)},
             m_prioritySlot >= 0 ? "Pick a tag below to replace the selected slot" : "Pick a tag below to add it",
             {11.0f, theme.textDim});
    y += ui.S(kSlotRow);
  }
  bool finish = priorities.finishDamaged;
  const Rect toggle{content.x, y + ui.S(4.0f), content.w, ui.S(20.0f)};
  if (ui.Toggle(hud_id::FinishDamaged(), toggle, "Finish damaged targets", finish)) {
    battle::TargetPriorities edited = priorities;
    edited.finishDamaged = finish;
    SubmitPriorities(doctrine, edited);
  }
  ui.Tooltip(hud_id::FinishDamaged(), "Favour targets the group can destroy within a few seconds.");
  Record(hud_id::FinishDamaged(), toggle);
  y += ui.S(28.0f);
  ui.Draw().FillRect({content.x, y + ui.S(4.0f), content.w, ui.S(1.0f)}, theme.panelBorder.WithAlpha(80));
  y += ui.S(12.0f);

  TagGroup group = TagGroup::Fallback;
  for (const TagPlacement& placement : palette) {
    const Rect chip{content.x + placement.rect.x, y + placement.rect.y, placement.rect.w, placement.rect.h};
    if (GroupOf(placement.tag) != group) {
      group = GroupOf(placement.tag);
      hud::Caption(ui, {content.x, chip.y, ui.S(44.0f), chip.h}, Name(group));
    }
    const WidgetId id = hud_id::PriorityTag(placement.tag);
    const auto begin = priorities.tags.begin();
    const bool listed = std::find(begin, begin + priorities.count, placement.tag) != begin + priorities.count;
    if (hud::Segment(ui, id, chip, Describe(placement.tag).shortName, listed)) {
      if (m_prioritySlot >= 0) {
        SubmitPriorities(doctrine, SetPriority(priorities, m_prioritySlot, placement.tag));
        m_prioritySlot = -1;
      } else if (listed) {
        if (priorities.count > 1) {
          const int at = static_cast<int>(std::find(begin, begin + priorities.count, placement.tag) - begin);
          SubmitPriorities(doctrine, RemovePriority(priorities, at));
        }
      } else if (priorities.count < battle::TargetPriorities::kMaxTags) {
        SubmitPriorities(doctrine, SetPriority(priorities, priorities.count, placement.tag));
      } else {
        AddToast("The list holds four tags: click a slot first to replace it");
      }
    }
    ui.Tooltip(id, std::string(Describe(placement.tag).name) + ": " + std::string(Describe(placement.tag).help)
                       + (listed ? " Click to remove it from the list." : " Click to add it."));
    Record(id, chip);
  }
  y += paletteHeight;
  if (canFollow) {
    const Rect row{content.x, y + ui.S(8.0f), content.w, ui.S(24.0f)};
    if (hud::Button(
            ui, hud_id::FollowGroup(), row, Icon::Close,
            "Follow group: "
                + PriorityText(m_subject.flight ? doctrine.groupFlight.priorities : doctrine.groupShip.priorities))) {
      ClearOverrides(mask);
      ClosePopup();
      return;
    }
    ui.Tooltip(hud_id::FollowGroup(), "Drop this override so the selection follows its group's priorities again.");
    Record(hud_id::FollowGroup(), row);
  }
}

void CommandHud::DrawPresetPopup(Ui& ui, const CardDoctrine& doctrine)
{
  const Theme& theme = ui.GetTheme();
  std::vector<const DoctrinePreset*> presets;
  for (const DoctrinePreset& preset : BuiltInPresets()) {
    if (preset.flight == m_subject.flight) {
      presets.push_back(&preset);
    }
  }
  const size_t builtIn = presets.size();
  for (const DoctrinePreset& preset : m_settings.presets) {
    if (preset.flight == m_subject.flight) {
      presets.push_back(&preset);
    }
  }
  const float rowHeight = ui.S(36.0f);
  const float width = ui.S(340.0f);
  const float height = ui.S(22.0f + 11.0f) + rowHeight * static_cast<float>(presets.size())
                       + (presets.size() > builtIn ? ui.S(10.0f) : 0.0f) + ui.S(36.0f);
  const Rect rect = PlacePopup(ui, width, height);
  m_popup.rect = rect;
  const Rect content = hud::PopupFrame(ui, Id("hud.popup.presets"), rect,
                                       m_subject.flight ? "PRESETS: FLIGHTS" : "PRESETS: TASK FORCES");
  float y = content.y;
  for (size_t i = 0; i < presets.size(); ++i) {
    if (i == builtIn) {
      ui.Draw().FillRect({content.x, y + ui.S(4.0f), content.w, ui.S(1.0f)}, theme.panelBorder.WithAlpha(80));
      y += ui.S(10.0f);
    }
    const DoctrinePreset& preset = *presets[i];
    const Rect row{content.x - ui.S(4.0f), y, content.w + ui.S(8.0f), rowHeight - ui.S(2.0f)};
    const WidgetId id = hud_id::PresetRow(static_cast<int>(i));
    const bool user = i >= builtIn;
    const Rect remove{row.Right() - ui.S(24.0f), row.y, ui.S(24.0f), row.h};
    const bool removeClicked = user && ui.Region(Id(id, 1), remove);
    const bool clicked = ui.Region(id, {row.x, row.y, row.w - (user ? remove.w : 0.0f), row.h});
    Record(id, row);
    if (ui.Hovered(id)) {
      ui.Draw().FillRect(row, theme.rowHover);
    }
    ui.Label({content.x + ui.S(4.0f), y + ui.S(2.0f), content.w - ui.S(30.0f), ui.S(16.0f)}, preset.name,
             {12.0f, theme.text, Align::Start, VAlign::Middle, FontWeight::Bold});
    ui.Label({content.x + ui.S(4.0f), y + ui.S(18.0f), content.w - ui.S(30.0f), ui.S(14.0f)}, PresetSummary(preset),
             {10.0f, theme.textDim});
    ui.Tooltip(id, "Apply \"" + preset.name + "\": " + PresetSummary(preset) + ".");
    if (user) {
      DrawIcon(ui.Draw(), Icon::Close, remove.Inset(ui.S(7.0f), ui.S(11.0f)),
               ui.Hovered(Id(id, 1)) ? theme.hullPoor : theme.textDim, ui.Scale());
      ui.Tooltip(Id(id, 1), "Delete this preset.");
    }
    if (removeClicked) {
      HudSettings settings = m_settings;
      std::erase_if(settings.presets, [&](const DoctrinePreset& existing) {
        return existing.name == preset.name && existing.flight == preset.flight;
      });
      ApplySettings(std::move(settings));
      return;
    }
    if (clicked) {
      if (Submit(PresetCommand(m_subject, preset))) {
        AddToast("Applied preset \"" + preset.name + "\"");
      }
      ClosePopup();
      return;
    }
    y += rowHeight;
  }
  const Rect footer{content.x, content.Bottom() - ui.S(26.0f), content.w, ui.S(26.0f)};
  if (m_naming) {
    ui.Region(Id("hud.preset.name"), footer);
    ui.Draw().FillRect(footer, theme.barBack);
    ui.Draw().StrokeRect(footer, theme.accent, ui.S(1.0f));
    const std::string shown = m_presetName.empty() ? "Type a name" : m_presetName;
    ui.Label(footer.Inset(ui.S(8.0f), 0.0f), shown,
             {12.0f, m_presetName.empty() ? theme.textDim : theme.text, Align::Start});
    const float caret = footer.x + ui.S(8.0f) + (m_presetName.empty() ? 0.0f : ui.TextWidth(m_presetName, 12.0f));
    if (static_cast<int>(m_screen.Seconds() * 2.0) % 2 == 0) {
      ui.Draw().FillRect({caret + ui.S(1.0f), footer.y + ui.S(5.0f), ui.S(1.5f), footer.h - ui.S(10.0f)}, theme.text);
    }
    ui.Label(footer.Inset(ui.S(8.0f), 0.0f), "Enter saves, Esc cancels", {10.0f, theme.textDim, Align::End});
    return;
  }
  if (hud::Button(ui, hud_id::SavePreset(), footer, Icon::Plus, "Save this doctrine as a preset")) {
    m_naming = true;
    m_presetName.clear();
  }
  ui.Tooltip(hud_id::SavePreset(), "Save every setting on the card under a name, for this and later battles.");
  Record(hud_id::SavePreset(), footer);
  (void)doctrine;
}

void CommandHud::DrawOptionsPopup(Ui& ui)
{
  const Theme& theme = ui.GetTheme();
  const float row = ui.S(22.0f);
  const float width = ui.S(340.0f);
  const float height = ui.S(22.0f + 11.0f) + ui.S(18.0f) + row * kAutoPauseEventCount + ui.S(8.0f)
                       + 3.0f * (ui.S(18.0f) + row + ui.S(8.0f));
  const Rect rect = PlacePopup(ui, width, height);
  m_popup.rect = rect;
  const Rect content = hud::PopupFrame(ui, Id("hud.popup.options"), rect, "OPTIONS");
  float y = content.y;
  HudSettings settings = m_settings;
  bool changed = false;
  hud::Caption(ui, {content.x, y, content.w, ui.S(16.0f)}, "AUTO-PAUSE WHEN");
  y += ui.S(18.0f);
  for (int i = 0; i < kAutoPauseEventCount; ++i) {
    const auto event = static_cast<AutoPauseEvent>(i);
    const WidgetId id = hud_id::AutoPause(event);
    const Rect line{content.x, y, content.w, row};
    bool value = settings.autoPause[i];
    if (ui.Toggle(id, line, Describe(event).label, value)) {
      settings.autoPause[i] = value;
      changed = true;
    }
    ui.Tooltip(id, Describe(event).help);
    Record(id, line);
    y += row;
  }
  y += ui.S(8.0f);
  const auto segments
      = [&](std::string_view caption, int count, auto label, auto isOn, auto choose, auto id, auto help) {
          hud::Caption(ui, {content.x, y, content.w, ui.S(16.0f)}, caption);
          y += ui.S(18.0f);
          const float w = content.w / static_cast<float>(count);
          for (int i = 0; i < count; ++i) {
            const Rect segment{content.x + static_cast<float>(i) * w, y, w + ui.S(1.0f), row};
            const WidgetId widget = id(i);
            if (hud::Segment(ui, widget, segment, label(i), isOn(i))) {
              choose(i);
              changed = true;
            }
            ui.Tooltip(widget, help(i));
            Record(widget, segment);
          }
          y += row + ui.S(8.0f);
        };
  const auto& phases = HudSettings::kPhaseChoices;
  segments(
      "COMMAND PHASES", static_cast<int>(phases.size()),
      [&](int i) {
        return phases[i] == 0 ? std::string("Off") : "Every " + std::to_string(phases[i]) + " s";
      },
      [&](int i) {
        return settings.commandPhaseSeconds == phases[i];
      },
      [&](int i) {
        settings.commandPhaseSeconds = phases[i];
      },
      [&](int i) {
        return hud_id::CommandPhase(phases[i]);
      },
      [&](int i) {
        return phases[i] == 0 ? std::string("Real time: the battle only pauses when you pause it or an event does.")
                              : "Pause every " + std::to_string(phases[i])
                                    + " s of battle time so you can plan, then resume: simultaneous turns.";
      });
  segments(
      "FACTION COLOURS", 2,
      [](int i) {
        return std::string(i ? "Colour-blind safe" : "Standard");
      },
      [&](int i) {
        return settings.colorBlindPalette == (i == 1);
      },
      [&](int i) {
        settings.colorBlindPalette = i == 1;
      },
      [](int i) {
        return hud_id::Palette(i == 1);
      },
      [](int i) {
        return std::string(i ? "Orange Alliance and sky-blue Empire, distinct for every common colour blindness."
                             : "The original red Alliance and green Empire.");
      });
  const auto& scales = HudSettings::kHudScales;
  segments(
      "HUD SIZE", static_cast<int>(scales.size()),
      [&](int i) {
        return std::to_string(static_cast<int>(scales[i] * 100.0f + 0.5f)) + "%";
      },
      [&](int i) {
        return std::abs(settings.hudScale - scales[i]) < 0.01f;
      },
      [&](int i) {
        settings.hudScale = scales[i];
      },
      [](int i) {
        return hud_id::HudScale(i);
      },
      [](int) {
        return std::string("Scale the HUD's text, panels and markers.");
      });
  if (changed) {
    ApplySettings(std::move(settings));
  }
  (void)theme;
}

void CommandHud::DrawConfirmWithdraw(Ui& ui)
{
  const Theme& theme = ui.GetTheme();
  const Rect screen = m_screen.Layout().screen;
  if (ui.Region(Id("hud.modal"), screen)) {
    ClosePopup();
    return;
  }
  ui.Draw().FillRect(screen, Rgb(0x000000, 120));
  const float width = ui.S(400.0f);
  const float height = ui.S(140.0f);
  const Rect rect{screen.Center().x - width * 0.5f, screen.Center().y - height * 0.5f, width, height};
  m_popup.rect = rect;
  const Rect content = hud::PopupFrame(ui, Id("hud.popup.withdraw"), rect, "WITHDRAW CONFIRMATION");
  float y = content.y;
  ui.Label({content.x, y, content.w, ui.S(18.0f)}, "Are you sure you wish to order a fleet withdrawal?",
           {13.0f, theme.text, Align::Start, VAlign::Middle, FontWeight::Bold});
  y += ui.S(20.0f);
  for (const std::string& line :
       WrapText(ui.Text(),
                "Every ship spools its hyperdrive and jumps as soon as it can. Ships held by a tractor beam or inside "
                "a gravity well keep fighting until they can leave.",
                ui.S(11.0f), content.w, 3)) {
    ui.Label({content.x, y, content.w, ui.S(14.0f)}, line, {11.0f, theme.textDim});
    y += ui.S(14.0f);
  }
  const float buttonWidth = ui.S(150.0f);
  const Rect confirm{content.Right() - 2.0f * buttonWidth - ui.S(8.0f), content.Bottom() - ui.S(26.0f), buttonWidth,
                     ui.S(26.0f)};
  const Rect cancel{content.Right() - buttonWidth, confirm.y, buttonWidth, confirm.h};
  Record(hud_id::ConfirmWithdraw(), confirm);
  if (hud::Button(ui, hud_id::ConfirmWithdraw(), confirm, Icon::Withdraw, "Withdraw fleet", true, true)) {
    ClosePopup();
    WithdrawFleet();
    return;
  }
  ui.Tooltip(hud_id::ConfirmWithdraw(), "Order every ship and squadron to leave the battle.");
  if (hud::Button(ui, Id("hud.withdraw_fleet.cancel"), cancel, Icon::Close, "Keep fighting")) {
    ClosePopup();
  }
  ui.Tooltip(Id("hud.withdraw_fleet.cancel"), "Close this without ordering anything (Esc).");
}

void CommandHud::DrawDragGhost(Ui& ui)
{
  if (!m_rosterDragging || m_dragUnits.empty()) {
    return;
  }
  const Theme& theme = ui.GetTheme();
  std::string text = m_screen.Label(m_dragUnits.front()).name;
  if (m_dragUnits.size() > 1) {
    text += " +" + std::to_string(m_dragUnits.size() - 1);
  }
  const float width = ui.TextWidth(text, 12.0f, FontWeight::Bold) + ui.S(16.0f);
  const Rect box{ui.Mouse().x + ui.S(12.0f), ui.Mouse().y + ui.S(6.0f), width, ui.S(22.0f)};
  ui.Draw().FillRect(box, theme.buttonOn.WithAlpha(230));
  ui.Draw().StrokeRect(box, theme.accent, ui.S(1.0f));
  ui.Label(box, text, {12.0f, theme.text, Align::Center, VAlign::Middle, FontWeight::Bold});
}

} // namespace reb::battle_ui
