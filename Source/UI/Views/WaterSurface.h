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

    The scheme is the standard two-buffer discretisation of the wave equation:

        next = (sum of four neighbours) / 2 - prev,  then damped

    It is unconditionally cheap — a couple of multiply-adds per cell — and
    stable as long as the damping stays below one. Everything is plain floats
    on a fixed grid allocated once in prepare(); step() never allocates.

    Coordinates are normalised 0..1 across the grid so callers never have to
    know the resolution.
*/
class WaterSurface
{
public:
    WaterSurface() = default;

    /** Allocates the grid. Call from the message thread. */
    void prepare (int gridSize);

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

        /** Lateral drift, -1..1. Advects the whole field sideways, which is
            what makes a current look like it is carrying the water. */
        float drift = 0.0f;

        /** Extra smoothing between neighbours — deep, cold, viscous water. */
        float viscosity = 0.0f;
    };

    void setParams (const Params& p) noexcept { params = p; }
    const Params& getParams() const noexcept { return params; }

    /** Strikes the surface. Position is normalised 0..1; radius is a fraction
        of the grid; strength is signed, so a negative value pulls the surface
        down the way a real impact does before it rebounds. */
    void impact (float nx, float ny, float radius, float strength) noexcept;

    /** Raises a standing pattern rather than a travelling one — the
        vibrational, cymatic look that RIPPLE asks for. */
    void exciteStanding (float nx, float ny, float wavelength, float strength) noexcept;

    /** Advances the simulation. dt is seconds; substeps keep behaviour stable
        and frame-rate independent. */
    void step (float dt, RandomGenerator& rng) noexcept;

    int getSize() const noexcept { return size; }
    bool isReady() const noexcept { return size > 0; }

    /** Current height at a cell. Out-of-range reads return zero, so callers
        may sample the neighbourhood of an edge cell without bounds checks. */
    float heightAt (int x, int y) const noexcept
    {
        if (x < 0 || y < 0 || x >= size || y >= size)
            return 0.0f;

        return current[(size_t) (y * size + x)];
    }

    const float* raw() const noexcept { return current.data(); }

    /** Largest absolute height anywhere, updated each step. Lets the renderer
        skip work entirely when the water is flat. */
    float getEnergy() const noexcept { return energy; }

private:
    void advect (float amount) noexcept;

    int size = 0;
    std::vector<float> current, previous, scratch;
    Params params;
    float energy = 0.0f;
    float accumulator = 0.0f;
    float driftCarry = 0.0f;
};

} // namespace ripples
