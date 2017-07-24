// Copyright (c) 2017 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "script/script.h"
#include "test/test_bitcoin.h"

#include "consensus/validation.h"
#include "main.h"
#include "options.h"

#include <boost/test/unit_test.hpp>

#include <string>
#include <vector>

BOOST_FIXTURE_TEST_SUITE(antireplay_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(test_is_commitment) {
    std::vector<unsigned char> data{};

    // Empty commitment.
    auto s = CScript() << OP_RETURN << data;
    BOOST_CHECK(s.IsCommitment(data));

    // Commitment to a value of the wrong size.
    data.push_back(42);
    BOOST_CHECK(!s.IsCommitment(data));

    // Not a commitment.
    s = CScript() << data;
    BOOST_CHECK(!s.IsCommitment(data));

    // Non empty commitment.
    s = CScript() << OP_RETURN << data;
    BOOST_CHECK(s.IsCommitment(data));

    // Commitment to the wrong value.
    data[0] = 0x42;
    BOOST_CHECK(!s.IsCommitment(data));

    // Commitment to a larger value.
    std::string str = "Bitcoin: A peer-to-peer Electronic Cash System";
    data = std::vector<unsigned char>(str.begin(), str.end());
    BOOST_CHECK(!s.IsCommitment(data));

    s = CScript() << OP_RETURN << data;
    BOOST_CHECK(s.IsCommitment(data));

    // 64 bytes commitment, still valid.
    data.resize(64);
    s = CScript() << OP_RETURN << data;
    BOOST_CHECK(s.IsCommitment(data));

    // Commitment is too large.
    data.push_back(23);
    s = CScript() << OP_RETURN << data;
    BOOST_CHECK(!s.IsCommitment(data));

    // Check with the actual replay commitment we are going to use.
    s = CScript() << OP_RETURN << GetAntiReplayCommitment();
    BOOST_CHECK(s.IsCommitment(GetAntiReplayCommitment()));
}

BOOST_AUTO_TEST_CASE(test_antireplay) {
    const int nBlockHeight = UAHF_DEFAULT_PROTECT_THIS_SUNSET;
    const int64_t nBlockTime = UAHF_DEFAULT_ACTIVATION_TIME;

    CMutableTransaction tx;
    tx.nVersion = 1;
    tx.vin.resize(1);
    tx.vin[0].prevout.hash = GetRandHash();
    tx.vin[0].prevout.n = 0;
    tx.vin[0].scriptSig = CScript();
    tx.vout.resize(1);
    tx.vout[0].nValue = 1;
    tx.vout[0].scriptPubKey = CScript();

    {
        // Base transaction is valid.
        CValidationState state;
        BOOST_CHECK(ContextualCheckTransaction(tx, state, nBlockHeight, nBlockTime, nBlockTime));
    }

    {
        // Base transaction is still valid after sunset.
        CValidationState state;
        BOOST_CHECK(ContextualCheckTransaction(tx, state, nBlockHeight + 1, nBlockTime, nBlockTime));
    }

    {
        // Base transaction is valid before the fork.
        CValidationState state;
        BOOST_CHECK(ContextualCheckTransaction(tx, state, nBlockHeight, nBlockTime - 1, nBlockTime -1));
    }

    tx.vout[0].scriptPubKey = CScript() << OP_RETURN << OP_0;

    {
        // Wrong commitment, still valid.
        CValidationState state;
        BOOST_CHECK(ContextualCheckTransaction(tx, state, nBlockHeight, nBlockTime, nBlockTime));
    }

    tx.vout[0].scriptPubKey = CScript() << OP_RETURN
                                        << GetAntiReplayCommitment();

    {
        // Anti replay commitment, not valid anymore.
        CValidationState state;
        BOOST_CHECK(!ContextualCheckTransaction(tx, state, nBlockHeight, nBlockTime, nBlockTime));
        BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-txn-replay");
    }

    {
        // Anti replay commitment, disabled before start time.
        CValidationState state;
        BOOST_CHECK(ContextualCheckTransaction(tx, state, nBlockHeight, nBlockTime - 1, nBlockTime - 1));
    }

    {
        // Anti replay commitment, disabled after sunset.
        CValidationState state;
        BOOST_CHECK(ContextualCheckTransaction(tx, state, nBlockHeight + 1, nBlockTime, nBlockTime));
    }
}

BOOST_AUTO_TEST_SUITE_END()
