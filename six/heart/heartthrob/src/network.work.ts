export interface FloatSample {
  address: string;
  value: number; // IEEE-754 32-bit float via Math.fround
  rawValueStr: string;
}

export interface PointPair {
  id: string;        // e.g. "0x1000-0x1004"
  addrA: string;
  addrB: string;
  x: number;
  y: number;
  valA: number;
  valB: number;
}

export type WorkerIncomingMessage =
  | { type: "start"; host: string; port: number }
  | { type: "stop" }
  | { type: "ingest_chunk"; chunk: string };

export type WorkerOutgoingMessage =
  | {
      type: "status";
      state: "listening" | "stopped" | "error";
      detail?: string;
    }
  | { type: "samples"; items: FloatSample[] }
  | { type: "points"; items: PointPair[] };

let lineBuffer = "";
const sampleByAddr = new Map<bigint, FloatSample>();

const RECORD_REGEX =
  /<\s*(0x[0-9a-fA-F]+|[0-9a-fA-F]+)\s*,\s*([+-]?(?:\d+\.?\d*|\.\d+)(?:[eE][+-]?\d+)?)\s*>/g;

function parseAddrBigInt(addr: string): bigint | null {
  try {
    const hex = addr.toLowerCase().startsWith("0x") ? addr.toLowerCase() : `0x${addr.toLowerCase()}`;
    return BigInt(hex);
  } catch {
    return null;
  }
} //\ parseAddrBigInt

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
} //\ parseChunk

// Groups samples by pairing closest numeric addresses into (x, y) coordinates.
// Adjust scaleX/scaleY/offsetX/offsetY to match your coordinate space.
function buildClosestAddressPairs(
  map: Map<bigint, FloatSample>,
  scaleX = 100,
  scaleY = 100,
  offsetX = 200,
  offsetY = 200,
): PointPair[] {
  if (map.size < 2) return [];

  const sortedAddrs = Array.from(map.keys()).sort((a, b) =>
    a < b ? -1 : a > b ? 1 : 0,
  );

  // Build candidate adjacent pairs sorted by address distance |addrB - addrA|
  const candidates: Array<{ dist: bigint; a: bigint; b: bigint }> = [];
  for (let i = 0; i < sortedAddrs.length - 1; i++) {
    const a = sortedAddrs[i];
    const b = sortedAddrs[i + 1];
    candidates.push({ dist: b - a, a, b });
  }

  candidates.sort((p, q) => (p.dist < q.dist ? -1 : p.dist > q.dist ? 1 : 0));

  const used = new Set<bigint>();
  const pairs: PointPair[] = [];

  for (const cand of candidates) {
    if (used.has(cand.a) || used.has(cand.b)) continue;
    const sA = map.get(cand.a);
    const sB = map.get(cand.b);
    if (!sA || !sB) continue;

    used.add(cand.a);
    used.add(cand.b);

    const valA = Number.isFinite(sA.value) ? sA.value : 0;
    const valB = Number.isFinite(sB.value) ? sB.value : 0;

    pairs.push({
      id: `${sA.address}-${sB.address}`,
      addrA: sA.address,
      addrB: sB.address,
      x: offsetX + valA * scaleX,
      y: offsetY + valB * scaleY,
      valA,
      valB,
    });
  }

  return pairs;
} //\ buildClosestAddressPairs

self.onmessage = (event: MessageEvent<WorkerIncomingMessage>) => {
  const msg = event.data;

  if (msg.type === "start") {
    lineBuffer = "";
    sampleByAddr.clear();
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
          sampleByAddr.set(addrNum, s);
        }
      }

      self.postMessage({
        type: "samples",
        items,
      } satisfies WorkerOutgoingMessage);

      const points = buildClosestAddressPairs(sampleByAddr);
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
  }
}; //\ self.onmessage
