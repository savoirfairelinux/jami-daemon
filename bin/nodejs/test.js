const JamiDaemon = require('.')

const cbMap = {
    'incomingAccountMessage':(a, b, c, d) => console.log("incomingAccountMessage " + a + b + c + d)
}

const daemon = new JamiDaemon(cbMap)
console.log(daemon.getAccountList())
