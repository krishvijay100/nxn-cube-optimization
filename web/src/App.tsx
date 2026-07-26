import { Canvas } from "@react-three/fiber";
import { useState, useCallback, useMemo } from "react";

import { Cube } from "./Cube";
import { fetchScramble, fetchSolve } from "./api";
import {
    formatSequenceVirtualCubeNet,
    parseUserScramble,
    solvedCube,
    type Cubie,
} from "./cubeState";

import "./styles.css";

type Phase = "idle" | "scrambling" | "scrambled" | "solving" | "playing";

export default function App() {
    const [n, setN] = useState(3);
    const [cubies, setCubies] = useState<Cubie[]>(() => solvedCube(3));
    const [moveQueue, setMoveQueue] = useState<string[]>([]);
    const [phase, setPhase] = useState<Phase>("idle");
    const [scramble, setScramble] = useState<string[]>([]);
    const [solution, setSolution] = useState<string[]>([]);
    const [scrambleInput, setScrambleInput] = useState("");
    const [error, setError] = useState<string | null>(null);

    const onScramble = useCallback(async () => {
        setError(null);
        try {
            const tokens = await fetchScramble(n, 20);
            setCubies(solvedCube(n));
            setScramble(tokens);
            setSolution([]);
            setPhase("scrambling");
            setMoveQueue(tokens);
        } catch (e) {
            setError((e as Error).message);
            setPhase("idle");
        }
    }, [n]);

    const onSolve = useCallback(async () => {
        setError(null);
        setPhase("solving");
        try {
            const sol = await fetchSolve(n, scramble);
            setSolution(sol);
            setPhase("playing");
            setMoveQueue(sol);
        } catch (e) {
            setError((e as Error).message);
            setPhase("scrambled");
        }
    }, [n, scramble]);

    const onUseScramble = useCallback(() => {
        setError(null);
        try {
            const tokens = parseUserScramble(scrambleInput, n);
            setCubies(solvedCube(n));
            setScramble(tokens);
            setSolution([]);
            setPhase("scrambling");
            setMoveQueue(tokens);
        } catch (e) {
            setError((e as Error).message);
        }
    }, [n, scrambleInput]);

    const onSizeChange = useCallback((nextN: number) => {
        setN(nextN);
        setCubies(solvedCube(nextN));
        setMoveQueue([]);
        setScramble([]);
        setSolution([]);
        setScrambleInput("");
        setError(null);
        setPhase("idle");
    }, []);

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
    const displayedScramble = useMemo(
        () => formatSequenceVirtualCubeNet(scramble, n),
        [scramble, n],
    );
    const displayedSolution = useMemo(
        () => formatSequenceVirtualCubeNet(solution, n),
        [solution, n],
    );

    return (
        <div className="app">
            <header>
                <h1>NxN Cube Solver</h1>
                <p className="sub">A high-performance C++ engine implementing the Kociemba two-phase algorithm.</p>
            </header>

            <div className="canvas-wrap">
                <Canvas camera={{ position: [4.5, 4, 5], fov: 45 }}>
                    <Cube
                        key={n}
                        n={n}
                        cubies={cubies}
                        setCubies={setCubies}
                        moveQueue={moveQueue}
                        setMoveQueue={onQueueChange}
                    />
                </Canvas>
            </div>

            <div className="controls">
                <label className="size-select">
                    Cube
                    <select
                        value={n}
                        onChange={event => onSizeChange(Number(event.target.value))}
                        disabled={busy}
                    >
                        {[3, 4, 5, 6, 7].map(size => (
                            <option key={size} value={size}>
                                {size}×{size}
                            </option>
                        ))}
                    </select>
                </label>
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

            <form
                className="scramble-input"
                onSubmit={event => {
                    event.preventDefault();
                    onUseScramble();
                }}
            >
                <input
                    type="text"
                    value={scrambleInput}
                    onChange={event => setScrambleInput(event.target.value)}
                    placeholder="Enter scramble, e.g. R U R' U' Rw2 r"
                    aria-label="Scramble notation"
                    disabled={busy}
                    spellCheck={false}
                />
                <button type="submit" disabled={busy || scrambleInput.trim() === ""}>
                    Use Scramble
                </button>
                <span className="input-help">
                    Supports WCA-style outer, wide, and noncentral inner-slice moves.
                </span>
            </form>

            {(scramble.length > 0 || solution.length > 0) && (
                <div className="moves">
                    {scramble.length > 0 && (
                        <div><strong>Scramble:</strong> {displayedScramble.join(" ")}</div>
                    )}
                    {solution.length > 0 && (
                        <div><strong>Solution ({solution.length}):</strong> {displayedSolution.join(" ")}</div>
                    )}
                </div>
            )}
        </div>
    );
}
