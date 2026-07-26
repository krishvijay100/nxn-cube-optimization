// architecture:
//   - `cubies` is source of truth (from cubeState.ts)
//   - during a face turn, mount a temporary <group> around just the affected
//     layer, drive its rotation from 0 -> ±pi/2 over MOVE_DURATION_MS, then
//     "commit" the rotation by calling rotateFace() on our cubies state
//   - useFrame runs every frame; we track animation state in a ref (not react
//     state) to avoid re-rendering 60 times per second

import { useFrame } from "@react-three/fiber";
import { OrbitControls } from "@react-three/drei";
import { useEffect, useMemo, useRef } from "react";
import { Group, Mesh, MeshStandardMaterial } from "three";

import type { Cubie, ParsedMove } from "./cubeState";
import {
    FACE_AXIS,
    applyMoves,
    cubieIsInMove,
    homeStickerColors,
    parseMove,
    rotateMove,
} from "./cubeState";

const MAX_ANIMATED_MOVES = 80;

interface Props {
    n: number;
    cubies: Cubie[];
    setCubies: (next: Cubie[]) => void;
    // queue of moves to play back sequentially; consume from the front
    moveQueue: string[];
    setMoveQueue: (next: string[]) => void;
}

export function Cube({ n, cubies, setCubies, moveQueue, setMoveQueue }: Props) {
    const anim = useRef<{
        move: ParsedMove;
        startedAt: number;
        durationMs: number;
        layerHomes: Set<number>;
    } | null>(null);
    const layerGroup = useRef<Group>(null);

    // start the next move whenever idle & queue has one
    useEffect(() => {
        if (anim.current !== null) return;
        if (moveQueue.length === 0) return;
        if (moveQueue.length > MAX_ANIMATED_MOVES) {
            const fastForwardCount = moveQueue.length - MAX_ANIMATED_MOVES;
            setCubies(applyMoves(
                cubies,
                moveQueue.slice(0, fastForwardCount),
                n,
            ));
            setMoveQueue(moveQueue.slice(fastForwardCount));
            return;
        }
        const token = moveQueue[0];
        const move = parseMove(token);
        const layerHomes = new Set(
            cubies
                .filter(cubie => cubieIsInMove(cubie, move, n))
                .map(cubie => cubie.home)
        );
        anim.current = {
            move,
            startedAt: performance.now(),
            durationMs: Math.max(20, Math.min(140, 1600 / moveQueue.length)),
            layerHomes,
        };
    }, [moveQueue, cubies, n]);

    useFrame(() => {
        if (!anim.current || !layerGroup.current) return;
        const { move, startedAt, durationMs } = anim.current;
        const [axis, sign] = FACE_AXIS[move.face];
        const totalAngle = move.quarterTurns * -sign * Math.PI / 2;
        const t = Math.min(1, (performance.now() - startedAt) / durationMs);

        // ease-in-out cubic so animation doesn't look mechanical
        const eased = t < 0.5 ? 4 * t * t * t : 1 - Math.pow(-2 * t + 2, 3) / 2;
        const currentAngle = totalAngle * eased;

        layerGroup.current.rotation.set(0, 0, 0);
        if (axis === "x") layerGroup.current.rotation.x = currentAngle;
        else if (axis === "y") layerGroup.current.rotation.y = currentAngle;
        else layerGroup.current.rotation.z = currentAngle;

        if (t >= 1) {
            // commit: replace cubies with the post-rotation state and drop the
            // temp group's rotation back to zero for next move
            const next = rotateMove(cubies, move, n);
            anim.current = null;
            layerGroup.current.rotation.set(0, 0, 0);
            setCubies(next);
            setMoveQueue(moveQueue.slice(1));
        }
    });

    // split cubies into "in the rotating layer" (rendered under layerGroup) vs
    // "static" (rendered directly under root)
    const layerHomes = anim.current?.layerHomes;
    const layerCubies: Cubie[] = [];
    const staticCubies: Cubie[] = [];
    for (const c of cubies) {
        if (layerHomes && layerHomes.has(c.home)) layerCubies.push(c);
        else staticCubies.push(c);
    }

    return (
        <>
            <OrbitControls enablePan={false} />
            <ambientLight intensity={0.7} />
            <directionalLight position={[5, 8, 5]} intensity={0.6} />
            <group scale={3 / n}>
                {staticCubies.map(c => <CubieMesh key={c.home} cubie={c} n={n} />)}
                <group ref={layerGroup}>
                    {layerCubies.map(c => <CubieMesh key={c.home} cubie={c} n={n} />)}
                </group>
            </group>
        </>
    );
}

function CubieMesh({ cubie, n }: { cubie: Cubie; n: number }) {
    const meshRef = useRef<Mesh>(null);
    const colors = useMemo(() => homeStickerColors(cubie, n), [cubie, n]);
    const materials = useMemo(
        () => colors.map(c => new MeshStandardMaterial({ color: c })),
        [colors],
    );

    // sync mesh transform from the cubie's current position/orientation on every render
    useEffect(() => {
        if (!meshRef.current) return;
        meshRef.current.position.copy(cubie.position);
        meshRef.current.quaternion.copy(cubie.orientation);
    });

    return (
        <mesh ref={meshRef} material={materials}>
            <boxGeometry args={[0.95, 0.95, 0.95]} />
        </mesh>
    );
}
