import type { FloatSample, PointPair, WorkerOutgoingMessage } from "./network.work";

export type SampleListener = (samples: FloatSample[]) => void;
export type PointPairListener = (points: PointPair[]) => void;
export type StatusListener = (
  state: "listening" | "stopped" | "error",
  detail?: string,
) => void;

interface IpcRendererLike {
  on(channel: string, listener: (_event: unknown, chunk: string) => void): void;
  removeListener(
    channel: string,
    listener: (_event: unknown, chunk: string) => void,
  ): void;
  invoke(
    channel: string,
    ...args: unknown[]
  ): Promise<{ ok: boolean; error?: string } | void>;
}

function getIpcRenderer(): IpcRendererLike | null {
  try {
    const req = (
      window as unknown as {
        require?: (id: string) => { ipcRenderer?: IpcRendererLike };
      }
    ).require;
    return req ? (req("electron").ipcRenderer ?? null) : null;
  } catch {
    return null;
  }
}

class NetworkManager {
  private worker: Worker | null = null;
  private sampleListeners = new Set<SampleListener>();
  private pointListeners = new Set<PointPairListener>();
  private statusListeners = new Set<StatusListener>();
  private latestByAddress = new Map<string, FloatSample>();
  private latestPoints: PointPair[] = [];
  private ipcListener: ((_event: unknown, chunk: string) => void) | null = null;

  public start(host = "127.0.0.1", port = 9000): void {
    this.stop();

    this.worker = new Worker(new URL("./network.work.ts", import.meta.url), {
      type: "module",
    });

    this.worker.onmessage = (event: MessageEvent<WorkerOutgoingMessage>) => {
      const msg = event.data;
      if (msg.type === "samples") {
        for (const item of msg.items) {
          this.latestByAddress.set(item.address, item);
        }
        for (const cb of this.sampleListeners) {
          cb(msg.items);
        }
      } else if (msg.type === "status") {
        for (const cb of this.statusListeners) {
          cb(msg.state, msg.detail);
        }
      } else if (msg.type === "points") {
        this.latestPoints = msg.items;
        for (const cb of this.pointListeners) {
          cb(msg.items);
       }
      }   
      
    }; //\ this.worker.onmessage

    this.worker.onerror = (err) => {
      for (const cb of this.statusListeners) {
        cb("error", err.message || "Worker error");
      }
    };

    this.worker.postMessage({ type: "start", host, port });

    const ipc = getIpcRenderer();
    if (ipc) {
      this.ipcListener = (_event: unknown, chunk: string) => {
        this.worker?.postMessage({ type: "ingest_chunk", chunk });
      };
      ipc.on("net:tcp-data", this.ipcListener);

      void ipc.invoke("net:start-tcp", host, port).then((res) => {
        if (!res || !res.ok) {
          for (const cb of this.statusListeners) {
            cb(
              "error",
              res?.error || "Failed to start TCP server in payload.mjs",
            );
          }
        }
      });
    }
  }

  public getLatestMap(): Map<string, FloatSample> {
    return this.latestByAddress;
  }

  public onSamples(cb: SampleListener): () => void {
    this.sampleListeners.add(cb);
    return () => this.sampleListeners.delete(cb);
  }

  public onStatus(cb: StatusListener): () => void {
    this.statusListeners.add(cb);
    return () => this.statusListeners.delete(cb);
  }

  public stop(): void {
    const ipc = getIpcRenderer();
    if (ipc && this.ipcListener) {
      ipc.removeListener("net:tcp-data", this.ipcListener);
      this.ipcListener = null;
    }
    if (ipc) {
      void ipc.invoke("net:stop-tcp");
    }
    if (this.worker) {
      this.worker.postMessage({ type: "stop" });
      this.worker.terminate();
      this.worker = null;
    }
  }

  public getLatestPoints(): PointPair[] {
      return this.latestPoints;
  }

  public onPoints(cb: PointPairListener): () => void {
    this.pointListeners.add(cb);
    return () => this.pointListeners.delete(cb);
  }
} //\ NetworkManager

export const _net = new NetworkManager();
export type { FloatSample };
