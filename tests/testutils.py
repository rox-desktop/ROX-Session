#!/usr/bin/env python2.2
import unittest
import sys
import os, time
from os.path import dirname, abspath, join

sys.path.insert(0, '..')

import utils

class TestUtils(unittest.TestCase):
	def testShellSplit(self):
		self.assertEquals(['hello', 'world'],
			utils.shell_split('hello world'))
		self.assertEquals(['hello', 'world'],
			utils.shell_split('hello "world"'))
		self.assertEquals(['hello', 'world"'],
			utils.shell_split("""hello 'world"'"""))
		self.assertEquals(['hello', 'worldhi'],
			utils.shell_split("""hello 'world'"hi\""""))
		self.assertEquals(['hello', 'world\''],
			utils.shell_split("""hello 'world\\''"""))
		self.assertEquals(['hello', '\\world'],
			utils.shell_split('hello \\\\"world"'))
		self.assertEquals(['hello world'],
			utils.shell_split('"hello world"'))
		self.assertEquals([], utils.shell_split(""))
		self.assertEquals(['hello', 'world'],
			utils.shell_split('   hello   world  '))

	def testShellSplitFailures(self):
		try:
			utils.shell_split("hi '")
			assert False
		except utils.SyntaxError:
			pass
		try:
			print utils.shell_split("hi \\")
			assert False
		except utils.SyntaxError:
			pass

		try:
			utils.shell_split('hello \\"world"')
		except utils.SyntaxError:
			pass
	

sys.argv.append('-v')
unittest.main()
