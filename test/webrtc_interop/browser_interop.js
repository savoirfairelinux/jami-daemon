const fs = require('fs');
const os = require('os');
const path = require('path');
const { spawn } = require('child_process');

const BRIDGE_EVENTS = new Set(['SDP', 'CONNECTED', 'RTP_SENT', 'RESULT']);
const BRIDGE_EVENT_TIMEOUT_MS = 30000;
const BRIDGE_CLOSE_TIMEOUT_MS = 2000;
const TRANSPORT_CC_URI = 'http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01';

function assert(condition, message) {
    if (!condition) {
        throw new Error(message);
    }
}

function assertMediaTransport(sdp, kind, message) {
    const pattern = new RegExp(`m=${kind}\\s+\\d+\\s+UDP/TLS/RTP/SAVPF`);
    assert(pattern.test(sdp), message);
}

function assertTransportCc(sdp, message) {
    assert(sdp.includes(TRANSPORT_CC_URI), `${message}: missing transport-cc extmap`);
    assert(/a=rtcp-fb:(\*|\d+) transport-cc/.test(sdp), `${message}: missing rtcp-fb transport-cc`);
}

function assertVideoPli(sdp, message) {
    assert(/a=rtcp-fb:(\*|\d+) nack pli/.test(sdp), `${message}: missing rtcp-fb nack pli`);
}

function stripGoogRemb(sdp) {
    return sdp
        .split(/\r?\n/)
        .filter((line) => !/^a=rtcp-fb:(\*|\d+) goog-remb$/i.test(line.trim()))
        .join('\r\n');
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

    const nextLine = (timeoutMs = BRIDGE_EVENT_TIMEOUT_MS) => new Promise((resolve, reject) => {
        if (queue.length) {
            resolve(queue.shift());
            return;
        }
        if (child.exitCode !== null) {
            reject(new Error(`jami_webrtc_sdp_bridge ${mode} exited early (${child.exitCode}): ${stderr.trim()}`));
            return;
        }
        const onExit = (code) => reject(new Error(`jami_webrtc_sdp_bridge ${mode} exited early (${code}): ${stderr.trim()}`));
        const resolver = (line) => {
            clearTimeout(timeout);
            child.off('close', onExit);
            resolve(line);
        };
        const timeout = setTimeout(() => {
            const index = waiters.indexOf(resolver);
            if (index >= 0) {
                waiters.splice(index, 1);
            }
            child.off('close', onExit);
            reject(new Error(`Timed out waiting for jami_webrtc_sdp_bridge ${mode} event (stderr=${stderr.trim()}, buffered=${buffered.trim()})`));
        }, timeoutMs);
        child.once('close', onExit);
        waiters.push(resolver);
    });

    const sendSdp = (type, sdp) => {
        const payload = Buffer.from(sdp, 'utf8').toString('base64');
        child.stdin.write(`SDP ${type} ${payload}\n`);
    };

    const startRtp = (profile = 'default') => {
        child.stdin.write(profile === 'default' ? 'START_RTP\n' : `START_RTP ${profile}\n`);
    };

    const close = () => new Promise((resolve) => {
        if (child.exitCode !== null) {
            resolve();
            return;
        }
        const timeout = setTimeout(() => {
            child.kill();
            resolve();
        }, BRIDGE_CLOSE_TIMEOUT_MS);
        child.once('close', () => {
            clearTimeout(timeout);
            resolve();
        });
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

async function waitForLiveIceReady(session) {
    while (true) {
        const event = parseBridgeLine(await session.nextLine());
        if (event.name === 'CONNECTED' && event.fields.dtls === 'pending') {
            return event;
        }
        if (event.name === 'CONNECTED' && event.fields.dtls === 'true') {
            return event;
        }
        throw new Error(`Unexpected bridge event before ICE readiness: ${event.name}`);
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

    function createSyntheticVideoSource() {
        const canvas = document.createElement('canvas');
        canvas.width = 160;
        canvas.height = 90;
        const context = canvas.getContext('2d');
        let frame = 0;
        function draw() {
            context.fillStyle = frame % 2 ? '#264653' : '#2a9d8f';
            context.fillRect(0, 0, canvas.width, canvas.height);
            context.fillStyle = '#ffffff';
            context.fillRect((frame * 7) % canvas.width, 20, 24, 24);
            frame += 1;
        }
        draw();
        const timer = setInterval(draw, 100);
        const stream = canvas.captureStream(10);
        const track = stream.getVideoTracks()[0];
        return {
            track,
            stop() {
                clearInterval(timer);
                track.stop();
            },
        };
    }

    function createPeer(peerId) {
        const peer = new RTCPeerConnection({
            bundlePolicy: 'max-bundle',
            iceServers: [],
        });
        const syntheticVideo = createSyntheticVideoSource();
        const remoteElements = [];
        peer.addEventListener('track', (event) => {
            const element = document.createElement(event.track.kind === 'audio' ? 'audio' : 'video');
            element.autoplay = true;
            element.muted = true;
            element.playsInline = true;
            element.srcObject = event.streams[0] || new MediaStream([event.track]);
            document.body.appendChild(element);
            remoteElements.push(element);
            element.play().catch(() => { });
        });
        const audio = peer.addTransceiver('audio', { direction: 'sendrecv' });
        const video = peer.addTransceiver(syntheticVideo.track, { direction: 'sendrecv' });
        peer.__jamiSyntheticMedia = [syntheticVideo];
        peer.__jamiRemoteElements = remoteElements;
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

    async function mediaStats(peer) {
        const reports = [];
        const stats = await peer.getStats();
        for (const report of stats.values()) {
            if (report.type !== 'inbound-rtp'
                && report.type !== 'outbound-rtp'
                && report.type !== 'remote-inbound-rtp'
                && report.type !== 'candidate-pair') {
                continue;
            }
            reports.push({
                type: report.type,
                kind: report.kind || report.mediaType || '',
                packetsReceived: report.packetsReceived || 0,
                packetsSent: report.packetsSent || 0,
                bytesReceived: report.bytesReceived || 0,
                bytesSent: report.bytesSent || 0,
                packetsLost: report.packetsLost || 0,
                jitter: report.jitter || 0,
                framesDecoded: report.framesDecoded || 0,
                framesEncoded: report.framesEncoded || 0,
                targetBitrate: report.targetBitrate || 0,
                totalRoundTripTime: report.totalRoundTripTime || 0,
                roundTripTimeMeasurements: report.roundTripTimeMeasurements || 0,
                availableOutgoingBitrate: report.availableOutgoingBitrate || 0,
                decoderImplementation: report.decoderImplementation || '',
                encoderImplementation: report.encoderImplementation || '',
                localId: report.localId || '',
                remoteId: report.remoteId || '',
            });
        }
        return reports;
    }

    async function waitForInboundRtp(peer, kind, timeoutMs) {
        const deadline = Date.now() + timeoutMs;
        while (true) {
            const reports = await mediaStats(peer);
            if (reports.some((report) => report.type === 'inbound-rtp'
                && report.kind === kind
                && report.packetsReceived > 0)) {
                return reports;
            }
            if (Date.now() >= deadline) {
                return reports;
            }
            await new Promise((resolve) => setTimeout(resolve, 50));
        }
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
        mediaStats(peerId) {
            return mediaStats(getPeer(peerId));
        },
        waitForInboundRtp(peerId, kind, timeoutMs) {
            return waitForInboundRtp(getPeer(peerId), kind, timeoutMs);
        },
        closePeer(peerId) {
            const peer = getPeer(peerId);
            for (const source of peer.__jamiSyntheticMedia || []) {
                source.stop();
            }
            for (const element of peer.__jamiRemoteElements || []) {
                element.remove();
            }
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
        assertTransportCc(offer.sdp, 'Browser offer did not advertise Transport-CC');
        assertMediaTransport(offer.sdp, 'audio', 'Browser offer audio transport mismatch');
        assertMediaTransport(offer.sdp, 'video', 'Browser offer video transport mismatch');

        const { stdout: jamiAnswer } = await runJami(bridgePath, 'answer', offer.sdp);
        assert(jamiAnswer.includes('a=group:BUNDLE'), 'Jami answer did not advertise BUNDLE');
        assert(jamiAnswer.includes('a=rtcp-mux'), 'Jami answer did not advertise rtcp-mux');
        assertTransportCc(jamiAnswer, 'Jami answer did not negotiate Transport-CC');
        assertVideoPli(offer.sdp, 'Browser offer did not advertise PLI');
        assertVideoPli(jamiAnswer, 'Jami answer did not negotiate PLI');

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
        assertTransportCc(jamiOffer, 'Jami offer did not advertise Transport-CC');
        assertVideoPli(jamiOffer, 'Jami offer did not advertise PLI');
        assertMediaTransport(jamiOffer, 'audio', 'Jami offer audio transport mismatch');
        assertMediaTransport(jamiOffer, 'video', 'Jami offer video transport mismatch');

        await callPage(browserSession.client, browserSession.sessionId, 'setRemote', peerId, 'offer', jamiOffer);
        const answer = await callPage(browserSession.client, browserSession.sessionId, 'createAnswer', peerId, false);
        assert(answer.sdp.includes('a=group:BUNDLE'), 'Browser answer did not advertise BUNDLE');
        assert(answer.sdp.includes('a=rtcp-mux'), 'Browser answer did not advertise rtcp-mux');
        assertTransportCc(answer.sdp, 'Browser answer did not negotiate Transport-CC');
        assertVideoPli(answer.sdp, 'Browser answer did not negotiate PLI');

        return {
            offerLength: jamiOffer.length,
            answerLength: answer.sdp.length,
            browserSignalingState: answer.signalingState,
        };
    } finally {
        await callPage(browserSession.client, browserSession.sessionId, 'closePeer', peerId).catch(() => { });
    }
}

function assertBandwidthTarget(result, chromeMediaStats = []) {
    const transportCcReports = Number(result.fields.transport_cc_reports || '0');
    const targetBitrateBps = Number(result.fields.target_bitrate_bps || '0');
    const acknowledgedBitrateBps = Number(result.fields.acknowledged_bitrate_bps || '0');
    const sentBitrateBps = Number(result.fields.sent_bitrate_bps || '0');
    const profileSentBitrateBps = Number(result.fields.profile_sent_bitrate_bps || '0');
    const expectedMaxSentBitrateBps = Number(result.fields.expected_max_sent_bitrate_bps || '0');
    const chromeTargetBitrateBps = Math.max(0,
        ...chromeMediaStats
            .filter((report) => report.type === 'outbound-rtp')
            .map((report) => Number(report.targetBitrate || 0)));
    const diagnostic = JSON.stringify({ bridge: result.fields, chromeMediaStats });
    assert(Number(result.fields.transport_cc_sent_packets || '0') > 0,
        `Jami did not stamp RTP packets with Transport-CC sequence numbers: ${diagnostic}`);
    assert(profileSentBitrateBps > 0,
        `Jami bandwidth profile did not report a shaped send bitrate: ${diagnostic}`);
    if (expectedMaxSentBitrateBps > 0) {
        assert(profileSentBitrateBps <= expectedMaxSentBitrateBps,
            `Jami RTP bandwidth profile was not shaped as expected: ${diagnostic}`);
    }
    if (transportCcReports > 0) {
        assert(targetBitrateBps >= 30000 && targetBitrateBps <= 5000000,
            `Jami Transport-CC target bitrate is outside configured bounds: ${diagnostic}`);
        assert(acknowledgedBitrateBps > 0,
            `Jami Transport-CC did not calculate acknowledged bitrate: ${diagnostic}`);
        assert(sentBitrateBps > 0,
            `Jami Transport-CC did not calculate sent bitrate: ${diagnostic}`);
    } else {
        assert(chromeTargetBitrateBps > 0,
            `Chrome did not expose an outbound target bitrate for the bandwidth scenario: ${diagnostic}`);
    }
}

async function finishLiveFlow(browserSession, peerId, session, profile = 'default') {
    await waitForLiveIceReady(session);
    session.startRtp(profile);
    const bridgeConnected = waitForLiveConnected(session);
    await callPage(browserSession.client, browserSession.sessionId, 'waitConnected', peerId, 20000);
    await bridgeConnected;
    const browserConnected = await callPage(browserSession.client,
        browserSession.sessionId,
        'waitTransportConnected',
        peerId,
        20000);
    const result = await waitForLiveResult(session);
    const chromeMediaStats = profile === 'bandwidth'
        ? await callPage(browserSession.client, browserSession.sessionId, 'waitForInboundRtp', peerId, 'video', 1000)
        : await callPage(browserSession.client, browserSession.sessionId, 'mediaStats', peerId);
    const chromeTargetBitrateBps = Math.max(0,
        ...chromeMediaStats
            .filter((report) => report.type === 'outbound-rtp')
            .map((report) => Number(report.targetBitrate || 0)));
    const chromeAvailableOutgoingBitrateBps = Math.max(0,
        ...chromeMediaStats
            .filter((report) => report.type === 'candidate-pair')
            .map((report) => Number(report.availableOutgoingBitrate || 0)));
    const browserTransport = browserConnected.transports[0] || {};
    const transportCcReports = Number(result.fields.transport_cc_reports || '0');
    assert(Number(result.fields.remote_packet_count || '0') > 0,
        `Jami live bridge did not observe browser return traffic: ${JSON.stringify(result.fields)}`);
    assert(Number(result.fields.remote_rtcp_bytes || '0') > 0 || transportCcReports > 0,
        `Jami live bridge did not observe browser return RTCP or Transport-CC: ${JSON.stringify(result.fields)}`);

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
        transportCcReports,
        targetBitrateBps: Number(result.fields.target_bitrate_bps || '0'),
        acknowledgedBitrateBps: Number(result.fields.acknowledged_bitrate_bps || '0'),
        sentBitrateBps: Number(result.fields.sent_bitrate_bps || '0'),
        profileSentBitrateBps: Number(result.fields.profile_sent_bitrate_bps || '0'),
        rtpPacketCount: Number(result.fields.rtp_packet_count || '0'),
        rtpPayloadBytes: Number(result.fields.rtp_payload_bytes || '0'),
        chromeTargetBitrateBps,
        chromeAvailableOutgoingBitrateBps,
        ...(profile === 'bandwidth' ? { bridgeFields: result.fields, chromeMediaStats } : {}),
    };
}

async function validateBrowserOfferToJamiLive(browserSession, bridgePath) {
    const peerId = 'browser-offer-to-jami-live';
    const session = startJamiLive(bridgePath, 'live-answer');
    await callPage(browserSession.client, browserSession.sessionId, 'makePeer', peerId);
    try {
        const offer = await callPage(browserSession.client, browserSession.sessionId, 'createOffer', peerId);
        assert(offer.sdp.includes('a=group:BUNDLE'), 'Browser live offer did not advertise BUNDLE');
        assertTransportCc(offer.sdp, 'Browser live offer did not advertise Transport-CC');
        assertUsableIceCandidates(offer.sdp, 'Browser live offer');
        session.sendSdp('offer', offer.sdp);

        const answerLine = parseBridgeLine(await session.nextLine());
        assert(answerLine.name === 'SDP', `Unexpected bridge SDP event: ${answerLine.name}`);
        assert(answerLine.fields.type === 'answer', 'Bridge did not send a live answer');
        const jamiAnswer = Buffer.from(answerLine.fields.payload, 'base64').toString('utf8');
        assertTransportCc(jamiAnswer, 'Jami live answer did not negotiate Transport-CC');
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
        assertTransportCc(jamiOffer, 'Jami live offer did not advertise Transport-CC');
        await callPage(browserSession.client, browserSession.sessionId, 'setRemote', peerId, 'offer', jamiOffer);

        const answer = await callPage(browserSession.client, browserSession.sessionId, 'createAnswer', peerId);
        assert(answer.sdp.includes('a=group:BUNDLE'), 'Browser live answer did not advertise BUNDLE');
        assertTransportCc(answer.sdp, 'Browser live answer did not negotiate Transport-CC');
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

async function validateBrowserOfferToJamiBandwidthQuality(browserSession, bridgePath) {
    const peerId = 'browser-offer-to-jami-bandwidth-quality';
    const session = startJamiLive(bridgePath, 'live-answer');
    await callPage(browserSession.client, browserSession.sessionId, 'makePeer', peerId);
    try {
        const offer = await callPage(browserSession.client, browserSession.sessionId, 'createOffer', peerId);
        assertTransportCc(offer.sdp, 'Browser bandwidth offer did not advertise Transport-CC');
        assertUsableIceCandidates(offer.sdp, 'Browser bandwidth offer');
        session.sendSdp('offer', stripGoogRemb(offer.sdp));

        const answerLine = parseBridgeLine(await session.nextLine());
        assert(answerLine.name === 'SDP', `Unexpected bridge SDP event: ${answerLine.name}`);
        assert(answerLine.fields.type === 'answer', 'Bridge did not send a live answer');
        const jamiAnswer = Buffer.from(answerLine.fields.payload, 'base64').toString('utf8');
        assertTransportCc(jamiAnswer, 'Jami bandwidth answer did not negotiate Transport-CC');
        await callPage(browserSession.client, browserSession.sessionId, 'setRemote', peerId, 'answer', jamiAnswer);

        const live = await finishLiveFlow(browserSession, peerId, session, 'bandwidth');
        assertBandwidthTarget({ fields: live.bridgeFields }, live.chromeMediaStats);
        const { bridgeFields, chromeMediaStats, ...publicLive } = live;

        return {
            offerLength: offer.sdp.length,
            answerLength: jamiAnswer.length,
            ...publicLive,
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
            browserOfferToJamiBandwidthQuality: await validateBrowserOfferToJamiBandwidthQuality(browserSession, bridgePath),
        };

        process.stdout.write(`${JSON.stringify(results, null, 2)}\n`);
    } finally {
        await browserSession.close();
    }
})().catch((error) => {
    process.stderr.write(`${error.stack || error.message}\n`);
    process.exit(1);
});