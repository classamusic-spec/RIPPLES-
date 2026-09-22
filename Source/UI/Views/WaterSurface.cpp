#include "UI/Views/WaterSurface.h"
#include "Utilities/MathUtils.h"

#include <algorithm>
#include <cmath>

namespace ripples
{

namespace
{
    // Sub-stepping keeps the water behaving the same whether the editor is
    // running at 60fps or stuttering at 20. Without it a dropped frame makes
    // the surface visibly lurch.
    constexpr float kFixedStep     = 1.0f / 120.0f;
    constexpr int   kMaxSubsteps   = 4;

    // The wave equation goes unstable as the neighbour weight approaches 1.
    constexpr float kMinTension    = 0.18f;
    constexpr float kMaxTension    = 0.47f;   // 0.5 is the stability limit; stay under it

    constexpr float kMinDamping    = 0.9000f;
    constexpr float kMaxDamping    = 0.9985f;

    // Below this the surface is treated as flat and stepping is skipped.
    constexpr float kFlatThreshold = 1.0e-4f;

    // Isotropic nine-point laplacian weights: 2/3 orthogonal, 1/6 diagonal,
    // 10/3 at the centre.
    constexpr float kOrthWeight    = 2.0f / 3.0f;
    constexpr float kDiagWeight    = 1.0f / 6.0f;
    constexpr float kCentreWeight  = 10.0f / 3.0f;

    constexpr float kChopRate      = 26.0f;   // impulses per second at chop = 1
    constexpr float kChopStrength  = 0.11f;
}

//==============================================================================
void WaterSurface::prepare (int gridSize)
{
    size = math::clamp (gridSize, 16, 512);

    const size_t n = (size_t) size * (size_t) size;
    current.assign (n, 0.0f);
    previous.assign (n, 0.0f);
    scratch.assign (n, 0.0f);

    energy = 0.0f;
    accumulator = 0.0f;
    driftCarry = 0.0f;
}

void WaterSurface::reset()
{
    std::fill (current.begin(), current.end(), 0.0f);
    std::fill (previous.begin(), previous.end(), 0.0f);
    energy = 0.0f;
    accumulator = 0.0f;
    driftCarry = 0.0f;
}

//==============================================================================
void WaterSurface::impact (float nx, float ny, float radius, float strength) noexcept
{
    if (size <= 0)
        return;

    const float cx = nx * (float) size;
    const float cy = ny * (float) size;
    const float r  = std::max (1.0f, radius * (float) size);
    const float r2 = r * r;

    const int x0 = std::max (0, (int) std::floor (cx - r));
    const int x1 = std::min (size - 1, (int) std::ceil (cx + r));
    const int y0 = std::max (0, (int) std::floor (cy - r));
    const int y1 = std::min (size - 1, (int) std::ceil (cy + r));

    for (int y = y0; y <= y1; ++y)
    {
        for (int x = x0; x <= x1; ++x)
        {
            const float dx = (float) x + 0.5f - cx;
            const float dy = (float) y + 0.5f - cy;
            const float d2 = dx * dx + dy * dy;

            if (d2 > r2)
                continue;

            // A raised cosine, so the dent has no hard edge to ring against.
            const float falloff = 0.5f + 0.5f * std::cos (std::sqrt (d2 / r2) * math::pi);

            current[(size_t) (y * size + x)] += strength * falloff;
        }
    }

    energy = std::max (energy, std::abs (strength));
}

void WaterSurface::exciteStanding (float nx, float ny, float wavelength, float strength) noexcept
{
    if (size <= 0)
        return;

    const float cx = nx * (float) size;
    const float cy = ny * (float) size;
    const float k  = math::twoPi / std::max (2.0f, wavelength * (float) size);

    // A radial standing pattern: concentric crests that do not travel. This is
    // the cymatic look -- a surface vibrating in place rather than a ripple
    // spreading out from a strike.
    const float reach = (float) size * 0.55f;
    const float reach2 = reach * reach;

    for (int y = 0; y < size; ++y)
    {
        for (int x = 0; x < size; ++x)
        {
            const float dx = (float) x + 0.5f - cx;
            const float dy = (float) y + 0.5f - cy;
            const float d2 = dx * dx + dy * dy;

            if (d2 > reach2)
                continue;

            const float d = std::sqrt (d2);
            const float envelope = 0.5f + 0.5f * std::cos (d / reach * math::pi);

            current[(size_t) (y * size + x)] += strength * envelope * std::cos (d * k);
        }
    }

    energy = std::max (energy, std::abs (strength));
}

//==============================================================================
void WaterSurface::advect (float amount) noexcept
{
    if (std::abs (amount) < 1.0e-4f)
        return;

    // Whole-cell shifts only: accumulate the fractional part and move when it
    // reaches a cell. Resampling every frame for a sub-pixel slide would blur
    // the field into soup within a few seconds.
    driftCarry += amount;

    const int shift = (int) driftCarry;

    if (shift == 0)
        return;

    driftCarry -= (float) shift;

    const int s = math::clamp (shift, -4, 4);

    // Both buffers move together. Shifting only the current one shears the two
    // halves of the leapfrog apart, which injects a step discontinuity into the
    // very next iteration and shows up as a hard vertical seam.
    for (auto* buffer : { &current, &previous })
    {
        for (int y = 0; y < size; ++y)
        {
            float* row = &(*buffer)[(size_t) (y * size)];
            std::copy (row, row + size, &scratch[(size_t) (y * size)]);

            const float* src = &scratch[(size_t) (y * size)];

            for (int x = 0; x < size; ++x)
            {
                int from = x - s;
                from = math::clamp (from, 0, size - 1);   // clamp, so edges do not wrap
                row[x] = src[from];
            }
        }
    }
}

void WaterSurface::step (float dt, RandomGenerator& rng) noexcept
{
    if (size <= 0)
        return;

    accumulator += math::clamp (dt, 0.0f, 0.1f);

    int steps = 0;

    while (accumulator >= kFixedStep && steps < kMaxSubsteps)
    {
        accumulator -= kFixedStep;
        ++steps;

        const float tension = math::clamp (kMinTension + params.tension * (kMaxTension - kMinTension),
                                           kMinTension, kMaxTension);

        // Viscosity damps the fast ripples harder than the slow swells, which
        // is what makes deep water look heavier rather than merely quieter.
        const float damping = math::clamp (params.damping, kMinDamping, kMaxDamping)
                                * (1.0f - 0.03f * math::clamp (params.viscosity, 0.0f, 1.0f));

        const int w = size;

        for (int y = 1; y < w - 1; ++y)
        {
            const float* cRow = &current[(size_t) (y * w)];
            const float* cUp  = &current[(size_t) ((y - 1) * w)];
            const float* cDn  = &current[(size_t) ((y + 1) * w)];
            float*       pRow = &previous[(size_t) (y * w)];

            for (int x = 1; x < w - 1; ++x)
            {
                const float orth = cRow[x - 1] + cRow[x + 1] + cUp[x] + cDn[x];
                const float diag = cUp[x - 1] + cUp[x + 1] + cDn[x - 1] + cDn[x + 1];

                // Discretised wave equation:
                //     u(t+1) = 2u - u(t-1) + c^2 * laplacian
                //
                // The laplacian is the ISOTROPIC nine-point stencil:
                //     (2/3) * orthogonal + (1/6) * diagonal - (10/3) * u
                //
                // The obvious four-neighbour version is cheaper and wrong for
                // this job: it propagates faster along the axes than across the
                // diagonals, so a circular impact spreads as a diamond and the
                // interference pattern comes out of the shader as hard-edged
                // rectangular patches. That is not a rendering artefact and no
                // amount of filtering hides it -- the wavefronts really are
                // square. Weighting the diagonals restores a round wave.
                //
                // Gathered, with c^2 = tension:
                float next = orth * (kOrthWeight * tension)
                               + diag * (kDiagWeight * tension)
                               + cRow[x] * (2.0f - kCentreWeight * tension)
                               - pRow[x];

                next *= damping;

                // The nine-point stencil's largest eigenvalue is 20/3, so von
                // Neumann stability needs c^2 <= 3/5; the tension range caps it
                // well below that. This clamp is a backstop only, so a
                // pathological parameter can never put NaN on screen.
                pRow[x] = math::clamp (math::sanitise (next), -4.0f, 4.0f);
            }
        }

        // The rim is a wall: zero the border so waves reflect off the bowl
        // instead of leaking out and vanishing.
        for (int i = 0; i < w; ++i)
        {
            previous[(size_t) i] = 0.0f;
            previous[(size_t) ((w - 1) * w + i)] = 0.0f;
            previous[(size_t) (i * w)] = 0.0f;
            previous[(size_t) (i * w + w - 1)] = 0.0f;
        }

        current.swap (previous);
    }

    if (steps == 0)
        return;

    // --- lateral current ----------------------------------------------------
    advect (params.drift * dt * (float) size * 0.35f);

    // --- chop: the CHAOS axis never lets the surface settle -----------------
    if (params.chop > 0.001f)
    {
        const float expected = params.chop * kChopRate * dt;
        int hits = (int) expected;

        if (rng.nextFloat() < expected - (float) hits)
            ++hits;

        for (int i = 0; i < std::min (hits, 8); ++i)
        {
            const float ang = rng.nextFloat() * math::twoPi;
            const float rad = std::sqrt (rng.nextFloat()) * 0.46f;

            // Wide enough to span several cells: a one-cell impulse shades as
            // a pinpoint of glitter rather than as a dimple in a liquid.
            impact (0.5f + std::cos (ang) * rad,
                    0.5f + std::sin (ang) * rad,
                    0.030f + rng.nextFloat() * 0.035f,
                    (rng.nextBool (0.5f) ? 1.0f : -1.0f) * kChopStrength * params.chop);
        }
    }

    // --- track energy so the renderer can skip a flat surface ---------------
    float peak = 0.0f;
    const size_t n = current.size();

    for (size_t i = 0; i < n; i += 7)        // sparse sample; this is only a hint
        peak = std::max (peak, std::abs (current[i]));

    energy = peak;

    if (energy < kFlatThreshold)
        energy = 0.0f;
}

} // namespace ripples
