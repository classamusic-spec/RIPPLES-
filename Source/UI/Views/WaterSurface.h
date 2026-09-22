#pragma once

#include "Utilities/MathUtils.h"
#include "Utilities/RandomGenerator.h"

#include <vector>

namespace ripples
{

/**
    A height field of water.

    This is a real damped wave simulation rather than a set of drawn circles.
    That distinction is the whole point: concentric rings can be spaced and
    faded to look like ripples, but they cannot interfere with each other,
    reflect off the edge of the bowl, or leave a wake behind a moving finger.
    A height field does all three for free, which is what actually reads as
    water.

    The scheme is the standard two-buffer discretisation of the wave equation
    with an isotropic nine-point laplacian, so a circular impact spreads as a
    circle rather than a diamond. It is unconditionally cheap — a handful of
    multiply-adds per cell — and stable as long as the tension stays under the
    von Neumann limit. Everything is plain floats on a fixed grid allocated
    once in prepare(); step() never allocates.

    The grid may be RECTANGULAR. Cells are square, so as long as the caller
    sizes the grid to the aspect ratio of the area it fills, a ripple that is
    round in grid space is round on screen. Coordinates handed in are
    normalised 0..1 across each axis, and a radius or wavelength is a fraction
    of the SHORTER axis, so a strike is the same physical size whatever the
    aspect.
*/
class WaterSurface
{
public:
    WaterSurface() = default;

    /** Allocates a square grid. */
    void prepare (int gridSize) { prepare (gridSize, gridSize); }

    /** Allocates a rectangular grid, width by height cells. */
    void prepare (int gridWidth, int gridHeight);

    /** Flattens the water without reallocating. */
    void reset();

    struct Params
    {
        /** How long a disturbance survives. Near 1 is glassy and rings for
            seconds; lower is choppy and dies quickly. */
        float damping = 0.992f;

        /** Propagation speed, 0..1. Low is syrupy, high is taut and fast. */
        float tension = 0.55f;

        /** Spontaneous agitation — the CHAOS axis. Sprinkles small random
            impulses so the surface never settles. */
        float chop = 0.0f;

        /** Extra smoothing between neighbours — deep, cold, viscous water. */
        float viscosity = 0.0f;
    };

    void setParams (const Params& p) noexcept { params = p; }
    const Params& getParams() const noexcept { return params; }

    /** Strikes the surface. Position is normalised 0..1 across each axis;
        radius is a fraction of the shorter axis; strength is signed, so a
        negative value pulls the surface down the way a real impact does before
        it rebounds. */
    void impact (float nx, float ny, float radius, float strength) noexcept;

    /** Selects the standing mode shape — a radial cymatic pattern centred on
        (nx, ny) with the given wavelength. The pattern is cached, so calling
        this every frame with unchanged arguments costs nothing. */
    void setStandingMode (float nx, float ny, float wavelength) noexcept;

    /** Adds the cached standing mode to the surface, signed. Driving this
        continuously in step with an oscillator is what makes a pattern STAND:
        struck once, any pattern immediately starts travelling and is gone. One
        multiply-add per cell, so it is cheap enough to run every frame. */
    void driveStanding (float amount) noexcept;

    /** Raises a standing pattern once. Shorthand for setStandingMode followed
        by driveStanding. */
    void exciteStanding (float nx, float ny, float wavelength, float strength) noexcept;

    /** Advances the simulation. dt is seconds; substeps keep behaviour stable
        and frame-rate independent. */
    void step (float dt, RandomGenerator& rng) noexcept;

    int  getWidth()  const noexcept { return width; }
    int  getHeight() const noexcept { return height; }
    int  getSize()   const noexcept { return width; }   // back-compat: square grids
    bool isReady()   const noexcept { return width > 0 && height > 0; }

    /** Current height at a cell. Out-of-range reads return zero, so callers
        may sample the neighbourhood of an edge cell without bounds checks. */
    float heightAt (int x, int y) const noexcept
    {
        if (x < 0 || y < 0 || x >= width || y >= height)
            return 0.0f;

        return current[(size_t) (y * width + x)];
    }

    const float* raw() const noexcept { return current.data(); }

    /** Largest absolute height anywhere, updated each step. Lets the renderer
        skip work entirely when the water is flat. */
    float getEnergy() const noexcept { return energy; }

private:
    int width = 0, height = 0;
    std::vector<float> current, previous, standing;
    float standingX = -1.0f, standingY = -1.0f, standingWavelength = -1.0f;
    Params params;
    float energy = 0.0f;
    float accumulator = 0.0f;
};

} // namespace ripples
