import os
import yaml
from ConfigParser import ConfigParser as cp

path = os.environ['HOME'] + "/.config/sflphone/sflphonedrc"
# path = "sflphonedrc"
c = cp()
c.read(path)
accnodes = ['srtp', 'tls', 'zrtp']
auxnodes = ['alsa', 'pulse', 'dtmf']
dico = {}
dico['accounts'] = []

# Dictionary used to convert string used in prior configuration file to new one.
conversion = {

	# addressbook
	'addressbook': 'addressbook',
	'contact_photo': 'photo',
	'enable': 'enabled',
	'max_results': 'maxResults',
	'phone_business': 'business',
	'phone_home': 'photo',
	'phone_mobile': 'mobile',

	# audio
	'audio': 'audio',
	'cardid_in': 'cardIn',
	'cardid_out': 'cardOut',
	'cardid_ring': 'cardRing',
	'framesize': 'frameSize',
	'plugin': 'plugin',
	'samplerate': 'smplRate',
	'deviceplayback': 'devicePlayback',
	'devicerecord': 'deviceRecord',
	'deviceringtone': 'deviceRingtone',
	'path': 'recordPath',
	'ringchoice': 'ringtonePath',
	'micro': 'volumeMic',
	'speakers': 'volumeSpkr',

	# hooks
	'hooks': 'hooks',
	'iax2_enabled': 'iax2Enabled',
	'phone_number_add_prefix': 'numberAddPrefix',
	'phone_number_enabled': 'numberEnabled',
	'sip_enabled': 'sipEnabled',
	'url_command': 'urlCommand',
	'url_sip_field': 'urlSipField',

	# general preference
	'preferences': 'preferences',
	'order': 'order',
	'api': 'audioApi',
	'display': 'searchBarDisplay',
	'limit': 'historyLimit',
	'mails': 'notifyMails',
	'zonetonechoice': 'zoneToneChoice',
	'portnum': 'portNum',
	'md5hash': 'md5Hash',

	# voip link
	'voiplink': 'voipPreferences',
	'playdtmf': 'playDtmf',
	'playtones': 'playTones',
	'pulselength': 'pulseLength',
	'senddtmfas': 'dtmfType',
	'symmetric': 'symmetric',
	'zidfile': 'zidFile',

	# account
	'accounts': 'accounts',
	'ip2ip': 'IP2IP',
	'alias': 'alias',
	'displayname': 'displayName',
	'localinterface': 'interface',
	'localport': 'port',
	'publishedaddress': 'publishAddr',
	'publishedport': 'publishPort',
	'publishedsameaslocal': 'sameasLocal',
	'activecodecs': 'codecs',
	# srtp
	'enable': 'enable',
	'keyexchange': 'keyExchange',
	'rtpfallback': 'rtpFallback',
	# stun
	'enable': 'stunEnabled',
	'server': 'stunServer',
	# tls
	'certificatefile': 'certificate',
	'certificatelistfile': 'calist',
	'ciphers': 'ciphers',
	'enable': 'enable',
	'listenerport': 'tlsPort',
	'method': 'password',
	'negotiationtimemoutmsec': '',
	'negotiationtimeoutsec': 'timeout',
	'password': 'password',
	'privatekeyfile': 'privateKey',
	'requireclientcertificate': 'requireCertif',
	'servername': 'server',
	'verifyclient': 'verifyClien',
	'verifyserver': 'verifyServer',
	# zrtp
	'displaysas': 'displaySAS',
	'displaysasonce': 'displaySasOnce',
	'hellohashenable': 'helloHashEnable',
	'notsuppwarning': 'notSuppWarning',

	# to be removed
	'listenerport': 'port',
}


# Dictionary to convert sections string
section_conversion = {

	'Accounts': 'accounts',
	'Addressbook': 'addressbook',
	'Audio': 'audio',
	'Hooks': 'hooks',
	'Preferences': 'preferences',
	'VoIPLink': 'voipPreferences',
	'Shortcuts': 'shortcuts'
}



# run over every sections in original file
for sec in c.sections():
    # accounts are now stored in an account list
    if 'Account' in sec or sec == 'IP2IP':
        dsec = 'accounts'
        # temporary account dictionary to be inserted in main dictionary 
	daccount = {}
        daccount['id'] = sec
        # temporary account dictionary to be inserted in account nodes
	subdic = {}
        # preparing account dictionary
        for x in accnodes:
            subdic[x] = {}
        # run over every options
        for opt in c.options(sec):
            spl = opt.split('.')
            # if this is an account node
            if spl[0] in accnodes:
                # add into the account dictionary
                print spl[1]
		subdic[spl[0]][conversion[spl[1]]] = c.get(sec, opt)
            # else, the options is attached to the primary dictionary
            else:
                daccount[spl[len(spl) -1]] = c.get(sec, opt)
        # insert account nodes in account dictionary
        for x in accnodes:
            daccount[x] = subdic[x]
        # insert account dictionary in main dictionary
        dico[dsec].append(daccount)
    else:
        dsec = section_conversion[sec]
        dico[dsec] = {}
        subdic = {}
        # prepare dictionary for section's node
        for x in auxnodes:
            subdic[x] = {}
        # run over all fields
        for opt in c.options(sec):
            spl = opt.split('.')
            # if this is a node
            if spl[0] in auxnodes:
                # add into sections dictionary
		print spl[1]
                subdic[spl[0]][conversion[spl[1]]] = c.get(sec, opt)
            # else if this option is attached to an accout
            else:
                dico[section_conversion[sec]][spl[len(spl) -1 ]] = c.get(sec, opt)
        # inserting the node into the account
            for x in auxnodes:
                if subdic[x]:
                    dico[section_conversion[sec]][x] = subdic[x]


# Make sure all accunt are enabled (especially IP2IP)
for acc in dico['accounts']:
	acc['enable'] = 'true'

# Save in new configuration file
newPath = os.environ['HOME'] + "/.config/sflphone/sflphonedrc"
# newPath = 'blah.yml'

# Save new configuration file
f = open(newPath, 'wr')
f.write(yaml.dump(dico, default_flow_style=False))
f.close()

#Addressbook: ajouter 'list': None
#Rings.ringChoice ringsringChoice
# dictionnaires:
# alsa, accounts, 
