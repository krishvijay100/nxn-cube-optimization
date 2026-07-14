import http from "k6/http";
import { check } from "k6";
import { Trend } from "k6/metrics";

import { SCRAMBLES } from "./scrambles.js";

const BASE_URL = __ENV.BASE_URL || "http://127.0.0.1:8000";
const VUS = parseInt(__ENV.VUS || "20", 10);
const DURATION = __ENV.DURATION || "30s";

const solveLatency = new Trend("solve_latency_ms", true);

export const options = {
    scenarios: {
        steady: {
            executor: "constant-vus",
            vus: VUS,
            duration: DURATION,
        },
    },
    thresholds: {
        "http_req_failed": ["rate<0.001"],
        "solve_latency_ms": ["p(95)<200", "p(99)<500"],
    },
};

export default function () {
    // rotate deterministically through the pool so every VU issues a mix of
    // scrambles, index by (VU id * iteration) to spread work across the pool
    const idx = (__VU * 31 + __ITER) % SCRAMBLES.length;
    const scramble = SCRAMBLES[idx];

    const res = http.post(
        `${BASE_URL}/solve`,
        JSON.stringify({ scramble }),
        { headers: { "Content-Type": "application/json" } },
    );

    solveLatency.add(res.timings.duration);
    check(res, {
        "200": (r) => r.status === 200,
        "has solution": (r) => r.json("num_moves") !== undefined,
    });
}
