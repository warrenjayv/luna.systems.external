import net from "node:net";

const HOST = process.env.MTU_HOST || "127.0.0.1";
const PORT = Number.parseInt(process.env.MTU_PORT || "9001", 10) || 9001;
const COUNT = Number.parseInt(process.env.MTU_COUNT || "50", 10) || 50;
const TICK_MS = Number.parseInt(process.env.MTU_TICK_MS || "100", 10) || 100;

const MIN_VAL = 0;
const MAX_VAL = 1000;
const MAX_SPEED = 6.0;

function randRange(min, max) {
  return min + Math.random() * (max - min);
}

function clampReflect(val, vel, min = MIN_VAL, max = MAX_VAL) {
  let nextVal = val + vel;
  let nextVel = vel;

  if (nextVal < min) {
    nextVal = min + (min - nextVal);
    nextVel = Math.abs(nextVel);
  } else if (nextVal > max) {
    nextVal = max - (nextVal - max);
    nextVel = -Math.abs(nextVel);
  }

  return {
    val: Math.max(min, Math.min(max, nextVal)),
    vel: nextVel,
  };
}

function createTravelers(count = COUNT) {
  const travelers = [];
  for (let i = 0; i < count; i++) {
    const baseAddr = 0x100000 + i * 8;
    travelers.push({
      addrA: `0x${baseAddr.toString(16)}`,
      addrB: `0x${(baseAddr + 4).toString(16)}`,
      a: randRange(MIN_VAL, MAX_VAL),
      b: randRange(MIN_VAL, MAX_VAL),
      va: randRange(-MAX_SPEED, MAX_SPEED),
      vb: randRange(-MAX_SPEED, MAX_SPEED),
    });
  }
  return travelers;
}

function stepTravelers(travelers) {
  for (const t of travelers) {
    if (Math.random() < 0.15) {
      t.va += randRange(-0.8, 0.8);
      t.vb += randRange(-0.8, 0.8);
    }

    t.va = Math.max(-MAX_SPEED, Math.min(MAX_SPEED, t.va));
    t.vb = Math.max(-MAX_SPEED, Math.min(MAX_SPEED, t.vb));

    const nextA = clampReflect(t.a, t.va);
    const nextB = clampReflect(t.b, t.vb);

    t.a = nextA.val;
    t.va = nextA.vel;
    t.b = nextB.val;
    t.vb = nextB.vel;
  }
}

function formatPacket(travelers) {
  return (
    travelers
      .map((t) => `<${t.addrA},${t.a.toFixed(2)}><${t.addrB},${t.b.toFixed(2)}>`)
      .join("") + "\n"
  );
}

const travelers = createTravelers(COUNT);
let timer = null;

const client = net.createConnection({ host: HOST, port: PORT }, () => {
  client.write(formatPacket(travelers));
  timer = setInterval(() => {
    stepTravelers(travelers);
    if (!client.destroyed) {
      client.write(formatPacket(travelers));
    }
  }, TICK_MS);
});

client.on("error", (err) => {
  if (timer) {
    clearInterval(timer);
    timer = null;
  }
  console.error(`[mtu] connection error (${HOST}:${PORT}):`, err.message);
});

client.on("close", () => {
  if (timer) {
    clearInterval(timer);
    timer = null;
  }
});

process.on("SIGINT", () => {
  if (timer) clearInterval(timer);
  client.destroy();
  process.exit(0);
});

process.on("SIGTERM", () => {
  if (timer) clearInterval(timer);
  client.destroy();
  process.exit(0);
});