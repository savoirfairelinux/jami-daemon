const fs = require('fs');
const path = require('path');
const { spawn } = require('child_process');
const {
    RtcpRrPacket,
    RTCPeerConnection,
    RTCRtpHeaderExtensionParameters,
    RTP_EXTENSION_URI,
    RtpHeader,
    RtpPacket,
} = require('werift');

function onceEvent(register) {
    return new Promise((resolve) => register(resolve));
}

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

function makePeer() {
    return new RTCPeerConnection({
        bundlePolicy: 'max-bundle',
        iceServers: [],
        iceUseIpv6: false,
        headerExtensions: {
            audio: [new RTCRtpHeaderExtensionParameters({ uri: RTP_EXTENSION_URI.sdesMid })],
            video: [new RTCRtpHeaderExtensionParameters({ uri: RTP_EXTENSION_URI.sdesMid })],
        },
    });
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
        child,
        sendSdp,
        startRtp,
        nextLine,
        close,
        getStderr: () => stderr,
    };
}

function parseBridgeLine(line) {
    const [name, ...pairs] = line.split(' ');
    const fields = {};
    for (const pair of pairs) {
        const index = pair.indexOf('=');
        if (index <= 0) {
            continue;
        }
        fields[pair.slice(0, index)] = pair.slice(index + 1);
    }
    return { name, fields };
}

async function waitForConnection(pc) {
    if (pc.iceConnectionState === 'connected' || pc.iceConnectionState === 'completed') {
        return;
    }

    await Promise.race([
        onceEvent((resolve) => {
            const maybeResolve = () => {
                if (pc.iceConnectionState === 'connected' || pc.iceConnectionState === 'completed') {
                    resolve();
                }
            };
            pc.connectionStateChange.subscribe(maybeResolve);
            pc.iceConnectionStateChange.subscribe(maybeResolve);
            maybeResolve();
        }),
        new Promise((_, reject) => {
            setTimeout(() => reject(new Error('Timed out waiting for live ICE connectivity')), 15000);
        }),
    ]);
}

async function waitForWeriftSrtp(dtlsTransport) {
    const deadline = Date.now() + 5000;
    while (Date.now() < deadline) {
        if (dtlsTransport.state === 'connected' && dtlsTransport.srtp && dtlsTransport.srtcp) {
            return;
        }
        await new Promise((resolve) => setTimeout(resolve, 10));
    }
    throw new Error(`Timed out waiting for werift SRTP context (state=${dtlsTransport.state}, srtp=${Boolean(dtlsTransport.srtp)}, srtcp=${Boolean(dtlsTransport.srtcp)})`);
}

async function waitForWeriftNominatedPair(dtlsTransport) {
    const deadline = Date.now() + 5000;
    while (Date.now() < deadline) {
        if (dtlsTransport.iceTransport.connection.nominated) {
            return;
        }
        await new Promise((resolve) => setTimeout(resolve, 10));
    }
    throw new Error('Timed out waiting for werift nominated ICE pair');
}

async function waitForLiveConnected(session) {
    while (true) {
        const line = await session.nextLine();
        const event = parseBridgeLine(line);
        if (event.name === 'CONNECTED' && event.fields.dtls === 'true') {
            return event;
        }
        if (event.name === 'CONNECTED') {
            continue;
        }
        throw new Error(`Unexpected bridge event before DTLS connectivity: ${line}`);
    }
}

async function waitForLiveResult(session) {
    while (true) {
        const line = await session.nextLine();
        const event = parseBridgeLine(line);
        if (event.name === 'CONNECTED') {
            continue;
        }
        if (event.name === 'RTP_SENT') {
            return event;
        }
        if (event.name === 'RESULT') {
            return event;
        }
        throw new Error(`Unexpected bridge event: ${line}`);
    }
}

function waitForRtpFromJami(pc) {
    const dtlsTransport = pc.dtlsTransports[0];
    assert(dtlsTransport, 'werift did not expose a DTLS transport');
    let rawMediaPackets = 0;
    let lastRawMediaBytes = 0;
    let lastRawMediaPrefix = '';
    let lastRawMediaDecrypt = 'not-attempted';
    const rawSubscription = dtlsTransport.iceTransport.connection.onData.subscribe((data) => {
        if (data.length > 1 && (data[0] & 0xc0) === 0x80) {
            rawMediaPackets += 1;
            lastRawMediaBytes = data.length;
            lastRawMediaPrefix = data.subarray(0, Math.min(8, data.length)).toString('hex');
            setImmediate(() => {
                if (!dtlsTransport.srtp) {
                    lastRawMediaDecrypt = 'srtp-not-ready';
                    return;
                }
                try {
                    const decrypted = dtlsTransport.srtp.decrypt(data);
                    const rtp = RtpPacket.deSerialize(decrypted);
                    lastRawMediaDecrypt = `ok:pt=${rtp.header.payloadType}:ssrc=${rtp.header.ssrc}`;
                } catch (error) {
                    lastRawMediaDecrypt = `${error.name || 'Error'}:${error.message}`;
                }
            });
        }
    });

    return {
        dtlsTransport,
        rtpPromise: Promise.race([
            onceEvent((resolve) => dtlsTransport.onRtp.subscribe(resolve)),
            new Promise((_, reject) => {
                setTimeout(
                    () => reject(new Error(`Timed out waiting for SRTP from Jami (rawMediaPackets=${rawMediaPackets}, lastRawMediaBytes=${lastRawMediaBytes}, lastRawMediaPrefix=${lastRawMediaPrefix}, lastRawMediaDecrypt=${lastRawMediaDecrypt})`)),
                    5000);
            }),
        ]).finally(() => rawSubscription.unSubscribe()),
    };
}

async function finalizeLiveFlow(pc, session, liveEvent, rtpPromise, dtlsTransport) {
    if (liveEvent.name !== 'RTP_SENT') {
        throw new Error(`Unexpected bridge event before RTP: ${liveEvent.name}`);
    }

    const nominatedPair = dtlsTransport.iceTransport.connection.nominated;
    const sentBefore = nominatedPair ? nominatedPair.packetsSent : 0;
    await dtlsTransport.sendRtcp([new RtcpRrPacket({ ssrc: 0x10203040, reports: [] })]);
    const sentAfterRtcp = nominatedPair ? nominatedPair.packetsSent : 0;
    assert(sentAfterRtcp > sentBefore, 'werift did not send encrypted return RTCP');
    const returnRtpBytes = await dtlsTransport.sendRtp(Buffer.from([0xde, 0xad, 0xbe, 0xef]), new RtpHeader({
        payloadType: 111,
        sequenceNumber: 0x2468,
        timestamp: 0x01020304,
        ssrc: 0x10203040,
    }));
    const sentAfter = nominatedPair ? nominatedPair.packetsSent : 0;
    assert(returnRtpBytes > 0, 'werift did not send encrypted return RTP');
    await rtpPromise;

    const result = parseBridgeLine(await session.nextLine());
    assert(result.name === 'RESULT', `Unexpected bridge result line: ${result.name}`);
    assert(result.fields.ice === 'true', 'Jami live bridge did not confirm ICE success');
    assert(result.fields.dtls === 'true', 'Jami live bridge did not confirm DTLS success');
    assert(result.fields.rtp_sent === 'true', 'Jami live bridge did not confirm RTP send');
    assert(Number(result.fields.remote_rtcp_bytes || '0') > 0,
        `Jami live bridge did not observe encrypted RTCP return traffic: ${JSON.stringify(result.fields)}`);

    assert(pc.connectionState === 'connected', 'werift connectionState is not connected');
    assert(pc.iceConnectionState === 'connected', 'werift iceConnectionState is not connected');
    assert(dtlsTransport.state === 'connected', 'werift DTLS transport is not connected');

    return {
        bridgeIce: result.fields.ice,
        bridgeDtls: result.fields.dtls,
        remotePacketBytes: Number(result.fields.remote_packet_bytes),
        remotePacketCount: Number(result.fields.remote_packet_count),
        remoteRtpBytes: Number(result.fields.remote_rtp_bytes),
        remoteRtcpBytes: Number(result.fields.remote_rtcp_bytes),
        weriftReturnRtpBytes: returnRtpBytes,
        weriftReturnPacketsSent: sentAfter - sentBefore,
        weriftReturnRtcpPacketsSent: sentAfterRtcp - sentBefore,
        weriftConnectionState: pc.connectionState,
        weriftIceConnectionState: pc.iceConnectionState,
        weriftDtlsState: dtlsTransport.state,
    };
}

async function validateWeriftOfferToJamiAnswer(bridgePath) {
    const pc = makePeer();

    try {
        pc.addTransceiver('audio', { direction: 'sendrecv' });
        pc.addTransceiver('video', { direction: 'sendrecv' });

        const offer = await pc.createOffer();
        await pc.setLocalDescription(offer);

        const localOffer = pc.localDescription && pc.localDescription.sdp;
        assert(localOffer && localOffer.includes('a=group:BUNDLE'), 'werift offer did not advertise BUNDLE');
        assert(localOffer.includes('urn:ietf:params:rtp-hdrext:sdes:mid'), 'werift offer did not advertise MID extmap');

        const { stdout: jamiAnswer } = await runJami(bridgePath, 'answer', localOffer);
        assert(jamiAnswer.includes('a=group:BUNDLE'), 'Jami answer did not advertise BUNDLE');
        assert(jamiAnswer.includes('a=mid:0'), 'Jami answer missing mid 0');
        assert(jamiAnswer.includes('a=mid:1'), 'Jami answer missing mid 1');
        assertMediaTransport(jamiAnswer, 'audio', 'Jami answer audio transport mismatch');
        assertMediaTransport(jamiAnswer, 'video', 'Jami answer video transport mismatch');

        await pc.setRemoteDescription({ type: 'answer', sdp: jamiAnswer });
        assert(pc.signalingState === 'stable', 'werift did not accept Jami answer');
        assert(pc.remoteIsBundled, 'werift did not recognize Jami bundled answer');

        return {
            offerLength: localOffer.length,
            answerLength: jamiAnswer.length,
        };
    } finally {
        await pc.close();
    }
}

async function validateJamiOfferToWeriftAnswer(bridgePath) {
    const { stdout: jamiOffer } = await runJami(bridgePath, 'offer');
    assert(jamiOffer.includes('a=group:BUNDLE'), 'Jami offer did not advertise BUNDLE');
    assert(jamiOffer.includes('urn:ietf:params:rtp-hdrext:sdes:mid'), 'Jami offer did not advertise MID extmap');
    assertMediaTransport(jamiOffer, 'audio', 'Jami offer audio transport mismatch');
    assertMediaTransport(jamiOffer, 'video', 'Jami offer video transport mismatch');

    const pc = makePeer();

    try {
        pc.addTransceiver('audio', { direction: 'sendrecv' });
        pc.addTransceiver('video', { direction: 'sendrecv' });

        await pc.setRemoteDescription({ type: 'offer', sdp: jamiOffer });
        assert(pc.signalingState === 'have-remote-offer', 'werift did not accept Jami offer');
        assert(pc.remoteIsBundled, 'werift did not recognize Jami bundled offer');

        const answer = await pc.createAnswer();
        await pc.setLocalDescription(answer);

        const localAnswer = pc.localDescription && pc.localDescription.sdp;
        assert(localAnswer && localAnswer.includes('a=group:BUNDLE'), 'werift answer did not advertise BUNDLE');
        assert(localAnswer.includes('a=mid:audio_0'), 'werift answer did not preserve Jami audio MID');
        assert(localAnswer.includes('a=mid:video_0'), 'werift answer did not preserve Jami video MID');

        return {
            offerLength: jamiOffer.length,
            answerLength: localAnswer.length,
        };
    } finally {
        await pc.close();
    }
}

async function validateWeriftOfferToJamiLive(bridgePath) {
    const session = startJamiLive(bridgePath, 'live-answer');
    const pc = makePeer();

    try {
        pc.addTransceiver('audio', { direction: 'sendrecv' });
        pc.addTransceiver('video', { direction: 'sendrecv' });

        const offer = await pc.createOffer();
        await pc.setLocalDescription(offer);

        const localOffer = pc.localDescription && pc.localDescription.sdp;
        assert(localOffer && localOffer.includes('a=group:BUNDLE'), 'werift live offer did not advertise BUNDLE');

        session.sendSdp('offer', localOffer);

        const answerLine = parseBridgeLine(await session.nextLine());
        assert(answerLine.name === 'SDP', `Unexpected bridge SDP event: ${answerLine.name}`);
        assert(answerLine.fields.type === 'answer', 'Bridge did not send a live answer');

        const jamiAnswer = Buffer.from(answerLine.fields.payload, 'base64').toString('utf8');
        await pc.setRemoteDescription({ type: 'answer', sdp: jamiAnswer });
        const dtlsTransport = pc.dtlsTransports[0];
        assert(dtlsTransport, 'werift did not expose a DTLS transport');
        await waitForConnection(pc);
        await waitForLiveConnected(session);
        await waitForWeriftSrtp(dtlsTransport);
        await waitForWeriftNominatedPair(dtlsTransport);
        const { rtpPromise } = waitForRtpFromJami(pc);
        session.startRtp();

        const liveEvent = await waitForLiveResult(session);
        const live = await finalizeLiveFlow(pc, session, liveEvent, rtpPromise, dtlsTransport);

        return {
            offerLength: localOffer.length,
            answerLength: jamiAnswer.length,
            ...live,
        };
    } finally {
        await pc.close();
        await session.close();
    }
}

async function validateJamiOfferToWeriftLive(bridgePath) {
    const session = startJamiLive(bridgePath, 'live-offer');
    const pc = makePeer();

    try {
        pc.addTransceiver('audio', { direction: 'sendrecv' });
        pc.addTransceiver('video', { direction: 'sendrecv' });

        const offerLine = parseBridgeLine(await session.nextLine());
        assert(offerLine.name === 'SDP', `Unexpected bridge SDP event: ${offerLine.name}`);
        assert(offerLine.fields.type === 'offer', 'Bridge did not send a live offer');

        const jamiOffer = Buffer.from(offerLine.fields.payload, 'base64').toString('utf8');
        await pc.setRemoteDescription({ type: 'offer', sdp: jamiOffer });

        const answer = await pc.createAnswer();
        await pc.setLocalDescription(answer);

        const localAnswer = pc.localDescription && pc.localDescription.sdp;
        assert(localAnswer && localAnswer.includes('a=group:BUNDLE'), 'werift live answer did not advertise BUNDLE');
        const dtlsTransport = pc.dtlsTransports[0];
        assert(dtlsTransport, 'werift did not expose a DTLS transport');
        session.sendSdp('answer', localAnswer);

        await waitForConnection(pc);
        await waitForLiveConnected(session);
        await waitForWeriftSrtp(dtlsTransport);
        await waitForWeriftNominatedPair(dtlsTransport);
        const { rtpPromise } = waitForRtpFromJami(pc);
        session.startRtp();
        const liveEvent = await waitForLiveResult(session);
        const live = await finalizeLiveFlow(pc, session, liveEvent, rtpPromise, dtlsTransport);

        return {
            offerLength: jamiOffer.length,
            answerLength: localAnswer.length,
            ...live,
        };
    } finally {
        await pc.close();
        await session.close();
    }
}

(async () => {
    const bridgePath = resolveBridgePath();
    const results = {
        bridge: bridgePath,
        weriftOfferToJamiAnswer: await validateWeriftOfferToJamiAnswer(bridgePath),
        jamiOfferToWeriftAnswer: await validateJamiOfferToWeriftAnswer(bridgePath),
        weriftOfferToJamiLive: await validateWeriftOfferToJamiLive(bridgePath),
        jamiOfferToWeriftLive: await validateJamiOfferToWeriftLive(bridgePath),
    };

    process.stdout.write(`${JSON.stringify(results, null, 2)}\n`);
})().catch((error) => {
    process.stderr.write(`${error.stack || error.message}\n`);
    process.exit(1);
});