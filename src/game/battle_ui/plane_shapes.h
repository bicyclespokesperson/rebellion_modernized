#pragma once
#include "battle_ui/camera.h"
#include "battle_ui/draw_list.h"

// Shapes that lie on the battle plane, drawn through the camera: true circles and sectors in the top-down view,
// projected polygons (ellipses) in the angled view. Centres and radii are world meters; angles are world radians
// (0 = +x, counter-clockwise).
namespace reb::battle_ui
{

void PlaneArc(DrawList& out, const Camera& camera, Vec2 center, float radius, float start, float sweep, Color color,
              const Stroke& stroke, Blend blend = Blend::Alpha);
void PlaneCircle(DrawList& out, const Camera& camera, Vec2 center, float radius, Color color, const Stroke& stroke,
                 Blend blend = Blend::Alpha);
// A filled annular sector (a pie slice when `inner` is 0, a ring or disc over a full turn).
void PlaneSector(DrawList& out, const Camera& camera, Vec2 center, float inner, float outer, float start, float sweep,
                 Color color, Blend blend = Blend::Alpha);
void PlaneDisc(DrawList& out, const Camera& camera, Vec2 center, float radius, Color color, Blend blend = Blend::Alpha);

} // namespace reb::battle_ui
