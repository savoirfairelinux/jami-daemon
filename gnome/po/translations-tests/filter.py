#!/usr/bin/python

# To be put in a directory inside the translations directory

import os
from subprocess import call

# run the tests
call([
    "pofilter",
    # test(s) to be run
    "-t", "printf",
    "-t", "xmltags",
    # input directory
    "..",
    # output directory
    "../errors"
])

# list of files with errors
errors = os.listdir("../errors")

# print the results
if len(errors) > 0:
    print "There is", len(errors), "files with errors:"
    for e in errors:
        print e
else:
    print "There is no errors."
