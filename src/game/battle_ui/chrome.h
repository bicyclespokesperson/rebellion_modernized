#pragma once
#include "battle_ui/widgets.h"

// The HUD's console chrome (tactical_combat.md §10.2), after the original game's consoles: TACTICAL.DLL's battle
// frame and side panels (brushed steel, bevelled edges, green-grid screens set into the metal) and the strategic
// windows (a title bar in the side's colours, LED readouts in black wells, steel buttons with a coloured lamp). It is
// drawn procedurally, so it is crisp at any HUD scale and needs no art from the installation. `s` is the UI scale.
namespace reb::battle_ui::chrome
{

// The theme for a side's consoles: the Alliance's warm steel, red title bars and red LEDs, or the Empire's blue steel,
// imperial-blue title bars and green LEDs; the colour-blind palette recolours the factions and bolts.
Theme SkinnedTheme(battle::Side side, bool colorBlind);

// A panel: a drop shadow, a bevelled steel rim with clipped corners and a smoked-glass body. `rim` in UI pixels.
void Plate(DrawList& draw, const Rect& rect, const Theme& theme, float s, float rim = 2.0f);
// The caption band along a plate's top edge, in the side's colours, with the side's emblem and the title. Returns
// the band's rect, so that the caller can place buttons in it.
Rect Caption(DrawList& draw, const Rect& plate, float height, std::string_view title, float textSize,
             const Theme& theme, float s);

enum class Face : uint8_t { Up, Hovered, Pressed, On, Disabled };
// The face for a button's state: disabled wins, then pressed, then on, then hovered.
Face FaceFor(bool hovered, bool pressed, bool on, bool enabled);
// A steel button cap; `On` lights its lamp slit.
void ButtonCap(DrawList& draw, const Rect& rect, Face face, const Theme& theme, float s);
// A recessed black well for readouts, gauges and check boxes.
void Well(DrawList& draw, const Rect& rect, const Theme& theme, float s);
// A gauge in a well: continuous, or `segments` LED blocks (off ones glow faintly) like the strategic screens' bars.
void Gauge(DrawList& draw, const Rect& rect, float fraction, Color fill, const Theme& theme, float s, int segments = 0);
// A round indicator lamp, lit with a glow or dark.
void Lamp(DrawList& draw, Point center, float radius, Color color, bool lit);
// An engraved line: a dark groove with a lit lower lip.
void Groove(DrawList& draw, Point from, Point to, const Theme& theme, float s);
// The side's emblem, filling a circle of `radius`: the Alliance starbird or the Imperial crest.
void Emblem(DrawList& draw, Point center, float radius, battle::Side side, Color color);
// The steel strip of the top bar, with a shadow cast onto the battle view below it.
void ConsoleBar(DrawList& draw, const Rect& rect, const Theme& theme, float s);
// An LED readout: `text` in the theme's readout colour over the unlit segments of `ghost` (such as "88:88"), in a
// well. `size` in device pixels.
void Readout(DrawList& draw, const Rect& rect, std::string_view text, std::string_view ghost, float size, Color color,
             const Theme& theme, float s, Align align = Align::Center);
// The big action button in the side's caption colours (Engage).
void CommandButton(DrawList& draw, const Rect& rect, Face face, const Theme& theme, float s);
// A small key cap with a label, for hotkeys and key reminders. `size` in device pixels.
void Keycap(DrawList& draw, const Rect& rect, std::string_view label, float size, const Theme& theme, float s);
// A ship card's shield and hull strips in one well.
void StatusBars(DrawList& draw, const Rect& rect, float shield, float hull, Color hullColor, const Theme& theme,
                float s);
// A slim notice plate (banners, prompts, toasts) with a coloured signal strip on its left edge.
void Notice(DrawList& draw, const Rect& rect, Color signal, const Theme& theme, float s, float fade = 1.0f);
// A medallion with the side's emblem embossed in the console metal.
void Medallion(DrawList& draw, Point center, float radius, const Theme& theme);
// Text with a one-pixel drop shadow, for captions and readouts over light or busy backgrounds.
void ShadowText(DrawList& draw, DrawText text, float s);

} // namespace reb::battle_ui::chrome
