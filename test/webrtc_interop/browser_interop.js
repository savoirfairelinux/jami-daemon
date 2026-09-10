const fs = require('fs');
const os = require('os');
const path = require('path');
const { spawn } = require('child_process');

const BRIDGE_EVENTS = new Set(['SDP', 'CONNECTED', 'RTP_SENT', 'RESULT']);

function assert(condition, message) {
    if (!condition) {
        throw new Error(message);
    }
}

function assertMediaTransport(sdp, kind, message) {
    const pattern = new RegExp(`m=${kind}\\s+\\d+\\s+UDP/TLS/RTP/SAVPF`);
    assert(pattern.test(sdp), message);
}

function assertUsableIceCandidates(sdp, message) {
    const candidates = sdp.split(/\r?\n/).filter((line) => line.startsWith('a=candidate:'));
    assert(candidates.length > 0, `${message}: browser SDP has no ICE candidates`);
    assert(!candidates.some((candidate) => /\.local(?:\s|$)/i.test(candidate)),
        `${message}: browser SDP still contains mDNS host candidates`);
}

function extractSdp(output) {
    const match = output.match(/(^|\n)v=0\r?\n/);
    if (!match) {
        return output;
    }
    return output.slice(match.index + match[1].length);
}

function resolveBridgePath() {
    const repoRoot = path.resolve(__dirname, '..', '..');
    const explicitPath = process.argv[2] || process.env.JAMI_WEBRTC_SDP_BRIDGE;
    const candidates = explicitPath
        ? [path.resolve(explicitPath)]
        : [
            path.join(repoRoot, 'build', 'jami_webrtc_sdp_bridge'),
            path.join(repoRoot, 'build-meson', 'jami_webrtc_sdp_bridge'),
            path.join(repoRoot, '..', 'build', 'daemon', 'jami_webrtc_sdp_bridge'),
        ];

    const bridgePath = candidates.find((candidate) => fs.existsSync(candidate));
    if (!bridgePath) {
        throw new Error(
            'Unable to find jami_webrtc_sdp_bridge. Build it first or pass its path as the first argument.',
        );
    }
    return bridgePath;
}

function resolveBrowserPath() {
    const explicitPath = process.env.JAMI_WEBRTC_BROWSER || process.env.CHROME_BIN;
    const candidates = explicitPath
        ? [explicitPath]
        : [
            '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',
            '/Applications/Chromium.app/Contents/MacOS/Chromium',
            '/Applications/Google Chrome Canary.app/Contents/MacOS/Google Chrome Canary',
        ];

    const browserPath = candidates.find((candidate) => fs.existsSync(candidate));
    if (!browserPath) {
        throw new Error('Unable to find Chrome/Chromium. Set JAMI_WEBRTC_BROWSER or CHROME_BIN to a browser binary.');
    }
    return browserPath;
}

function runJami(bridgePath, mode, input = '') {
    return new Promise((resolve, reject) => {
        const child = spawn(bridgePath, [mode], {
            cwd: __dirname,
            stdio: ['pipe', 'pipe', 'pipe'],
        });

        let stdout = '';
        let stderr = '';

        child.stdout.on('data', (chunk) => {
            stdout += chunk.toString();
        });
        child.stderr.on('data', (chunk) => {
            stderr += chunk.toString();
        });
        child.on('error', reject);
        child.on('close', (code) => {
            if (code !== 0) {
                reject(new Error(`jami_webrtc_sdp_bridge ${mode} failed (${code}): ${stderr.trim()}`));
                return;
            }
            resolve({ stdout: extractSdp(stdout), stderr });
        });

        child.stdin.end(input);
    });
}

function startJamiLive(bridgePath, mode) {
    const child = spawn(bridgePath, [mode], {
        cwd: __dirname,
        stdio: ['pipe', 'pipe', 'pipe'],
    });

    let stderr = '';
    const queue = [];
    const waiters = [];

    child.stderr.on('data', (chunk) => {
        stderr += chunk.toString();
    });

    child.stdout.setEncoding('utf8');
    let buffered = '';
    child.stdout.on('data', (chunk) => {
        buffered += chunk;
        while (true) {
            const lineEnd = buffered.indexOf('\n');
            if (lineEnd < 0) {
                break;
            }
            const line = buffered.slice(0, lineEnd).trim();
            buffered = buffered.slice(lineEnd + 1);
            if (!line) {
                continue;
            }
            const eventName = line.split(' ', 1)[0];
            if (!BRIDGE_EVENTS.has(eventName)) {
                continue;
            }
            if (waiters.length) {
                waiters.shift()(line);
            } else {
                queue.push(line);
            }
        }
    });

    const nextLine = () => new Promise((resolve, reject) => {
        if (queue.length) {
            resolve(queue.shift());
            return;
        }
        if (child.exitCode !== null) {
            reject(new Error(`jami_webrtc_sdp_bridge ${mode} exited early (${child.exitCode}): ${stderr.trim()}`));
            return;
        }
        const onExit = (code) => {
            reject(new Error(`jami_webrtc_sdp_bridge ${mode} exited early (${code}): ${stderr.trim()}`));
        };
        const resolver = (line) => {
            child.off('close', onExit);
            resolve(line);
        };
        child.once('close', onExit);
        waiters.push(resolver);
    });

    const sendSdp = (type, sdp) => {
        const payload = Buffer.from(sdp, 'utf8').toString('base64');
        child.stdin.write(`SDP ${type} ${payload}\n`);
    };

    const startRtp = () => {
        child.stdin.write('START_RTP\n');
    };

    const close = () => new Promise((resolve) => {
        if (child.exitCode !== null) {
            resolve();
            return;
        }
        child.once('close', resolve);
        child.stdin.end();
    });

    return {
        sendSdp,
        startRtp,
        nextLine,
        close,
    };
}

function parseBridgeLine(line) {
    const [name, ...pairs] = line.split(' ');
    const fields = {};
    for (const pair of pairs) {
        const separator = pair.indexOf('=');
        if (separator <= 0) {
            continue;
        }
        fields[pair.slice(0, separator)] = pair.slice(separator + 1);
    }
    return { name, fields };
}

async function waitForLiveConnected(session) {
    while (true) {
        const event = parseBridgeLine(await session.nextLine());
        if (event.name === 'CONNECTED' && event.fields.dtls === 'true') {
            return event;
        }
        if (event.name === 'CONNECTED') {
            continue;
        }
        throw new Error(`Unexpected bridge event before DTLS connectivity: ${event.name}`);
    }
}

async function waitForLiveResult(session) {
    let rtpSent = false;
    while (true) {
        const event = parseBridgeLine(await session.nextLine());
        if (event.name === 'CONNECTED') {
            continue;
        }
        if (event.name === 'RTP_SENT') {
            rtpSent = true;
            continue;
        }
        if (event.name === 'RESULT') {
            assert(rtpSent || event.fields.rtp_sent === 'true', 'Jami bridge did not report RTP send before result');
            assert(event.fields.ice === 'true', 'Jami live bridge did not confirm ICE success');
            assert(event.fields.dtls === 'true', 'Jami live bridge did not confirm DTLS success');
            assert(event.fields.rtp_sent === 'true', 'Jami live bridge did not confirm RTP send');
            return event;
        }
        throw new Error(`Unexpected bridge event: ${event.name}`);
    }
}

function launchBrowser(browserPath) {
    return new Promise((resolve, reject) => {
        const profileDir = fs.mkdtempSync(path.join(os.tmpdir(), 'jami-webrtc-chrome-'));
        const args = [
            '--headless=new',
            '--remote-debugging-port=0',
            '--remote-allow-origins=*',
            `--user-data-dir=${profileDir}`,
            '--no-first-run',
            '--no-default-browser-check',
            '--disable-background-networking',
            '--disable-extensions',
            '--disable-sync',
            '--disable-features=WebRtcHideLocalIpsWithMdns',
            '--disable-webrtc-hide-local-ips-with-mdns',
            '--force-webrtc-ip-handling-policy=default',
            'about:blank',
        ];

        const child = spawn(browserPath, args, { stdio: ['ignore', 'pipe', 'pipe'] });
        let output = '';
        let settled = false;
        const timeout = setTimeout(() => {
            if (!settled) {
                settled = true;
                child.kill();
                reject(new Error(`Timed out waiting for Chrome DevTools endpoint: ${output.trim()}`));
            }
        }, 10000);

        const cleanupProfile = () => {
            fs.rmSync(profileDir, { recursive: true, force: true });
        };

        const onData = (chunk) => {
            output += chunk.toString();
            const match = output.match(/DevTools listening on (ws:\/\/[^\s]+)/);
            if (!match || settled) {
                return;
            }
            settled = true;
            clearTimeout(timeout);
            resolve({ child, browserWsUrl: match[1], cleanupProfile });
        };

        child.stdout.on('data', onData);
        child.stderr.on('data', onData);
        child.on('error', (error) => {
            if (!settled) {
                settled = true;
                clearTimeout(timeout);
                cleanupProfile();
                reject(error);
            }
        });
        child.on('close', (code) => {
            if (!settled) {
                settled = true;
                clearTimeout(timeout);
                cleanupProfile();
                reject(new Error(`Chrome exited before DevTools was ready (${code}): ${output.trim()}`));
            }
        });
    });
}

class CdpClient {
    constructor(webSocket) {
        this.webSocket = webSocket;
        this.nextMessageId = 1;
        this.pending = new Map();

        this.webSocket.addEventListener('message', (event) => {
            const rawData = typeof event.data === 'string' ? event.data : event.data.toString();
            const message = JSON.parse(rawData);
            if (!message.id) {
                return;
            }
            const pending = this.pending.get(message.id);
            if (!pending) {
                return;
            }
            this.pending.delete(message.id);
            if (message.error) {
                pending.reject(new Error(message.error.message || JSON.stringify(message.error)));
            } else {
                pending.resolve(message.result || {});
            }
        });
    }

    static connect(webSocketUrl) {
        if (typeof WebSocket !== 'function') {
            throw new Error('This browser interop test requires a Node.js runtime with WebSocket support.');
        }

        return new Promise((resolve, reject) => {
            const webSocket = new WebSocket(webSocketUrl);
            webSocket.addEventListener('open', () => resolve(new CdpClient(webSocket)), { once: true });
            webSocket.addEventListener('error', () => reject(new Error('Unable to connect to Chrome DevTools')),
                { once: true });
        });
    }

    send(method, params = {}, sessionId = undefined) {
        const messageId = this.nextMessageId++;
        const message = { id: messageId, method, params };
        if (sessionId) {
            message.sessionId = sessionId;
        }

        return new Promise((resolve, reject) => {
            this.pending.set(messageId, { resolve, reject });
            this.webSocket.send(JSON.stringify(message));
        });
    }

    close() {
        this.webSocket.close();
    }
}

function formatException(exceptionDetails) {
    if (!exceptionDetails) {
        return 'Unknown browser exception';
    }
    if (exceptionDetails.exception && exceptionDetails.exception.description) {
        return exceptionDetails.exception.description;
    }
    return exceptionDetails.text || JSON.stringify(exceptionDetails);
}

async function evaluate(client, sessionId, expression) {
    const response = await client.send('Runtime.evaluate', {
        expression,
        awaitPromise: true,
        returnByValue: true,
        userGesture: true,
    }, sessionId);

    if (response.exceptionDetails) {
        throw new Error(formatException(response.exceptionDetails));
    }
    return response.result ? response.result.value : undefined;
}

const PAGE_HARNESS = `(() => {
    const peers = new Map();

    function setCodecPreferences(transceiver, kind, preferredMimeTypes) {
        if (!transceiver.setCodecPreferences || !RTCRtpSender.getCapabilities) {
            return;
        }

        const capabilities = RTCRtpSender.getCapabilities(kind);
        if (!capabilities || !capabilities.codecs) {
            return;
        }

        const preferred = [];
        for (const mimeType of preferredMimeTypes) {
            const normalized = mimeType.toLowerCase();
            preferred.push(...capabilities.codecs.filter((codec) => codec.mimeType.toLowerCase() === normalized));
        }
        if (preferred.length) {
            transceiver.setCodecPreferences(preferred);
        }
    }

    function createPeer(peerId) {
        const peer = new RTCPeerConnection({
            bundlePolicy: 'max-bundle',
            iceServers: [],
        });
        const audio = peer.addTransceiver('audio', { direction: 'sendrecv' });
        const video = peer.addTransceiver('video', { direction: 'sendrecv' });
        setCodecPreferences(audio, 'audio', ['audio/opus', 'audio/PCMU', 'audio/PCMA', 'audio/G722']);
        setCodecPreferences(video, 'video', ['video/VP8']);
        peers.set(peerId, peer);
        return peer;
    }

    function getPeer(peerId) {
        const peer = peers.get(peerId);
        if (!peer) {
            throw new Error('Unknown RTCPeerConnection: ' + peerId);
        }
        return peer;
    }

    function state(peer) {
        return {
            signalingState: peer.signalingState,
            connectionState: peer.connectionState,
            iceConnectionState: peer.iceConnectionState,
            iceGatheringState: peer.iceGatheringState,
        };
    }

    function waitForIceGatheringComplete(peer, timeoutMs) {
        if (peer.iceGatheringState === 'complete') {
            return Promise.resolve();
        }
        return new Promise((resolve, reject) => {
            const timeout = setTimeout(() => {
                peer.removeEventListener('icegatheringstatechange', onChange);
                reject(new Error('Timed out waiting for browser ICE gathering'));
            }, timeoutMs);
            function onChange() {
                if (peer.iceGatheringState === 'complete') {
                    clearTimeout(timeout);
                    peer.removeEventListener('icegatheringstatechange', onChange);
                    resolve();
                }
            }
            peer.addEventListener('icegatheringstatechange', onChange);
        });
    }

    function waitConnected(peer, timeoutMs) {
        if (peer.connectionState === 'connected' || peer.iceConnectionState === 'connected'
            || peer.iceConnectionState === 'completed') {
            return Promise.resolve(state(peer));
        }
        return new Promise((resolve, reject) => {
            const timeout = setTimeout(() => {
                peer.removeEventListener('connectionstatechange', onChange);
                peer.removeEventListener('iceconnectionstatechange', onChange);
                reject(new Error('Timed out waiting for browser connectivity: ' + JSON.stringify(state(peer))));
            }, timeoutMs);
            function onChange() {
                if (peer.connectionState === 'failed' || peer.iceConnectionState === 'failed') {
                    clearTimeout(timeout);
                    peer.removeEventListener('connectionstatechange', onChange);
                    peer.removeEventListener('iceconnectionstatechange', onChange);
                    reject(new Error('Browser WebRTC connection failed: ' + JSON.stringify(state(peer))));
                    return;
                }
                if (peer.connectionState === 'connected' || peer.iceConnectionState === 'connected'
                    || peer.iceConnectionState === 'completed') {
                    clearTimeout(timeout);
                    peer.removeEventListener('connectionstatechange', onChange);
                    peer.removeEventListener('iceconnectionstatechange', onChange);
                    resolve(state(peer));
                }
            }
            peer.addEventListener('connectionstatechange', onChange);
            peer.addEventListener('iceconnectionstatechange', onChange);
            onChange();
        });
    }

    async function transportState(peer) {
        const transports = [];
        const stats = await peer.getStats();
        for (const report of stats.values()) {
            if (report.type !== 'transport') {
                continue;
            }
            transports.push({
                dtlsRole: report.dtlsRole || '',
                dtlsState: report.dtlsState || '',
                iceState: report.iceState || '',
                selectedCandidatePairId: report.selectedCandidatePairId || '',
                bytesReceived: report.bytesReceived || 0,
                bytesSent: report.bytesSent || 0,
            });
        }
        return { ...state(peer), transports };
    }

    async function waitTransportConnected(peer, timeoutMs) {
        const deadline = Date.now() + timeoutMs;
        while (true) {
            const snapshot = await transportState(peer);
            if (snapshot.connectionState === 'connected'
                || snapshot.transports.some((transport) => transport.dtlsState === 'connected')) {
                return snapshot;
            }
            if (snapshot.connectionState === 'failed' || snapshot.iceConnectionState === 'failed'
                || snapshot.transports.some((transport) => transport.dtlsState === 'failed')) {
                throw new Error('Browser WebRTC transport failed: ' + JSON.stringify(snapshot));
            }
            if (Date.now() >= deadline) {
                throw new Error('Timed out waiting for browser DTLS connectivity: ' + JSON.stringify(snapshot));
            }
            await new Promise((resolve) => setTimeout(resolve, 50));
        }
    }

    window.__jamiInterop = {
        makePeer(peerId) {
            createPeer(peerId);
            return true;
        },
        async createOffer(peerId) {
            const peer = getPeer(peerId);
            await peer.setLocalDescription(await peer.createOffer());
            await waitForIceGatheringComplete(peer, 10000);
            return { sdp: peer.localDescription.sdp, ...state(peer) };
        },
        async createAnswer(peerId, waitForIceGathering) {
            const peer = getPeer(peerId);
            await peer.setLocalDescription(await peer.createAnswer());
            if (waitForIceGathering !== false) {
                await waitForIceGatheringComplete(peer, 10000);
            }
            return { sdp: peer.localDescription.sdp, ...state(peer) };
        },
        async setRemote(peerId, type, sdp) {
            const peer = getPeer(peerId);
            await peer.setRemoteDescription({ type, sdp });
            return state(peer);
        },
        waitConnected(peerId, timeoutMs) {
            return waitConnected(getPeer(peerId), timeoutMs);
        },
        waitTransportConnected(peerId, timeoutMs) {
            return waitTransportConnected(getPeer(peerId), timeoutMs);
        },
        closePeer(peerId) {
            const peer = getPeer(peerId);
            peer.close();
            peers.delete(peerId);
            return state(peer);
        },
    };
    return true;
})()`;

async function callPage(client, sessionId, method, ...args) {
    const expression = `window.__jamiInterop[${JSON.stringify(method)}](...${JSON.stringify(args)})`;
    return evaluate(client, sessionId, expression);
}

async function createBrowserSession(browserPath) {
    const browser = await launchBrowser(browserPath);
    const client = await CdpClient.connect(browser.browserWsUrl);
    const target = await client.send('Target.createTarget', { url: 'about:blank' });
    const attached = await client.send('Target.attachToTarget', { targetId: target.targetId, flatten: true });
    const sessionId = attached.sessionId;
    await client.send('Runtime.enable', {}, sessionId);
    await evaluate(client, sessionId, PAGE_HARNESS);

    const close = async () => {
        try {
            await client.send('Browser.close');
        } catch (error) {
            browser.child.kill();
        }
        client.close();
        await new Promise((resolve) => browser.child.once('close', resolve));
        browser.cleanupProfile();
    };

    return { client, sessionId, close };
}

async function validateBrowserOfferToJamiAnswer(browserSession, bridgePath) {
    const peerId = 'browser-offer-to-jami-answer';
    await callPage(browserSession.client, browserSession.sessionId, 'makePeer', peerId);
    try {
        const offer = await callPage(browserSession.client, browserSession.sessionId, 'createOffer', peerId);
        assert(offer.sdp.includes('a=group:BUNDLE'), 'Browser offer did not advertise BUNDLE');
        assert(offer.sdp.includes('urn:ietf:params:rtp-hdrext:sdes:mid'), 'Browser offer did not advertise MID extmap');
        assertMediaTransport(offer.sdp, 'audio', 'Browser offer audio transport mismatch');
        assertMediaTransport(offer.sdp, 'video', 'Browser offer video transport mismatch');

        const { stdout: jamiAnswer } = await runJami(bridgePath, 'answer', offer.sdp);
        assert(jamiAnswer.includes('a=group:BUNDLE'), 'Jami answer did not advertise BUNDLE');
        assert(jamiAnswer.includes('a=rtcp-mux'), 'Jami answer did not advertise rtcp-mux');

        const state = await callPage(browserSession.client, browserSession.sessionId, 'setRemote', peerId, 'answer', jamiAnswer);
        assert(state.signalingState === 'stable', 'Browser did not accept Jami answer');

        return {
            offerLength: offer.sdp.length,
            answerLength: jamiAnswer.length,
            browserSignalingState: state.signalingState,
        };
    } finally {
        await callPage(browserSession.client, browserSession.sessionId, 'closePeer', peerId).catch(() => { });
    }
}

async function validateJamiOfferToBrowserAnswer(browserSession, bridgePath) {
    const peerId = 'jami-offer-to-browser-answer';
    await callPage(browserSession.client, browserSession.sessionId, 'makePeer', peerId);
    try {
        const { stdout: jamiOffer } = await runJami(bridgePath, 'offer');
        assert(jamiOffer.includes('a=group:BUNDLE'), 'Jami offer did not advertise BUNDLE');
        assert(jamiOffer.includes('urn:ietf:params:rtp-hdrext:sdes:mid'), 'Jami offer did not advertise MID extmap');
        assertMediaTransport(jamiOffer, 'audio', 'Jami offer audio transport mismatch');
        assertMediaTransport(jamiOffer, 'video', 'Jami offer video transport mismatch');

        await callPage(browserSession.client, browserSession.sessionId, 'setRemote', peerId, 'offer', jamiOffer);
        const answer = await callPage(browserSession.client, browserSession.sessionId, 'createAnswer', peerId, false);
        assert(answer.sdp.includes('a=group:BUNDLE'), 'Browser answer did not advertise BUNDLE');
        assert(answer.sdp.includes('a=rtcp-mux'), 'Browser answer did not advertise rtcp-mux');

        return {
            offerLength: jamiOffer.length,
            answerLength: answer.sdp.length,
            browserSignalingState: answer.signalingState,
        };
    } finally {
        await callPage(browserSession.client, browserSession.sessionId, 'closePeer', peerId).catch(() => { });
    }
}

async function finishLiveFlow(browserSession, peerId, session) {
    await callPage(browserSession.client, browserSession.sessionId, 'waitConnected', peerId, 20000);
    await waitForLiveConnected(session);
    const browserConnected = await callPage(browserSession.client,
        browserSession.sessionId,
        'waitTransportConnected',
        peerId,
        20000);
    session.startRtp();
    const result = await waitForLiveResult(session);
    const browserTransport = browserConnected.transports[0] || {};

    return {
        browserConnectionState: browserConnected.connectionState,
        browserIceConnectionState: browserConnected.iceConnectionState,
        browserDtlsState: browserTransport.dtlsState || '',
        browserDtlsRole: browserTransport.dtlsRole || '',
        bridgeIce: result.fields.ice,
        bridgeDtls: result.fields.dtls,
        bridgeRtpSent: result.fields.rtp_sent,
        remotePacketBytes: Number(result.fields.remote_packet_bytes || '0'),
        remotePacketCount: Number(result.fields.remote_packet_count || '0'),
        remoteRtpBytes: Number(result.fields.remote_rtp_bytes || '0'),
        remoteRtcpBytes: Number(result.fields.remote_rtcp_bytes || '0'),
    };
}

async function validateBrowserOfferToJamiLive(browserSession, bridgePath) {
    const peerId = 'browser-offer-to-jami-live';
    const session = startJamiLive(bridgePath, 'live-answer');
    await callPage(browserSession.client, browserSession.sessionId, 'makePeer', peerId);
    try {
        const offer = await callPage(browserSession.client, browserSession.sessionId, 'createOffer', peerId);
        assert(offer.sdp.includes('a=group:BUNDLE'), 'Browser live offer did not advertise BUNDLE');
        assertUsableIceCandidates(offer.sdp, 'Browser live offer');
        session.sendSdp('offer', offer.sdp);

        const answerLine = parseBridgeLine(await session.nextLine());
        assert(answerLine.name === 'SDP', `Unexpected bridge SDP event: ${answerLine.name}`);
        assert(answerLine.fields.type === 'answer', 'Bridge did not send a live answer');
        const jamiAnswer = Buffer.from(answerLine.fields.payload, 'base64').toString('utf8');
        await callPage(browserSession.client, browserSession.sessionId, 'setRemote', peerId, 'answer', jamiAnswer);
        const live = await finishLiveFlow(browserSession, peerId, session);

        return {
            offerLength: offer.sdp.length,
            answerLength: jamiAnswer.length,
            ...live,
        };
    } finally {
        await callPage(browserSession.client, browserSession.sessionId, 'closePeer', peerId).catch(() => { });
        await session.close();
    }
}

async function validateJamiOfferToBrowserLive(browserSession, bridgePath) {
    const peerId = 'jami-offer-to-browser-live';
    const session = startJamiLive(bridgePath, 'live-offer');
    await callPage(browserSession.client, browserSession.sessionId, 'makePeer', peerId);
    try {
        const offerLine = parseBridgeLine(await session.nextLine());
        assert(offerLine.name === 'SDP', `Unexpected bridge SDP event: ${offerLine.name}`);
        assert(offerLine.fields.type === 'offer', 'Bridge did not send a live offer');
        const jamiOffer = Buffer.from(offerLine.fields.payload, 'base64').toString('utf8');
        await callPage(browserSession.client, browserSession.sessionId, 'setRemote', peerId, 'offer', jamiOffer);

        const answer = await callPage(browserSession.client, browserSession.sessionId, 'createAnswer', peerId);
        assert(answer.sdp.includes('a=group:BUNDLE'), 'Browser live answer did not advertise BUNDLE');
        assertUsableIceCandidates(answer.sdp, 'Browser live answer');
        session.sendSdp('answer', answer.sdp);
        const live = await finishLiveFlow(browserSession, peerId, session);

        return {
            offerLength: jamiOffer.length,
            answerLength: answer.sdp.length,
            ...live,
        };
    } finally {
        await callPage(browserSession.client, browserSession.sessionId, 'closePeer', peerId).catch(() => { });
        await session.close();
    }
}

(async () => {
    const bridgePath = resolveBridgePath();
    const browserPath = resolveBrowserPath();
    const browserSession = await createBrowserSession(browserPath);
    try {
        const results = {
            bridge: bridgePath,
            browser: browserPath,
            browserOfferToJamiAnswer: await validateBrowserOfferToJamiAnswer(browserSession, bridgePath),
            jamiOfferToBrowserAnswer: await validateJamiOfferToBrowserAnswer(browserSession, bridgePath),
            browserOfferToJamiLive: await validateBrowserOfferToJamiLive(browserSession, bridgePath),
            jamiOfferToBrowserLive: await validateJamiOfferToBrowserLive(browserSession, bridgePath),
        };

        process.stdout.write(`${JSON.stringify(results, null, 2)}\n`);
    } finally {
        await browserSession.close();
    }
})().catch((error) => {
    process.stderr.write(`${error.stack || error.message}\n`);
    process.exit(1);
});