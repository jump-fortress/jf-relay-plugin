import net from 'node:net';
import { encodeSnapshot, instances, selections } from './protocol.mjs';

const { PROGRESS_URL, PROGRESS_TOKEN, RELAY_SERVER_CODE } = process.env;

if (!PROGRESS_URL || !PROGRESS_TOKEN || !RELAY_SERVER_CODE) {
  throw new Error('PROGRESS_URL, PROGRESS_TOKEN and RELAY_SERVER_CODE are required');
}

const url = new URL(PROGRESS_URL);

if (url.protocol !== 'wss:') {
  throw new Error('PROGRESS_URL must use wss://');
}

url.searchParams.set('token', PROGRESS_TOKEN);
url.searchParams.set('channel', 'progress');

const endpoint = instance => process.platform === 'win32'
  ? `\\\\.\\pipe\\jf-spec-${instance}`
  : `/tmp/jf-spec-${process.getuid()}/rankings-${instance}.sock`;

const pipes = new Map();
const pipeReconnects = new Map();

let snapshot;
let socket;

let latest = 'CLEAR\n';
let lastSnapshotAt = 0;

let stopping = false;
let websocketReconnect;
let retries = 0;
let receivedSnapshot = false;

function send(pipe, line) {
  if (!pipe || pipe.destroyed || pipe.connecting) return;

  // A stalled local consumer should reconnect, not accumulate an unbounded queue.
  if (pipe.writableLength > 8192) {
    pipe.destroy();
    return;
  }

  pipe.write(line);
}

function publish() {
  for (const pipe of pipes.values()) send(pipe, latest);
}

function clear() {
  latest = 'CLEAR\n';
  lastSnapshotAt = 0;
  snapshot = undefined;

  publish();
}

function connectPipe(instance) {
  if (stopping) return;

  const pipe = net.createConnection(endpoint(instance));
  pipes.set(instance, pipe);

  pipe.on('connect', () => {
    console.log(`Connected to TF2 instance ${instance}`);
    send(pipe, latest);
  });

  // TF2 may not be running yet; the close handler schedules another attempt.
  pipe.on('error', error => {
    if (!['ENOENT', 'ECONNREFUSED', 'EBUSY'].includes(error.code)) {
      console.warn(`TF2 instance ${instance}: ${error.message}`);
    }
  });

  pipe.on('close', () => {
    pipes.delete(instance);

    if (!stopping) {
      pipeReconnects.set(instance, setTimeout(() => connectPipe(instance), 1000));
    }
  });
}

function connectWebSocket() {
  if (stopping) return;

  const connection = new WebSocket(url);
  socket = connection;

  const handshake = setTimeout(() => connection.close(), 10000);
  let opened = 0;

  connection.addEventListener('open', () => {
    clearTimeout(handshake);
    opened = Date.now();
    console.log(`Connected to progress feed for ${RELAY_SERVER_CODE}`);
  });

  connection.addEventListener('message', event => {
    try {
      if (typeof event.data !== 'string' || event.data.length > 65536) {
        throw new Error('Invalid message size');
      }

      const value = JSON.parse(event.data);

      if (value.type === 'spectator_select') {
        selectPlayers(value);
        return;
      }

      if (value.type === 'relay_progress_reset') {
        clear();
        return;
      }

      const line = encodeSnapshot(value, RELAY_SERVER_CODE);
      if (line === null) return;

      if (!receivedSnapshot) {
        console.log(`Received progress snapshot: map=${value.map}, players=${value.userids.length}`);
        receivedSnapshot = true;
      }

      latest = line;
      snapshot = value;
      lastSnapshotAt = Date.now();

      publish();
    } catch {
      console.warn('Rejected invalid progress message');
      clear();
    }
  });

  connection.addEventListener('error', () => console.warn('Progress WebSocket error'));

  connection.addEventListener('close', event => {
    clearTimeout(handshake);
    clear();

    if (stopping) return;

    if (opened && Date.now() - opened >= 30000) retries = 0;

    const delay = Math.min(30000, 1000 * 2 ** Math.min(retries++, 5));
    console.log(`Progress connection closed (${event.code}); retrying in ${delay}ms`);
    websocketReconnect = setTimeout(connectWebSocket, delay);
  });
}

function selectPlayers(value) {
  try {
    const current = lastSnapshotAt && Date.now() - lastSnapshotAt < 15000 ? snapshot : undefined;

    for (const selection of selections(value, current, RELAY_SERVER_CODE)) {
      const pipe = pipes.get(selection.instance);

      if (selection.error || !pipe || pipe.destroyed || pipe.connecting) {
        console.warn(`${selection.instance}: ${selection.error ?? 'Game not connected'}`);
        continue;
      }

      send(pipe, selection.line);
    }
  } catch (error) {
    console.warn(`Selection rejected: ${error.message}`);
  }
}

const watchdog = setInterval(() => {
  if (lastSnapshotAt && Date.now() - lastSnapshotAt >= 15000) {
    clear();
    socket?.close(1000, 'Progress stale');
  }
}, 1000);

function stop() {
  if (stopping) return;

  stopping = true;
  clearInterval(watchdog);
  for (const timer of pipeReconnects.values()) clearTimeout(timer);
  clearTimeout(websocketReconnect);

  clear();

  for (const pipe of pipes.values()) pipe.end();
  socket?.close();

  setTimeout(() => process.exit(0), 500).unref();
}

process.once('SIGINT', stop);
process.once('SIGTERM', stop);

for (const instance of instances) connectPipe(instance);
connectWebSocket();
