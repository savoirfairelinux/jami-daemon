#!/usr/bin/python

# To be put in a directory inside the translations directory
# To be run after all errors have been corrected or ignored

import os
import sys
from subprocess import call

# check if template is present
if "sflphone.pot" not in os.listdir(".."):
    print "Theres is no template file, add one and try again."
    sys.exit()

# number of files with errors
num_errors = len(os.listdir("../errors"))

# run the merge
call([
    "pomerge",
    # template
    "-t", "../sflphone.pot",
    # input directory
    "../errors",
    # output directory
    "..",
    # remove pofilter comments
    "--mergecomments=no"
])

# remove errors directory
call(["rm", "-r", "../errors"])

# print the results
print num_errors, "files have been merged."
