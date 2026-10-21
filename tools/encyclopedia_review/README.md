# Encyclopedia extraction review

From the repository root:

```sh
uv run tools/review_encyclopedia.py
```

Open http://127.0.0.1:8765. No dependencies, model inference, or rebuild is needed.
The server binds only to loopback. `--port`, `--cache-dir`, and `--out` override
its defaults. It needs the existing encyclopedia builder's `inventory.json`,
`EDATA.nnn/source.png`, and `EDATA.nnn/cutout.png` outputs.

The initial population is portraits, capital ships, and fighters. Other sets are
available in the selector; “All icon categories” excludes mission artwork.
Explicitly skipped overrides and missing image pairs are excluded. Each image
appears once per set, regardless of how many game icons use it. This reviews the
cached extraction before downstream portrait cropping and palette conversion.

Each set gets an independent, saved random permutation and independent grades.
Grade all 10 images before advancing; the final batch may have fewer than 10.
Click a selected grade again to clear it. Missing features, extra background, and
both count as failures. Click an image to enlarge both sides; change the cutout
background to inspect edges.

Estimates use fully graded batches, excluding ungraded and partially graded
batches. The displayed 95% Wilson interval is an approximate binomial interval
without finite-population correction. The affected-image range multiplies those
limits by the population and rounds outward. A fully reviewed population shows
the measured rate. Small samples have wide uncertainty; stopping because a
particular failure rate looks interesting can bias the result. Choose a sample
size (such as 20) before grading. Rates apply only to the selected set.

Grades and random order are written atomically under the ignored directory
`work/hd/encyclopedia/reviews/`. The app displays the exact path and can export all
sets as JSON. A fingerprint of metadata and the actual source/cutout PNG bytes
separates reviews after an extraction changes; prior review files remain intact.
The last visited page is remembered in browser local storage. Do not run multiple
servers against the same output directory or grade from multiple tabs at once.

Only app source belongs in Git. Images, manifests, and review results stay local.
