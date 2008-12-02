# $Id: 100_peertopeer.py 2025 2008-06-15 19:43:43Z bennylp $
#
from inc_cfg import *

# Direct peer to peer presence
test_param = TestParam(
		"Direct peer to peer presence",
		[
			InstanceParam("client1", "--null-audio"),
			InstanceParam("client2", "--null-audio")
		]
		)
