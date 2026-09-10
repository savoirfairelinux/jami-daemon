// External media interop harness: drives the libjami external media
// endpoint API end to end. A werift RTCPeerConnection acts as the browser
// endpoint, the external_media_bridge binary places a real Jami call
// carrying the browser offer, and the callee's media stack (a regular
// Jami account) connects ICE/DTLS directly to werift.
//
// Usage: node external_media_interop.js [path/to/jami_external_media_bridge]

const fs = require('fs');
const path = require('path');
const { spawn } = require('child_process');
const { RTCPeerConnection, useOPUS } = require('werift');

const BRIDGE_EVENTS = new Set(['READY', 'SDP', 'RESULT']);
const EVENT_TIMEOUT_MS = 120000;
const CONNECT_TIMEOUT_MS = 30000;
const RTP_TIMEOUT_MS = 20000;

function assert(condition, message) {
    if (!condition) {
        throw new Error(message);
    }
}

function resolveBridgePath() {
    const repoRoot = path.resolve(__dirname, '..', '..');
    const explicitPath = process.argv[2] || process.env.JAMI_EXTERNAL_MEDIA_BRIDGE;
    const candidates = explicitPath
        ? [path.resolve(explicitPath)]
        : [
            path.join(repoRoot, 'build', 'jami_external_media_bridge'),
            path.join(repoRoot, 'build-meson', 'jami_external_media_bridge'),
        ];
    const bridgePath = candidates.find((candidate) => fs.existsSync(candidate));
    if (!bridgePath) {
        throw new Error('Unable to find jami_external_media_bridge. Build it first or pass its path.');
    }
    return bridgePath;
}

function startBridge(bridgePath) {
    const child = spawn(bridgePath, [], {
        cwd: path.resolve(__dirname, '..', 'unitTest'),
        stdio: ['pipe', 'pipe', 'pipe'],
        env: { ...process.env, HOME: '/tmp' },
    });

    let stderr = '';
    const queue = [];
    const waiters = [];

    const stderrLogPath = process.env.JAMI_BRIDGE_STDERR;
    const stderrLog = stderrLogPath ? fs.createWriteStream(stderrLogPath) : null;

    child.stderr.on('data', (chunk) => {
        stderr += chunk.toString();
        if (stderrLog) {
            stderrLog.write(chunk);
        }
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

    const nextEvent = (timeoutMs = EVENT_TIMEOUT_MS) => new Promise((resolve, reject) => {
        if (queue.length) {
            resolve(queue.shift());
            return;
        }
        if (child.exitCode !== null) {
            reject(new Error(`bridge exited early (${child.exitCode}): ${stderr.slice(-2000)}`));
            return;
        }
        const onExit = (code) => reject(new Error(`bridge exited early (${code}): ${stderr.slice(-2000)}`));
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
            reject(new Error('timed out waiting for bridge event'));
        }, timeoutMs);
        child.once('close', onExit);
        waiters.push(resolver);
    });

    const send = (line) => child.stdin.write(`${line}\n`);
    const waitExit = () => new Promise((resolve) => {
        if (child.exitCode !== null) {
            resolve(child.exitCode);
            return;
        }
        child.once('close', resolve);
    });

    return { child, nextEvent, send, waitExit, getStderr: () => stderr };
}

function parseEvent(line) {
    const [name, ...parts] = line.split(' ');
    const fields = {};
    for (const part of parts) {
        const eq = part.indexOf('=');
        if (eq > 0) {
            fields[part.slice(0, eq)] = part.slice(eq + 1);
        }
    }
    return { name, fields };
}

function waitForConnection(pc, timeoutMs) {
    return new Promise((resolve, reject) => {
        if (pc.connectionState === 'connected') {
            resolve();
            return;
        }
        const timeout = setTimeout(
            () => reject(new Error(`werift connection timed out (state=${pc.connectionState})`)),
            timeoutMs,
        );
        pc.connectionStateChange.subscribe((state) => {
            if (state === 'connected') {
                clearTimeout(timeout);
                resolve();
            } else if (state === 'failed' || state === 'closed') {
                clearTimeout(timeout);
                reject(new Error(`werift connection ${state}`));
            }
        });
    });
}

function waitForRtp(pc, timeoutMs) {
    return new Promise((resolve, reject) => {
        const timeout = setTimeout(
            () => reject(new Error('timed out waiting for RTP from the Jami peer')),
            timeoutMs,
        );
        const dtlsTransport = pc.dtlsTransports[0];
        dtlsTransport.onRtp.subscribe((rtp) => {
            clearTimeout(timeout);
            resolve(rtp);
        });
    });
}

async function main() {
    const bridgePath = resolveBridgePath();
    const bridge = startBridge(bridgePath);

    const pc = new RTCPeerConnection({
        bundlePolicy: 'max-bundle',
        codecs: { audio: [useOPUS()], video: [] },
        iceServers: [],
        iceUseIpv6: false,
    });

    try {
        const ready = parseEvent(await bridge.nextEvent());
        assert(ready.name === 'READY', `expected READY, got ${ready.name}`);

        const transceiver = pc.addTransceiver('audio', { direction: 'sendrecv' });
        void transceiver;

        // Vanilla ICE: setLocalDescription resolves once gathering finishes.
        const offer = await pc.createOffer();
        await pc.setLocalDescription(offer);
        const localSdp = pc.localDescription.sdp;
        assert(localSdp.includes('a=candidate'), 'werift offer has no ICE candidates');

        bridge.send(`CALL offer ${Buffer.from(localSdp).toString('base64')}`);

        const sdpEvent = parseEvent(await bridge.nextEvent());
        assert(sdpEvent.name === 'SDP', `expected SDP, got ${sdpEvent.name}`);
        assert(sdpEvent.fields.type === 'answer', 'expected an SDP answer');
        const answer = Buffer.from(sdpEvent.fields.payload, 'base64').toString();
        assert(answer.includes('opus/48000/2'), 'answer has no opus');
        assert(/a=fingerprint/.test(answer), 'answer has no DTLS fingerprint');
        console.log('--- Jami answer ---\n' + answer);

        await pc.setRemoteDescription({ type: 'answer', sdp: answer });

        await waitForConnection(pc, CONNECT_TIMEOUT_MS);
        console.log('werift: ICE+DTLS connected to the Jami peer');

        const rtp = await waitForRtp(pc, RTP_TIMEOUT_MS);
        console.log(`werift: received RTP from the Jami peer (pt=${rtp.header.payloadType}, ssrc=${rtp.header.ssrc})`);

        bridge.send('DONE');
        const result = parseEvent(await bridge.nextEvent());
        assert(result.name === 'RESULT' && result.fields.status === 'ok',
            `bridge reported failure: ${JSON.stringify(result.fields)}`);

        const exitCode = await bridge.waitExit();
        assert(exitCode === 0, `bridge exited with ${exitCode}`);

        console.log('EXTERNAL_MEDIA_INTEROP_OK');
    } finally {
        try { await pc.close(); } catch (_) { /* ignore */ }
        if (bridge.child.exitCode === null) {
            bridge.child.kill();
        }
    }
}

main().catch((error) => {
    console.error('EXTERNAL_MEDIA_INTEROP_FAIL:', error.message);
    process.exit(1);
});
