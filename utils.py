class SyntaxError(Exception): pass

def shell_split(command):
	"""Convert command to a list, rather as a shell would do, but without the
	expansions."""
	args = []
	tokens = iter(command)
	arg = None
	quote_type = None
	for next in tokens:
		if next == '\\':
			try:
				next = tokens.next()
			except StopIteration:
				raise SyntaxError("\\ at end of line in: %s" % command)
			if arg is None:
				arg = ''
			arg += next
			continue

		if quote_type is None:
			if next in ' \t':
				# Unquoted whitespace - new argument
				if arg is not None:
					args.append(arg)
					arg = None
			elif next in '\'"':
				# Starting quoted section
				quote_type = next
				if arg is None:
					arg = ''
			else:
				# Continuing non-quoted non-whitespace text
				if arg is None:
					arg = ''
				arg += next
		else:
			if next == quote_type:
				# End quote
				quote_type = None
			else:
				arg += next

	if quote_type is not None:
		raise SyntaxError("Expected %s, not end-of-line in: %s" %
				(quote_type, command))

	if arg is not None:
		args.append(arg)

	return args
