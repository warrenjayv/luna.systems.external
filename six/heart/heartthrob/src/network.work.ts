export interface FloatSample {
  address: string;
  value: number; // IEEE-754 32-bit float via Math.fround
  rawValueStr: string;
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
  | { type: "samples"; items: FloatSample[] };

let lineBuffer = "";
const RECORD_REGEX =
  /<\s*(0x[0-9a-fA-F]+|[0-9a-fA-F]+)\s*,\s*([+-]?(?:\d+\.?\d*|\.\d+)(?:[eE][+-]?\d+)?)\s*>/g;

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

self.onmessage = (event: MessageEvent<WorkerIncomingMessage>) => {
  const msg = event.data;

  if (msg.type === "start") {
    lineBuffer = "";
    // If bridging from Electron IPC or a WebSocket relay, push incoming text chunks via "ingest_chunk"
    self.postMessage({
      type: "status",
      state: "listening",
      detail: `Worker ready on ${msg.host}:${msg.port}`,
    } satisfies WorkerOutgoingMessage);
  } else if (msg.type === "ingest_chunk") {
    const items = parseChunk(msg.chunk);
    if (items.length > 0) {
      self.postMessage({
        type: "samples",
        items,
      } satisfies WorkerOutgoingMessage);
    }
  } else if (msg.type === "stop") {
    lineBuffer = "";
    self.postMessage({
      type: "status",
      state: "stopped",
    } satisfies WorkerOutgoingMessage);
  }
};
