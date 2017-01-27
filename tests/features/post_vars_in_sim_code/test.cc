// Google test includes
#include "gtest/gtest.h"

// Autogenerated simulation code includess
#include "post_vars_in_sim_code_CODE/definitions.h"

// **NOTE** base-class for simulation tests must be
// included after auto-generated globals are includes
#include "../../utils/simulation_test_post_vars.h"

TEST_P(SimulationTestPostVars, AcceptableError)
{
  float err = Simulate(
    [](float t)
    {
        return (t > 1.1001) && (fmod(t-DT-(d+1)*DT+5e-5,1.0f) < 1e-4);
    });

    // Step simulation
    Step();
  }

  // Check total error is less than some tolerance
  EXPECT_LT(err, 3e-2);
}

#ifndef CPU_ONLY
auto simulatorBackends = ::testing::Values(true, false);
#else
auto simulatorBackends = ::testing::Values(false);
#endif

INSTANTIATE_TEST_CASE_P(SimCode,
                        SimulationTestPostVars,
                        simulatorBackends);