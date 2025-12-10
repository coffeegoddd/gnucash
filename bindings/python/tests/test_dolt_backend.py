import os
import unittest

from gnucash import Session, SessionOpenMode


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


if __name__ == "__main__":
    unittest.main()
