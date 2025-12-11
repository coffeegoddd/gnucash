/********************************************************************\
 * test-dolt-backend.cpp: Basic tests for the Dolt backend          *
 *                                                                  *
 * These tests are intended to be run against an existing Dolt      *
 * database specified via the TEST_DOLT_URL environment variable,   *
 * e.g.:                                                            *
 *   TEST_DOLT_URL="dolt://user:password@host/database-name/"      *
 *                                                                  *
 * The tests will be skipped if TEST_DOLT_URL is not set.           *
 ********************************************************************/

#include <config.h>

#include <gtest/gtest.h>

#include "qof.h"
#include "qofsession.h"
#include "qofbook.h"
#include "qofbackend.h"

#include "gnc-backend-dolt.h"

namespace {

class DoltBackendTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        url = std::getenv("TEST_DOLT_URL");
        if (!url)
            GTEST_SKIP() << "TEST_DOLT_URL not set; skipping Dolt backend tests";
    }

    const char* url {nullptr};
};

TEST_F(DoltBackendTest, BasicSessionOpen)
{
    auto book = qof_book_new();
    auto session = qof_session_new(book);

    qof_session_begin(session, url, SESSION_NORMAL_OPEN);
    EXPECT_EQ(qof_session_get_error(session), ERR_BACKEND_NO_ERR);

    auto be = qof_session_get_backend(session);
    ASSERT_NE(be, nullptr);
    EXPECT_TRUE(gnc_dolt_backend_is_dolt(be));

    qof_session_end(session);
    qof_session_destroy(session);
}

TEST_F(DoltBackendTest, BranchOperations)
{
    auto book = qof_book_new();
    auto session = qof_session_new(book);

    qof_session_begin(session, url, SESSION_NORMAL_OPEN);
    EXPECT_EQ(qof_session_get_error(session), ERR_BACKEND_NO_ERR);

    auto be = qof_session_get_backend(session);
    ASSERT_NE(be, nullptr);
    ASSERT_TRUE(gnc_dolt_backend_is_dolt(be));

    // List branches; should not error, may be empty.
    auto branches = gnc_dolt_list_branches(be);
    if (branches)
    {
        for (gchar** p = branches; *p; ++p)
            g_free(*p);
        g_free(branches);
    }

    // Create a throwaway branch name based on time.
    auto t = static_cast<long>(time(nullptr));
    auto branch_name = g_strdup_printf("gnucash_test_branch_%ld", t);

    EXPECT_TRUE(gnc_dolt_create_branch(be, branch_name));

    // Just check that checkout call does not raise an error; whether
    // it actually changes data is covered by higher-level tests.
    EXPECT_TRUE(gnc_dolt_checkout_branch(be, branch_name));

    g_free(branch_name);

    qof_session_end(session);
    qof_session_destroy(session);
}

TEST_F(DoltBackendTest, OpenOnBranchHead)
{
    auto book = qof_book_new();
    auto session = qof_session_new(book);

    // First open a normal session so we can create a throwaway branch.
    qof_session_begin(session, url, SESSION_NORMAL_OPEN);
    ASSERT_EQ(qof_session_get_error(session), ERR_BACKEND_NO_ERR);

    auto be = qof_session_get_backend(session);
    ASSERT_NE(be, nullptr);
    ASSERT_TRUE(gnc_dolt_backend_is_dolt(be));

    // Create a unique branch name.
    auto t = static_cast<long>(time(nullptr));
    auto branch_name = g_strdup_printf("gnucash_test_open_on_branch_%ld", t);
    ASSERT_TRUE(gnc_dolt_create_branch(be, branch_name));

    // End this session and reuse the same QofSession object to open on the new branch.
    qof_session_end(session);

    ASSERT_TRUE(gnc_dolt_session_open_on_branch(session, url, branch_name,
                                                SESSION_NORMAL_OPEN, nullptr));
    EXPECT_EQ(qof_session_get_error(session), ERR_BACKEND_NO_ERR);

    auto be2 = qof_session_get_backend(session);
    ASSERT_NE(be2, nullptr);
    EXPECT_TRUE(gnc_dolt_backend_is_dolt(be2));

    g_free(branch_name);

    qof_session_end(session);
    qof_session_destroy(session);
}

TEST_F(DoltBackendTest, SessionCheckoutBranchReopensAndLoads)
{
    auto book = qof_book_new();
    auto session = qof_session_new(book);

    // Open on the default branch and create a throwaway target branch.
    qof_session_begin(session, url, SESSION_NORMAL_OPEN);
    ASSERT_EQ(qof_session_get_error(session), ERR_BACKEND_NO_ERR);

    auto be = qof_session_get_backend(session);
    ASSERT_NE(be, nullptr);
    ASSERT_TRUE(gnc_dolt_backend_is_dolt(be));

    auto t = static_cast<long>(time(nullptr));
    auto branch_name = g_strdup_printf("gnucash_test_checkout_branch_%ld", t);
    ASSERT_TRUE(gnc_dolt_create_branch(be, branch_name));

    // Load the current branch so the session has an attached book.
    qof_session_load(session, nullptr);
    ASSERT_EQ(qof_session_get_error(session), ERR_BACKEND_NO_ERR);

    // Switch the session to the new branch; this should end and reopen
    // the session internally and then load a fresh book.
    ASSERT_TRUE(gnc_dolt_session_checkout_branch(session, branch_name,
                                                 SESSION_NORMAL_OPEN, nullptr));
    EXPECT_EQ(qof_session_get_error(session), ERR_BACKEND_NO_ERR);

    auto be2 = qof_session_get_backend(session);
    ASSERT_NE(be2, nullptr);
    EXPECT_TRUE(gnc_dolt_backend_is_dolt(be2));

    g_free(branch_name);

    qof_session_end(session);
    qof_session_destroy(session);
}

TEST_F(DoltBackendTest, SafeSaveFlushesAndCommitsWithoutPriorSave)
{
    auto book = qof_book_new();
    auto session = qof_session_new(book);

    // Open a normal session on the default Dolt branch.
    qof_session_begin(session, url, SESSION_NORMAL_OPEN);
    ASSERT_EQ(qof_session_get_error(session), ERR_BACKEND_NO_ERR);

    auto be = qof_session_get_backend(session);
    ASSERT_NE(be, nullptr);
    ASSERT_TRUE(gnc_dolt_backend_is_dolt(be));

    // Mark the book dirty to ensure safe_save() has something to flush.
    qof_book_mark_session_dirty(qof_session_get_book(session));

    // Call safe_save() directly; for Dolt this should flush any pending
    // changes and create a Dolt commit on the current branch.
    qof_session_safe_save(session, nullptr);
    EXPECT_EQ(qof_session_get_error(session), ERR_BACKEND_NO_ERR);

    qof_session_end(session);
    qof_session_destroy(session);
}

} // anonymous namespace
