# Contextual crop enhancement and assembly

Use for large static textured art when whole-image generation cannot retain useful
detail at the chosen pack size. It is unnecessary for most 400×200 encyclopedia
images. Set the target integer scale and keep an unchanged original `S` throughout.

## First pass

1. Select a grid based on the source and actual tool output budget. A 2×2 grid can
   suffice; use the original workflow's 3×3 only when justified. Avoid splitting
   faces or repeated rigid structures when a whole-object crop is practical.
2. Partition into cores with explicit half-open source-coordinate boxes
   `[left, top, right, bottom)`. Pad each side by about 20% of the core dimension,
   clipped to source bounds. Record both core and padded boxes. Work in top-left
   visual coordinates, regardless of BMP storage row orientation.
3. Enhance each padded crop with inputs `[S = original context, C = target crop]`.
   Request the maximum useful supported native dimensions, not an invented 8K
   capability. Record actual returned dimensions, prompt, and artifact path before
   assembly. Independent calls may run concurrently where supported.
4. Start the master with the deterministic baseline at exact target dimensions.
   Register and blend accepted patches into it. Keep rejected patches on disk and
   leave their baseline pixels intact.

## Selective follow-up

Inspect the assembled master before another pass. Choose only salient regions with
an observable need: a portrait's face/costume, a ship's hull, terrain, material detail.
Use zero to eight padded ROIs as appropriate, not a fixed quota. Exclude quiet sky,
flat UI, text, emblems, and details too ambiguous to constrain.

Crop the current master, but continue to supply unchanged `S` as original context.
Record each ROI's coordinates in that master's space and its mapping to original
source space. A third pass, if justified, uses tighter defect-specific ROIs; keep
matching mechanical and facial features consistent across edits.

## Registration and blending

- Map padded boxes into the output master. Returned crop dimensions may differ:
  establish their scale explicitly rather than assuming native output is pack size.
- Use global similarity registration first, based on reliable correspondences to
  the prior accepted image/baseline. An exact-sized image can still have geometry
  drift. Review residuals at silhouettes and high-contrast landmarks; record the
  transform and acceptance decision. If correspondence is weak, retain prior pixels.
- Bounded smooth local correction is optional only with reliable correspondence.
  Never warp rigid ships, repeated panels, emblems, lettering, or UI geometry to
  rescue a bad generation. Reject that patch instead.
- Match broad exposure/color without transferring old fine texture. Use normalized
  overlap or multiband blending; feather roughly 15–25% of patch borders, with alpha
  reaching zero at interior patch boundaries. Do not fade the image's outer edge.
  Keep protected masks and original transparency authoritative. Normalize weights
  so overlaps do not darken and uncovered regions retain prior pixels.
- Inspect full-image and 100% overlap/focal crops after every assembled pass. Blending
  can hide a seam but cannot repair doubled edges, shifted features, inconsistent
  object design, or texture scale changes. Retry only the affected region (at most
  twice per defect), or restore prior content and report it.

If reliable registration, masks, or blending cannot be performed with available
tools, stop this mode and keep the whole-image/baseline result. Do not substitute a
plain resize-and-paste operation and describe it as registered enhancement.
