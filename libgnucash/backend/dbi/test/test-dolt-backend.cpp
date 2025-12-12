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

#include "gnc-backend-dbi.h"
#include "gnc-backend-dolt.h"

namespace {

class DoltBackendTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        /* Ensure the DBI backend and its libdbi instance are initialized
         * before attempting to open any Dolt-backed sessions. The GLib
         * test fixtures finalize the backend after each run, so we need
         * to re-initialize here for the GoogleTest-based suite. */
        gnc_module_init_backend_dbi();

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

    // Explicitly select the main branch so that any subsequent Dolt
    // operations operate against a known branch instead of relying on
    // the server's default.
    ASSERT_TRUE(gnc_dolt_checkout_branch(be, "main"));

    // List branches; should not error, may be empty. We intentionally
    // avoid creating or checking out throwaway branches so that this
    // test exercises only read-only branch metadata on the main branch.
    auto branches = gnc_dolt_list_branches(be);
    if (branches)
    {
        for (gchar** p = branches; *p; ++p)
            g_free(*p);
        g_free(branches);
    }

    qof_session_end(session);
    qof_session_destroy(session);
}

TEST_F(DoltBackendTest, SafeSaveFlushesWithoutError)
{
    auto book = qof_book_new();
    auto session = qof_session_new(book);

    // Open a normal session on the default Dolt branch.
    qof_session_begin(session, url, SESSION_NORMAL_OPEN);
    ASSERT_EQ(qof_session_get_error(session), ERR_BACKEND_NO_ERR);

    auto be = qof_session_get_backend(session);
    ASSERT_NE(be, nullptr);
    ASSERT_TRUE(gnc_dolt_backend_is_dolt(be));

    // Explicitly select the main branch so that safe_save() operates
    // against a known Dolt branch instead of relying on the server's
    // default; GncDoltBackend::safe_sync now requires this.
    ASSERT_TRUE(gnc_dolt_checkout_branch(be, "main"));

    // Mark the book dirty to ensure safe_save() has something to flush.
    qof_book_mark_session_dirty(qof_session_get_book(session));

    // Call safe_save() directly; for Dolt this should safely flush any
    // pending changes into the working set on the explicitly selected
    // branch. Creating Dolt commits is an explicit caller operation and
    // is not performed by safe_save().
    qof_session_safe_save(session, nullptr);
    EXPECT_EQ(qof_session_get_error(session), ERR_BACKEND_NO_ERR);

    qof_session_end(session);
    qof_session_destroy(session);
}

} // anonymous namespace
