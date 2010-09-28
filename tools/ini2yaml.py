import os
import yaml
from ConfigParser import ConfigParser as cp

# path = os.environ['HOME'] + "/.config/sflphone/sflphonedrc"
path = "sflphonedrc"
c = cp()
c.read(path)
accnodes = ['srtp', 'tls', 'zrtp']
auxnodes = ['alsa', 'pulse', 'dtmf']
dico = {}
dico['Accounts'] = []


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
# parcourt des sections du fichier d'origine
for sec in c.sections():
    # les comptes sont maintenant dans une liste de comptes
    if 'Account' in sec or sec == 'IP2IP':
        dsec = 'Accounts'
        # dict temporaire pour insertion ulterieure des comptes dans le dictionnaire
        daccount = {}
        daccount['id'] = sec
        # dict temporaire pour insertion ulterieure des nodes dans le compte
        subdic = {}
        # preparation du dictionnaire pour les nodes du compte
        for x in accnodes:
            subdic[x] = {}
        # parcourt des options
        for opt in c.options(sec):
            spl = opt.split('.')
            # si nous avons affaire a un node
            if spl[0] in accnodes:
                # on ajoute dans le sous dict du compte
                print spl[1]
		subdic[spl[0]][conversion[spl[1]]] = c.get(sec, opt)
            # sinon l'option est attachee au compte
            else:
                daccount[spl[len(spl) -1]] = c.get(sec, opt)
        # insertion des nodes dans le compte
        for x in accnodes:
            daccount[x] = subdic[x]
        #insertion du compte dans le dictionnaire principal
        dico[dsec].append(daccount)
    else:
        dsec = sec
        dico[dsec] = {}
        #print dsec
        #for opt in c.options(sec):
        #    dico[dsec][opt] = c.get(sec, opt)
        #    print opt 
        subdic = {}
        # preparation du dictionnaire pour les nodes de la section
        for x in auxnodes:
            subdic[x] = {}
        # parcourt des options
        for opt in c.options(sec):
            spl = opt.split('.')
            # si nous avons affaire a un node
            if spl[0] in auxnodes:
                # on ajoute dans le sous dict du compte
		print spl[1]
                subdic[spl[0]][conversion[spl[1]]] = c.get(sec, opt)
            # sinon l'option est attachee au compte
            else:
                dico[sec][spl[len(spl) -1 ]] = c.get(sec, opt)
        # insertion des nodes dans le compte
            for x in auxnodes:
                if subdic[x]:
                    dico[sec][x] = subdic[x]


f = open('blah.yml', 'wr')
f.write(yaml.dump(dico, default_flow_style=False))
f.close()

#Addressbook: ajouter 'list': None
#Rings.ringChoice ringsringChoice
# dictionnaires:
# alsa, accounts, 
