import os
import unittest

from gnucash import Session, SessionOpenMode, ERR_BACKEND_NO_ERR
from gnucash import gnucash_core_c


class DoltBackendTestCase(unittest.TestCase):
    """Basic integration tests for the Dolt backend via Python bindings.

    These tests require TEST_DOLT_URL to be set to a valid Dolt-backed
    URL, e.g. "dolt://user:password@host/database-name/". If the
    variable is not set the tests are skipped.
    """

    @classmethod
    def setUpClass(cls):
        cls.url = os.environ.get("TEST_DOLT_URL")
        if not cls.url:
            raise unittest.SkipTest("TEST_DOLT_URL not set; skipping Dolt backend tests")

    def test_session_open_and_detection(self):
        with Session(self.url, SessionOpenMode.SESSION_NORMAL_OPEN) as sess:
            self.assertTrue(sess.is_dolt_backend())

    def test_branch_operations(self):
        with Session(self.url, SessionOpenMode.SESSION_NORMAL_OPEN) as sess:
            self.assertTrue(sess.is_dolt_backend())

            # Explicitly select the main branch so any Dolt operations
            # operate against a known branch instead of relying on the
            # server's default.
            sess.dolt_checkout_branch("main")

            # List branches; should not error, may be empty. We
            # intentionally avoid creating or checking out throwaway
            # branches so this test exercises only read-only branch
            # metadata.
            branches = sess.dolt_list_branches()
            self.assertIsInstance(branches, list)

    def test_commit(self):
        with Session(self.url, SessionOpenMode.SESSION_NORMAL_OPEN) as sess:
            self.assertTrue(sess.is_dolt_backend())

            # Explicitly select the main branch so add/commit operate
            # against a known branch instead of relying on the server's
            # default.
            sess.dolt_checkout_branch("main")

            # Ensure we can call add/commit without raising.
            sess.dolt_add()
            commit_hash = sess.dolt_commit("Test commit from Python bindings")
            # Commit hash may be None if not available; just assert the
            # call didn't raise.
            self.assertTrue(commit_hash is None or isinstance(commit_hash, str))

    def test_safe_save_flushes_without_error(self):
        # Open a Dolt-backed session without using the context manager so
        # we can control when safe_save() is invoked.
        sess = Session(self.url, SessionOpenMode.SESSION_NORMAL_OPEN)
        try:
            self.assertTrue(sess.is_dolt_backend())

            # Explicitly select the main branch so that safe_save()
            # operates against a known Dolt branch instead of relying
            # on the server's default.
            sess.dolt_checkout_branch("main")

            # Mark the book as having unsaved changes so that safe_save()
            # exercises the flush path (safe_save does not create Dolt
            # commits; callers must explicitly dolt_add/dolt_commit).
            book = sess.book
            gnucash_core_c.qof_book_mark_session_dirty(book.get_instance())

            # Invoke safe_save() directly; for Dolt this should flush any
            # pending changes to the working set on the explicitly
            # selected branch without leaving a backend error.
            sess.safe_save(None)
            self.assertEqual(sess.get_error(), ERR_BACKEND_NO_ERR)
        finally:
            sess.end()
            sess.destroy()


if __name__ == "__main__":
    unittest.main()
