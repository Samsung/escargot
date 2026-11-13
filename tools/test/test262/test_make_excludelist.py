#!/usr/bin/env python3

import contextlib
import io
import unittest
from unittest.mock import patch

import make_excludelist as generator


FRONT = '<excludeList>\n'
END = '</excludeList>\n'


def entry(name, reason='TODO'):
    return '    <test id="' + name + '"><reason>' + reason + '</reason></test>\n'


class ExcludeListTests(unittest.TestCase):
    def setUp(self):
        self.previous = FRONT + '<!-- keep -->\n' + entry('z', 'custom') + entry('a') + END
        self.template = FRONT + entry('z', 'template reason').rstrip('\n')

    def test_unchanged_preserves_exact_text(self):
        self.assertEqual(generator.update_exclude_list(
            self.previous, self.template, {'a'}), self.previous)

    def test_changes_preserve_order_and_reason(self):
        result = generator.update_exclude_list(self.previous, self.template, {'new'})
        self.assertEqual(result, FRONT + '<!-- keep -->\n' + entry('z', 'custom')
                         + entry('new') + END)
        self.assertEqual(generator.update_exclude_list(result, self.template, {'new'}), result)

    def test_template_without_final_newline(self):
        result = generator.update_exclude_list(FRONT + END, self.template, {'new'})
        self.assertEqual(result, FRONT + entry('z', 'template reason') + entry('new') + END)

    def test_all_pass_removes_only_non_template_entries(self):
        self.assertEqual(generator.update_exclude_list(self.previous, self.template, set()),
                         FRONT + '<!-- keep -->\n' + entry('z', 'custom') + END)

    def test_commented_entries_and_duplicate_entries_stay_unchanged(self):
        previous = (FRONT + '<!-- ' + entry('example').strip() + ' -->\n'
                    + entry('z') + entry('z') + END)
        self.assertEqual(generator.update_exclude_list(previous, self.template, set()), previous)

    def test_new_ids_are_xml_escaped(self):
        result = generator.update_exclude_list(FRONT + END, FRONT, {'a&b'})
        self.assertEqual(generator.test_ids(result), {'a&b'})

    def test_report_counts_ids_instead_of_lines(self):
        result = generator.update_exclude_list(self.previous, self.template, {'new'})
        with contextlib.redirect_stdout(io.StringIO()) as output:
            generator.report_changes(self.previous, result)
        self.assertIn('New exclusions: 1', output.getvalue())
        self.assertIn('Removed exclusions: 1', output.getvalue())

    def test_incomplete_summary_restores_previous_list(self):
        with patch.object(generator, 'run_all_test262', return_value='=== Test262 Summary ==='), \
                patch.object(generator, 'write_exclude_list') as write, \
                patch('builtins.open', unittest.mock.mock_open(read_data=self.previous)), \
                patch('sys.argv', ['make_excludelist.py']):
            with self.assertRaises(RuntimeError):
                generator.main()
        write.assert_called_once_with(self.previous)


if __name__ == '__main__':
    unittest.main()
