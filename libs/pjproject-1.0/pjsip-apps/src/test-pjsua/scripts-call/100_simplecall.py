# $Id: 100_simplecall.py 2025 2008-06-15 19:43:43Z bennylp $
#
from inc_cfg import *

# Simple call
test_param = TestParam(
		"Basic call",
		[
			InstanceParam("callee", "--null-audio --max-calls=1"),
			InstanceParam("caller", "--null-audio --max-calls=1")
		]
		)
