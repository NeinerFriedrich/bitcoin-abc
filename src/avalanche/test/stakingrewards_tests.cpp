// Copyright (c) 2023 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <avalanche/avalanche.h>
#include <avalanche/processor.h>
#include <chainparams.h>
#include <consensus/activation.h>
#include <net_processing.h>
#include <policy/block/stakingrewards.h>

#include <test/util/blockindex.h>
#include <test/util/setup_common.h>

#include <boost/test/unit_test.hpp>

struct StakingRewardsActivationTestingSetup : public TestingSetup {
    void checkStakingRewardsActivation(const std::string &net,
                                       const bool expectActivation) {
        SelectParams(net);
        const Consensus::Params &params = Params().GetConsensus();

        gArgs.ForceSetArg("-avalanche", "1");

        bilingual_str error;
        g_avalanche = avalanche::Processor::MakeProcessor(
            *m_node.args, *m_node.chain, m_node.connman.get(),
            *Assert(m_node.chainman), m_node.mempool.get(), *m_node.scheduler,
            error);

        BOOST_CHECK(g_avalanche);
        BOOST_CHECK(isAvalancheEnabled(gArgs));

        // Before Cowperthwaite activation
        const auto activation = gArgs.GetIntArg(
            "-cowperthwaiteactivationtime", params.cowperthwaiteActivationTime);
        SetMockTime(activation - 1000000);

        std::array<CBlockIndex, 12> blocks;
        for (size_t i = 1; i < blocks.size(); ++i) {
            blocks[i].pprev = &blocks[i - 1];
        }

        SetMTP(blocks, activation - 1);
        BOOST_CHECK(!IsStakingRewardsActivated(params, &blocks.back()));

        SetMTP(blocks, activation);
        BOOST_CHECK_EQUAL(IsStakingRewardsActivated(params, &blocks.back()),
                          expectActivation);

        SetMTP(blocks, activation + 1);
        BOOST_CHECK_EQUAL(IsStakingRewardsActivated(params, &blocks.back()),
                          expectActivation);

        // If avalanche is disabled, staking rewards are disabled
        gArgs.ForceSetArg("-avalanche", "0");
        BOOST_CHECK(!isAvalancheEnabled(gArgs));
        BOOST_CHECK(!IsStakingRewardsActivated(params, &blocks.back()));

        gArgs.ForceSetArg("-avalanche", "1");
        BOOST_CHECK(isAvalancheEnabled(gArgs));
        BOOST_CHECK_EQUAL(IsStakingRewardsActivated(params, &blocks.back()),
                          expectActivation);

        // If g_avalanche is null, staking rewards are disabled
        g_avalanche.reset(nullptr);
        BOOST_CHECK(!g_avalanche);
        BOOST_CHECK(!IsStakingRewardsActivated(params, &blocks.back()));

        gArgs.ClearForcedArg("-avalanche");
    }
};

BOOST_FIXTURE_TEST_SUITE(stakingrewards_tests,
                         StakingRewardsActivationTestingSetup)

BOOST_AUTO_TEST_CASE(isstakingrewardsactivated) {
    checkStakingRewardsActivation("regtest", false);
    checkStakingRewardsActivation("test", false);
    checkStakingRewardsActivation("main", true);
}

BOOST_AUTO_TEST_SUITE_END()
