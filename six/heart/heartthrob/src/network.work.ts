export interface FloatSample {
  address: string;
  value: number; // IEEE-754 32-bit float via Math.fround
  rawValueStr: string;
}

export interface PointPair {
  id: string; // e.g. "0x1000-0x1004"
  addrA: string;
  addrB: string;
  x: number;
  y: number;
  valA: number;
  valB: number;
}

export type WorkerIncomingMessage =
  | {
      type: "start";
      host: string;
      port: number;
      width?: number;
      height?: number;
    }
  | { type: "stop" }
  | { type: "ingest_chunk"; chunk: string }
  | { type: "viewport"; width: number; height: number };

export type WorkerOutgoingMessage =
  | {
      type: "status";
      state: "listening" | "stopped" | "error";
      detail?: string;
    }
  | { type: "samples"; items: FloatSample[] }
  | { type: "points"; items: PointPair[] };

let lineBuffer = "";
let viewportWidth = 1200;
let viewportHeight = 800;

const MAX_SAMPLES = 50;
const MIN_FLOAT = 0;
const MAX_FLOAT = 1000;
const sampleByAddr = new Map<bigint, FloatSample>();

const RECORD_REGEX =
  /<\s*(0x[0-9a-fA-F]+|[0-9a-fA-F]+)\s*,\s*([+-]?(?:\d+\.?\d*|\.\d+)(?:[eE][+-]?\d+)?)\s*>/g;

function parseAddrBigInt(addr: string): bigint | null {
  try {
    const hex = addr.toLowerCase().startsWith("0x")
      ? addr.toLowerCase()
      : `0x${addr.toLowerCase()}`;
    return BigInt(hex);
  } catch {
    return null;
  }
}

function parseChunk(chunk: string): FloatSample[] {
  lineBuffer += chunk;
  const parts = lineBuffer.split(/\r?\n/);
  lineBuffer = parts.pop() ?? "";

  const samples: FloatSample[] = [];
  for (const line of parts) {
    const trimmed = line.trim();
    if (!trimmed) continue;

    RECORD_REGEX.lastIndex = 0;
    let match: RegExpExecArray | null;
    while ((match = RECORD_REGEX.exec(trimmed)) !== null) {
      const addrRaw = match[1];
      const valRaw = match[2];
      const parsedFloat = Number.parseFloat(valRaw);
      if (Number.isFinite(parsedFloat)) {
        const normalizedAddr = addrRaw.toLowerCase().startsWith("0x")
          ? addrRaw.toLowerCase()
          : `0x${addrRaw.toLowerCase()}`;

        samples.push({
          address: normalizedAddr,
          value: Math.fround(parsedFloat),
          rawValueStr: valRaw,
        });
      }
    }
  }
  return samples;
}

function buildClosestAddressPairs(
  map: Map<bigint, FloatSample>,
  width = viewportWidth,
  height = viewportHeight,
): PointPair[] {
  if (map.size < 2) return [];

  const padX = Math.max(60, width * 0.1);
  const padTop = Math.max(80, height * 0.14);
  const padBottom = Math.max(60, height * 0.1);

  const usableW = Math.max(100, width - padX * 2);
  const usableH = Math.max(100, height - padTop - padBottom);

  const cappedEntries = Array.from(map.entries()).slice(-MAX_SAMPLES);
  const workingMap = new Map<bigint, FloatSample>(cappedEntries);

  const sortedAddrs = Array.from(workingMap.keys()).sort((a, b) =>
    a < b ? -1 : a > b ? 1 : 0,
  );

  const candidates: Array<{ dist: bigint; a: bigint; b: bigint }> = [];
  for (let i = 0; i < sortedAddrs.length - 1; i++) {
    const a = sortedAddrs[i];
    const b = sortedAddrs[i + 1];
    candidates.push({ dist: b - a, a, b });
  }

  candidates.sort((p, q) => (p.dist < q.dist ? -1 : p.dist > q.dist ? 1 : 0));

  const used = new Set<bigint>();
  const pairs: PointPair[] = [];

  for (let idx = 0; idx < candidates.length; idx++) {
    const cand = candidates[idx];
    if (used.has(cand.a) || used.has(cand.b)) continue;
    const sA = workingMap.get(cand.a);
    const sB = workingMap.get(cand.b);
    if (!sA || !sB) continue;

    used.add(cand.a);
    used.add(cand.b);

    const valA = Number.isFinite(sA.value) ? sA.value : 0;
    const valB = Number.isFinite(sB.value) ? sB.value : 0;

    const normA = Math.max(
      0,
      Math.min(1, (valA - MIN_FLOAT) / (MAX_FLOAT - MIN_FLOAT)),
    );
    const rawNormB = Math.max(
      0,
      Math.min(1, (valB - MIN_FLOAT) / (MAX_FLOAT - MIN_FLOAT)),
    );
    const normB = idx % 2 === 0 ? rawNormB : 1 - rawNormB;

    pairs.push({
      id: `${sA.address}-${sB.address}`,
      addrA: sA.address,
      addrB: sB.address,
      x: padX + normA * usableW,
      y: padTop + normB * usableH,
      valA,
      valB,
    });
  }

  return pairs;
}

self.onmessage = (event: MessageEvent<WorkerIncomingMessage>) => {
  const msg = event.data;

  if (msg.type === "start") {
    lineBuffer = "";
    sampleByAddr.clear();
    if (typeof msg.width === "number" && msg.width > 0) {
      viewportWidth = msg.width;
    }
    if (typeof msg.height === "number" && msg.height > 0) {
      viewportHeight = msg.height;
    }
    self.postMessage({
      type: "status",
      state: "listening",
      detail: `Worker ready on ${msg.host}:${msg.port}`,
    } satisfies WorkerOutgoingMessage);
  } else if (msg.type === "ingest_chunk") {
    const items = parseChunk(msg.chunk);
    if (items.length > 0) {
      for (const s of items) {
        const addrNum = parseAddrBigInt(s.address);
        if (addrNum !== null) {
          if (sampleByAddr.has(addrNum)) {
            sampleByAddr.delete(addrNum);
          }
          sampleByAddr.set(addrNum, s);

          while (sampleByAddr.size > MAX_SAMPLES) {
            const oldestKey = sampleByAddr.keys().next().value;
            if (oldestKey === undefined) break;
            sampleByAddr.delete(oldestKey);
          }
        }
      }

      self.postMessage({
        type: "samples",
        items,
      } satisfies WorkerOutgoingMessage);

      const points = buildClosestAddressPairs(
        sampleByAddr,
        viewportWidth,
        viewportHeight,
      );
      self.postMessage({
        type: "points",
        items: points,
      } satisfies WorkerOutgoingMessage);
    }
  } else if (msg.type === "stop") {
    lineBuffer = "";
    sampleByAddr.clear();
    self.postMessage({
      type: "status",
      state: "stopped",
    } satisfies WorkerOutgoingMessage);
  } else if (msg.type === "viewport") {
    if (msg.width > 0) viewportWidth = msg.width;
    if (msg.height > 0) viewportHeight = msg.height;
    if (sampleByAddr.size >= 2) {
      self.postMessage({
        type: "points",
        items: buildClosestAddressPairs(
          sampleByAddr,
          viewportWidth,
          viewportHeight,
        ),
      } satisfies WorkerOutgoingMessage);
    }
  }
};