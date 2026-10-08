#pragma once

// Shared geography fixtures (PEO-096): a generated square costs about a second under
// the sanitizers, so the cases that read the same one share it (scenario budget, D-033).

#include "peo/core/geography.hpp"

namespace peo::test {

/// Seed 1's square (0,0) with the default params, generated once per test run.
inline const core::GeographyRegion& seed1_square00() {
    static const core::GeographyRegion square =
        core::generate_square(1, core::GeographyParams{}, core::SquareCoord{0, 0});
    return square;
}

} // namespace peo::test
