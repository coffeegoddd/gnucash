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

            branches = sess.dolt_list_branches()
            self.assertIsInstance(branches, list)

            # Create and checkout a throwaway branch name.
            branch_name = f"gnucash_py_test_branch_{os.getpid()}"
            sess.dolt_create_branch(branch_name)
            sess.dolt_checkout_branch(branch_name)

    def test_commit(self):
        with Session(self.url, SessionOpenMode.SESSION_NORMAL_OPEN) as sess:
            self.assertTrue(sess.is_dolt_backend())

            # Ensure we can call add/commit without raising.
            sess.dolt_add()
            commit_hash = sess.dolt_commit("Test commit from Python bindings")
            # Commit hash may be None if not available; just assert the
            # call didn't raise.
            self.assertTrue(commit_hash is None or isinstance(commit_hash, str))

    def test_open_on_branch_helper(self):
        # Create a throwaway branch on the default backend.
        branch_name = f"gnucash_py_test_open_on_branch_{os.getpid()}"
        with Session(self.url, SessionOpenMode.SESSION_NORMAL_OPEN) as sess:
            self.assertTrue(sess.is_dolt_backend())
            sess.dolt_create_branch(branch_name)

        # Use a fresh Session and the high-level helper to open directly
        # on the new branch's HEAD.
        sess2 = Session()
        try:
            sess2.dolt_open_on_branch(self.url, branch_name)
            self.assertTrue(sess2.is_dolt_backend())
            # Accessing book should succeed for a fully-loaded session.
            self.assertIsNotNone(sess2.book)
        finally:
            sess2.end()
            sess2.destroy()

    def test_session_checkout_branch_helper(self):
        # Start on the default branch.
        sess = Session(self.url, SessionOpenMode.SESSION_NORMAL_OPEN)
        try:
            self.assertTrue(sess.is_dolt_backend())

            # Create a throwaway branch name.
            branch_name = f"gnucash_py_test_session_checkout_{os.getpid()}"
            sess.dolt_create_branch(branch_name)

            # Switch the existing Session to the new branch using the
            # session-level helper, which will reopen and reload.
            sess.dolt_session_checkout_branch(branch_name)
            self.assertTrue(sess.is_dolt_backend())
            self.assertIsNotNone(sess.book)
        finally:
            sess.end()
            sess.destroy()

    def test_safe_save_flushes_without_error(self):
        # Open a Dolt-backed session without using the context manager so
        # we can control when safe_save() is invoked.
        sess = Session(self.url, SessionOpenMode.SESSION_NORMAL_OPEN)
        try:
            self.assertTrue(sess.is_dolt_backend())

            # Mark the book as having unsaved changes so that safe_save()
            # exercises the flush path before committing via Dolt.
            book = sess.book
            gnucash_core_c.qof_book_mark_session_dirty(book.get_instance())

            # Invoke safe_save() directly; for Dolt this should flush any
            # pending changes and create a Dolt commit on the current branch
            # without leaving a backend error.
            sess.safe_save(None)
            self.assertEqual(sess.get_error(), ERR_BACKEND_NO_ERR)
        finally:
            sess.end()
            sess.destroy()


if __name__ == "__main__":
    unittest.main()
