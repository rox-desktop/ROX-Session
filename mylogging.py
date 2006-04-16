# Python 2.2 doesn't have a logging module
import sys
def info(*args): print args
def warn(*args): print >>sys.stderr, args
def debug(*args): pass

class Logger:
	def setLevel(self, level): pass
getLogger = Logger

DEBUG, INFO = 1, 2
