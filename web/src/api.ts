// thin wrapper around the fastapi service, two endpoints:
//   GET  /scramble?length=N   -> { scramble, seed }
//   POST /solve  { scramble } -> { solution, num_moves }

const BASE = import.meta.env.VITE_API_URL ?? "http://127.0.0.1:8000";

export async function fetchScramble(length = 20): Promise<string[]> {
    const r = await fetch(`${BASE}/scramble?length=${length}`);
    if (!r.ok) throw new Error(`scramble ${r.status}: ${await r.text()}`);
    return (await r.json()).scramble as string[];
}

export async function fetchSolve(scramble: string[]): Promise<string[]> {
    const r = await fetch(`${BASE}/solve`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ scramble }),
    });
    if (!r.ok) throw new Error(`solve ${r.status}: ${await r.text()}`);
    return (await r.json()).solution as string[];
}
