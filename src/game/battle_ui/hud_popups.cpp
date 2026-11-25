// The command HUD's popups (tactical_combat.md §5.2, §5.4, §5.7): a doctrine setting's options, HUD options, the
// fleet-withdrawal confirmation, and the ghost of a roster drag.
#include <algorithm>

#include "battle_ui/command_hud.h"
#include "battle_ui/hud_draw.h"
#include "battle_ui/selection.h"

namespace reb::battle_ui
{

void CommandHud::DrawPopup(Ui& ui)
{
  if (m_popup.kind == PopupKind::None) {
    return;
  }
  const bool doctrinePopup = m_popup.kind == PopupKind::Field;
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
  const int current = (doctrine.mixed & mask) ? -1 : GetOption(field, m_subject.flight, doctrine.ship, doctrine.flight);
  const float width = ui.S(330.0f);
  const float textWidth = width - ui.S(16.0f) - ui.S(14.0f);
  const float helpLine = ui.Text().LineHeight(ui.S(10.5f)) + ui.S(1.0f);
  std::vector<std::vector<std::string>> helps;
  float height = ui.S(22.0f + 11.0f);
  for (int i = 0; i < count; ++i) {
    helps.push_back(WrapText(ui.Text(), Option(field, i).help, ui.S(10.5f), textWidth, 2));
    height += ui.S(24.0f) + helpLine * static_cast<float>(helps.back().size()) + ui.S(2.0f);
  }
  const Rect rect = PlacePopup(ui, width, height);
  m_popup.rect = rect;
  const Rect content = hud::PopupFrame(ui, Id("hud.popup.field"), rect, Describe(field).label);
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
      SetOption(field, m_subject.flight, i, edited.ship, edited.flight);
      SubmitDoctrine(edited, mask);
      ClosePopup();
      if (field == DoctrineField::Mission && edited.flight.mission == battle::FlightMission::Escort) {
        StartPick(PickMode::Protect);
      }
      return;
    }
    y += rowHeight + ui.S(2.0f);
  }
}

void CommandHud::DrawOptionsPopup(Ui& ui)
{
  const Theme& theme = ui.GetTheme();
  const float row = ui.S(22.0f);
  const float width = ui.S(340.0f);
  const float height = ui.S(22.0f + 11.0f) + ui.S(18.0f) + row * kAutoPauseEventCount + ui.S(8.0f)
                       + 2.0f * (ui.S(18.0f) + row + ui.S(8.0f));
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
