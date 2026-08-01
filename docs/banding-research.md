# Walltz Banding Research: Complete Analysis

## Summary of Investigation
- Analyzed walltz source code (WallpaperProcessor.cpp): blur pipeline, saturation boost
- Analyzed Apple Backgroundifier source (NSImageEffects.m): vImage blur + saturation
- Built numerical simulation of the full pipeline with sigma=45, factor=1.8
- Tested 5 dithering strategies on synthetic photographic gradient

## 1. Root Cause of "Waves" / Contour Banding

### Primary mechanism: Quantize-then-saturate pipeline

The walltz pipeline does:
```
float blur → quantize_to_8bit → boostSaturation_on_8bit
```

**Step 1: Gaussian blur in float → 8-bit quantize.**
At sigma=45, neighbor pixels differ by as little as **0.012 units** in smooth gradient regions. Quantizing to 8-bit (step size = 1.0) collapses ~50 adjacent pixels to the SAME value, creating uniform contour blocks. The G channel loses the most — only **53 unique values** remain out of 256 (R:169, B:125).

**Step 2: Saturation boost on 8-bit data (the critical problem).**
`boostSaturation` computes:
```cpp
int gray = (r + g + b) / 3;          // integer division — truncates!
int nr = int(gray + (r - gray) * 1.8); // int() cast — truncates!
```

Two quantization artifacts compound:

1. **Gray-jump discontinuities**: `gray = (r+g+b)/3` changes when the sum crosses a multiple-of-3 boundary. Since the blur produces near-identical neighbor pixels, small input variations push the sum across 3n boundaries. When gray jumps by 1, **ALL THREE output channels jump simultaneously**, creating a highly visible contour stripe.

2. **Variable-width bands**: The walltz saturation produces bands with **highly non-uniform width** — standard deviation up to 13.1px vs mean 14.1px (G band). This irregular pattern of narrow bands (some 1px wide) mixed with wide bands (up to 45px) creates a **"wave" visual** rather than clean, evenly-spaced contour bands.

**Quantification** (from simulation):
| Stage | R bands | G bands | B bands | Mean band width |
|-------|---------|---------|---------|----------------|
| Blurred 8-bit | 50 | 16 | 36 | 11-35 px, uniform |
| After walltz sat | 65 | 40 | 51 | 9-14 px, non-uniform |
| Float sat + quantize | 83 | 20 | 74 | 7-27 px, uniform |

### Why true Gaussian makes waves MORE visible than box-cascade

The box-cascade (3× box blur used by Apple Backgroundifier and earlier walltz) has a non-ideal frequency response that introduces ~1% residual ringing/Gibbs-like artifacts. This **accidental noise dithers** the contour boundaries, making them less visible.

The true Gaussian has a mathematically perfect frequency response. Output pixels that should be identical **ARE** identical. The contour blocks are perfectly uniform. This makes boundaries **MAXIMALLY visible** — the contour edges are razor-sharp transitions between uniform blocks.

**Counterintuitive result**: An imperfect blur produced better-looking output because its errors masked the quantization. Fixing the blur revealed (and amplified) the downstream quantization problem.

### Numerical evidence

```
Gray-jump coincidence (simulation):
  Gray value changes in blurred 8-bit: 34 positions
  R output jumps coinciding with gray jumps: 33/65 (51%)
  G output jumps coinciding with gray jumps: 29/40 (72%)
  B output jumps coinciding with gray jumps: 27/51 (53%)
```

Over 50% of all saturation-output transitions are triggered by gray integer-division boundaries, not by actual color changes in the image.

## 2. Dithering Strategy Analysis

### Tested approaches on synthetic gradient (sigma=45, 1.8x sat)

| Method | Mean band width | Narrow bands (≤2px) | High-freq energy | Deterministic | Parallel | Green splatters |
|--------|----------------|-------------------|-----------------|---------------|----------|-----------------|
| **No dither** (current) | 9-14 px | 0 | 0.021 | ✓ | ✓ | 0% |
| **Ordered dither (Bayer 8×8)** | **1.3 px** | **~400** | 0.048 | ✓ | ✓ | **0%** |
| **Floyd-Steinberg** | 2.3-4.2 px | 54-220 | 0.021 | ✓ | ✗ | 0% |
| Independent ±1 noise | banded | ~0 | - | ✗ | ✓ | 0% |
| Shared ±1 noise | banded | ~0 | - | ✗ | ✓ | 0% |

**Key findings:**
- **Ordered dither (Bayer 8×8) applied during float saturation quantize is the best approach**: 438-454 band transitions (nearly every pixel is unique), fully parallelizable, deterministic, no color shifts
- **Floyd-Steinberg error diffusion** also effective but inherently serial (scanline order) with risk of scanline artifacts at high-contrast edges
- **Independent per-channel noise** did NOT produce green splatters in our test at any noise level (±1 to ±3). The "green splatters" reported previously may be specific to certain image content or a different noise implementation
- **Float saturation + ordered dither** eliminates ALL visible banding in simulation

### Recommended dither: Ordered dither (Bayer 8×8)

Advantages for this use case:
1. **Fully parallel** — each pixel independent, trivial to vectorize/GPU
2. **Deterministic** — no random number generation, reproducible output
3. **No color shift** — same threshold applied to all 3 channels preserves hue
4. **Simple implementation** — single lookup into a 64-byte Bayer matrix
5. **No green splatters** — correlated noise across channels

Implementation:
```cpp
// Bayer 8×8 threshold matrix (normalized to [-0.5, 0.5])
static const float bayer8[64] = {
    -0.4921875, -0.2421875, -0.3046875, -0.0546875, -0.4453125, -0.1953125, -0.2578125, -0.0078125,
    -0.0078125, -0.2578125, -0.1953125, -0.4453125, -0.0546875, -0.3046875, -0.2421875, -0.4921875,
    // ... (full 8×8 matrix)
};

// During saturation compute in float:
float th = bayer8[(y & 7) * 8 + (x & 7)];
int nr = (int)(sr + th);  // or use roundf(sr + th)
```

## 3. How Professional Tools Prevent This

### Apple Backgroundifier (NSImageEffects.m) — SOURCE CODE ANALYZED

Uses **vImage** (Accelerate framework) — 8-bit ARGB pipeline:

1. **Blur**: Three box blurs via `vImageBoxConvolve_ARGB8888` → box-cascade approximation of Gaussian
   - Uses the SVG spec formula: `radius = floor((inputRadius * 3 * sqrt(2*PI) / 4 + 0.5) / 2)`
   - Three successive box blurs approximate a Gaussian to ~3% accuracy
   - The approximation error acts as natural dither

2. **Saturation**: Matrix multiply via `vImageMatrixMultiply_ARGB8888`
   - Uses **proper luminance-weighted saturation matrix** (Rec. 709: 0.2126R, 0.7152G, 0.0722B)
   - Fixed-point conversion uses **roundf()** (not truncation): `saturationMatrix[i] = (int16_t)roundf(fp[i] * 256)`
   - Divisor=256 gives 8 bits of fractional precision during computation
   - vImage internal pipeline likely uses higher intermediate precision

3. **No explicit dithering step** — relies on:
   - The box-cascade's inherent approximation noise
   - Proper rounding (roundf) in matrix coefficients
   - Saturating in the same 8-bit pass (no intermediate quantize)

### Photoshop

1. **Recommends 16-bit or 32-bit working mode** for any operation involving blur or color adjustments
2. In 16-bit mode: 15 bits per channel (32,768 levels) vs 8-bit's 256 — 128× more precision
3. In 32-bit mode: full float32 pipeline, effectively no quantization during intermediate steps
4. "Convert to Smart Object" preserves original data through non-destructive filters
5. Common professional technique: Add 1-2% monochromatic noise BEFORE blur to dither the gradient — the noise is blurred away but breaks up contouring
6. Gaussian Blur filter operates at the document's bit depth (8, 16, or 32 bpc)
7. **Does NOT apply dither in the Gaussian blur itself** — relies entirely on working at sufficient bit depth

### GIMP (since 2.10)

1. **GEGL processing engine** operates in **32-bit floating point** per channel by default
2. Image precision can be set: 8-bit integer / 16-bit integer / 16-bit float / 32-bit float
3. Gaussian blur (GEGL operation) works at whatever precision the image is set to
4. **In 8-bit mode, GIMP's Gaussian blur also produces banding** — confirmed by community reports (GitLab issue #14844)
5. The GIMP-recommended workflow: work in 32-bit float, then dither to 8-bit on final export
6. **"Dither or posterize" filter** can be applied as a post-processing step for banding reduction — uses spatial dithering similar to ordered dither

### Summary Table

| Tool | Internal precision | Blur method | Saturation method | Dithering |
|------|-------------------|-------------|-------------------|-----------|
| Apple vImage | 8-bit + 8-bit frac | 3× box blur | Matrix mult, roundf(×256) | None explicit (box noise) |
| Apple Core Image | float16 on GPU | True Gaussian (CI) | CIFilter chain in float | None (float pipeline) |
| Photoshop (8-bit) | 8-bit | Gaussian at doc depth | In doc precision | None (banding in 8-bit) |
| Photoshop (16-bit) | 15+1 bit | Gaussian at doc depth | In doc precision | None needed (65536 levels) |
| GIMP 2.10+ | 32-bit float (GEGL) | GEGL gaussian-blur | GEGL op in float | Optional dither filter |

## 4. Is 8-bit Fundamentally Insufficient?

**Yes, for sigma=45 gradient it is fundamentally insufficient.**

From simulation:
- Minimum pixel-to-pixel difference in blurred gradient: **0.012 units**
- 8-bit quantization step: **1.0 unit**
- Bits needed to represent 0.012 difference within 0-255 range: **14.4 bits**
- 85-97% of adjacent pixels in the saturated gradient differ by < 0.5 units and thus map to the same 8-bit value

For sigma=40-50:
- **12-bit** (4096 levels) would be the minimum for artifact-free output
- **16-bit** (65536 levels) provides comfortable margin
- **32-bit float** eliminates all quantization concerns

Since walltz outputs 8-bit PNG, the only practical approach is to dither during the final quantize.

## 5. Best Fix for walltz

### Option A (RECOMMENDED): Move saturation into float pipeline + ordered dither

```
Current: float blur → 8-bit quantize → 8-bit saturation boost → output
Fixed:   float blur → float saturation → ordered-dithered quantize to 8-bit → output
```

Changes needed in `WallpaperProcessor.cpp`:
1. `boostSaturation` should accept `float*` buffer, compute in float with proper gray
2. Apply ordered dither during the final `floatToImage` step
3. This is a ~20-line change

### Option B (Simpler): Post-hoc ordered dither on 8-bit output

Keep current pipeline, add ordered dither as final step. Less effective but minimal code change.

### Option C (Minimal): Fix gray computation

Change `gray = (r+g+b)/3` to use proper luminance weights:
```cpp
int gray = (299*r + 587*g + 114*b) / 1000;
```
And use rounding instead of truncation:
```cpp
int nr = std::clamp((int)(gray + (r - gray) * factor + 0.5), 0, 255);
```
This improves color accuracy but doesn't eliminate banding.

## 6. Key References

- Apple NSImageEffects.m (vImage blur + saturation): `/var/home/pavel/src/walltz/backgroundifier-public/Backgroundifier/External/Modified/NSImageEffects.m`
- walltz source: `/var/home/pavel/src/walltz/walltz/src/WallpaperProcessor.cpp`
- Bart Wronski, "Dithering part three – real world 2D quantization dithering" (bartwronski.com)
- GIMP precision docs (docs.gimp.org/2.10/nl/gimp-image-precision.html)
- Greg Benz, "8, 12, 14 vs 16-bit depth: What do you really need?!" (gregbenzphotography.com)
- GIMP GitLab issue #14844 — "Lack of precision in gaussian blur"
- Adobe community: "Banding when doing gaussian blur in 16-bit black and white mode"
- Simulation outputs in `/tmp/walltz_analysis/`
