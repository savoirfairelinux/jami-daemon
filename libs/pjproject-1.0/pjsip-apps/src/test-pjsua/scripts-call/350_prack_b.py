# $Id: 350_prack_b.py 2059 2008-06-26 13:29:29Z bennylp $
#
from inc_cfg import *

# TCP call
test_param = TestParam(
		"Caller requires PRACK",
		[
			InstanceParam("callee", "--null-audio --max-calls=1"),
			InstanceParam("caller", "--null-audio --max-calls=1 --use-100rel")
		]
		)
