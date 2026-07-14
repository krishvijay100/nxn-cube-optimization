// architecture:
//   - `cubies` is source of truth (from cubeState.ts) — 27 cubies at
//     integer grid positions with quaternion orientations
//   - during a face turn, mount a temporary <group> around just the affected
//     layer, drive its rotation from 0 -> ±pi/2 over MOVE_DURATION_MS, then
//     "commit" the rotation by calling rotateFace() on our cubies state
//   - useFrame runs every frame; we track animation state in a ref (not react
//     state) to avoid re-rendering 60 times per second

import { useFrame } from "@react-three/fiber";
import { OrbitControls } from "@react-three/drei";
import { useEffect, useMemo, useRef } from "react";
import { Group, Mesh, MeshStandardMaterial } from "three";

import type { Cubie, Face } from "./cubeState";
import {
    FACE_AXIS,
    homeStickerColors,
    parseMove,
    rotateFace,
} from "./cubeState";

const MOVE_DURATION_MS = 220;

interface Props {
    cubies: Cubie[];
    setCubies: (next: Cubie[]) => void;
    // queue of moves to play back sequentially; consume from the front
    moveQueue: string[];
    setMoveQueue: (next: string[]) => void;
}

export function Cube({ cubies, setCubies, moveQueue, setMoveQueue }: Props) {
    const anim = useRef<{
        face: Face;
        quarterTurns: number;
        startedAt: number;
        layerHomes: Set<number>;
    } | null>(null);
    const layerGroup = useRef<Group>(null);

    // start the next move whenever idle & queue has one
    useEffect(() => {
        if (anim.current !== null) return;
        if (moveQueue.length === 0) return;
        const token = moveQueue[0];
        const { face, quarterTurns } = parseMove(token);
        const [axis, sign] = FACE_AXIS[face];
        const layerHomes = new Set(
            cubies
                .filter(c => Math.round(
                    axis === "x" ? c.position.x : axis === "y" ? c.position.y : c.position.z
                ) === sign)
                .map(c => c.home)
        );
        anim.current = {
            face,
            quarterTurns,
            startedAt: performance.now(),
            layerHomes,
        };
    }, [moveQueue, cubies]);

    useFrame(() => {
        if (!anim.current || !layerGroup.current) return;
        const { face, quarterTurns, startedAt } = anim.current;
        const [axis, sign] = FACE_AXIS[face];
        const totalAngle = quarterTurns * -sign * Math.PI / 2;
        const t = Math.min(1, (performance.now() - startedAt) / MOVE_DURATION_MS);

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
            const next = rotateFace(cubies, face, quarterTurns);
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
            <group>
                {staticCubies.map(c => <CubieMesh key={c.home} cubie={c} />)}
                <group ref={layerGroup}>
                    {layerCubies.map(c => <CubieMesh key={c.home} cubie={c} />)}
                </group>
            </group>
        </>
    );
}

function CubieMesh({ cubie }: { cubie: Cubie }) {
    const meshRef = useRef<Mesh>(null);
    const colors = useMemo(() => homeStickerColors(cubie.home), [cubie.home]);
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

