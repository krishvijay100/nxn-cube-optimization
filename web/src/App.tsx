import { Canvas } from "@react-three/fiber";
import { useState, useCallback } from "react";

import { Cube } from "./Cube";
import { fetchScramble, fetchSolve } from "./api";
import { solvedCube, type Cubie } from "./cubeState";

import "./styles.css";

type Phase = "idle" | "scrambling" | "scrambled" | "solving" | "playing";

export default function App() {
    const [cubies, setCubies] = useState<Cubie[]>(() => solvedCube());
    const [moveQueue, setMoveQueue] = useState<string[]>([]);
    const [phase, setPhase] = useState<Phase>("idle");
    const [scramble, setScramble] = useState<string[]>([]);
    const [solution, setSolution] = useState<string[]>([]);
    const [error, setError] = useState<string | null>(null);

    const onScramble = useCallback(async () => {
        setError(null);
        try {
            const tokens = await fetchScramble(20);
            setScramble(tokens);
            setSolution([]);
            setPhase("scrambling");
            setMoveQueue(tokens);
        } catch (e) {
            setError((e as Error).message);
            setPhase("idle");
        }
    }, []);

    const onSolve = useCallback(async () => {
        setError(null);
        setPhase("solving");
        try {
            const sol = await fetchSolve(scramble);
            setSolution(sol);
            setPhase("playing");
            setMoveQueue(sol);
        } catch (e) {
            setError((e as Error).message);
            setPhase("scrambled");
        }
    }, [scramble]);

    // watch for queue drain -> advance phase. cube.tsx calls this every time
    // it commits a move; the drain-to-empty is our animation-complete signal
    const onQueueChange = useCallback((next: string[]) => {
        setMoveQueue(next);
        if (next.length === 0) {
            setPhase(prev => {
                if (prev === "scrambling") return "scrambled";
                if (prev === "playing") return "idle";
                return prev;
            });
        }
    }, []);

    const busy = phase === "scrambling" || phase === "solving" || phase === "playing";

    return (
        <div className="app">
            <header>
                <h1>NxN Cube Solver</h1>
                <p className="sub">A high-performance C++ engine implementing the Kociemba two-phase algorithm.</p>
            </header>

            <div className="canvas-wrap">
                <Canvas camera={{ position: [4.5, 4, 5], fov: 45 }}>
                    <Cube
                        cubies={cubies}
                        setCubies={setCubies}
                        moveQueue={moveQueue}
                        setMoveQueue={onQueueChange}
                    />
                </Canvas>
            </div>

            <div className="controls">
                <button onClick={onScramble} disabled={busy}>Scramble</button>
                <button onClick={onSolve} disabled={busy || phase !== "scrambled"}>Solve</button>
                <span className="status">
                    {phase === "idle" && "click Scramble to begin"}
                    {phase === "scrambling" && `scrambling (${moveQueue.length} left)`}
                    {phase === "scrambled" && `scrambled with ${scramble.length} moves — click Solve`}
                    {phase === "solving" && "solving..."}
                    {phase === "playing" && `playing solution (${moveQueue.length} of ${solution.length} left)`}
                </span>
                {error && <span className="error">{error}</span>}
            </div>

            {(scramble.length > 0 || solution.length > 0) && (
                <div className="moves">
                    {scramble.length > 0 && (
                        <div><strong>Scramble:</strong> {scramble.join(" ")}</div>
                    )}
                    {solution.length > 0 && (
                        <div><strong>Solution ({solution.length}):</strong> {solution.join(" ")}</div>
                    )}
                </div>
            )}
        </div>
    );
}
