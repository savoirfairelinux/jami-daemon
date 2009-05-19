#!/usr/bin/env python

from sflphonectrlsimple import SflPhoneCtrlSimple


class SflPhoneTests(SflPhoneCtrlSimple):

    def test_get_allaccounts_methods(self):

        print "--- getAllAccounts() ---"
        for account in self.getAllAccounts():
            print "  " + account
        print "\n"

        print "--- getAllRegisteredAccounts() ---"
        for account in self.getAllRegisteredAccounts():
            print "  " + account
        print "\n"

        print "--- getAllSipAccounts() ---"
        for account in self.getAllSipAccounts():
            print "  " + account
        print "\n"

        print "--- getAllIaxAccounts() ---"
        for account in self.getAllIaxAccounts():
            print "  " + account
        print "\n"

 #   def test_codecs_methods(self):

#        print "--- getCodecList() ---"
#        for codec int self.getCodecList():
#            print "  " + codec
#        print "\n"


sfl = SflPhoneTests()

sfl.test_get_allaccounts_methods()




		
			
