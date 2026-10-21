// Renders a battle_ui::DrawList with SDL_Renderer: shapes and text as batched SDL_RenderGeometry triangles over one
// glyph atlas (anti-aliased with a one-pixel alpha fringe), sprites as textured quads.
#pragma once
#include <SDL3/SDL.h>

#include <vector>

#include "battle_ui/draw_list.h"
#include "port/native/space_battle/sprite_source.h"
#include "port/native/space_battle/text_atlas.h"

namespace reb::native::space_battle
{

class DrawExecutor
{
  public:
  DrawExecutor(TextAtlas& text, SpriteSource& sprites);

  // Draws onto the renderer's current target; leaves the clip rectangle cleared.
  void Execute(SDL_Renderer* renderer, const battle_ui::DrawList& list);

  private:
  void Batch(SDL_Texture* texture, battle_ui::Blend blend); // starts a new batch when the texture or blend changes
  void Flush();
  void Vertex(battle_ui::Point p, SDL_FColor color);
  void Triangle(battle_ui::Point a, battle_ui::Point b, battle_ui::Point c, SDL_FColor ca, SDL_FColor cb,
                SDL_FColor cc);

  void Draw(const battle_ui::DrawSprite& sprite);
  void Draw(const battle_ui::DrawRect& rect);
  void Draw(const battle_ui::DrawPolyline& line);
  void Draw(const battle_ui::DrawArc& arc);
  void Draw(const battle_ui::DrawSector& sector);
  void Draw(const battle_ui::DrawPolygon& polygon);
  void Draw(const battle_ui::DrawTriangles& triangles);
  void Draw(const battle_ui::DrawText& text);
  void Draw(const battle_ui::DrawClip& clip);

  void StrokePath(const std::vector<battle_ui::Point>& points, bool closed, battle_ui::Color color,
                  const battle_ui::Stroke& stroke, battle_ui::Blend blend);
  void Polyline(const std::vector<battle_ui::Point>& points, bool closed, SDL_FColor color, float width);
  void FilledRect(float x0, float y0, float x1, float y1, SDL_FColor color);

  TextAtlas& m_text;
  SpriteSource& m_sprites;
  SDL_Renderer* m_renderer = nullptr;
  std::vector<SDL_Vertex> m_vertices;
  SDL_Texture* m_texture = nullptr; // null: the text atlas (texel coordinates until the flush)
  battle_ui::Blend m_blend = battle_ui::Blend::Alpha;
  SDL_FPoint m_white{};
};

} // namespace reb::native::space_battle
