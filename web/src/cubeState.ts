// 27 cubies live at grid positions (x, y, z) each in {-1, 0, +1}; key by
// slot index = (x+1) + 3*(y+1) + 9*(z+1), slot 13 is the hidden center
//
// each cubie carries:
//   - `home`: its slot on the solved cube (never changes; determines sticker colors)
//   - `slot`: its current position (changes as moves are applied)
//   - `orientation`: quaternion tracking accumulated rotation (composes with each move)

import { Quaternion, Vector3 } from "three";

export const FACE_COLORS = {
    U: "#ffffff",
    D: "#ffd500",
    F: "#009e60",
    B: "#0051ba",
    R: "#c41e3a",
    L: "#ff5800",
    HIDDEN: "#111111",
} as const;

export type Face = "U" | "D" | "F" | "B" | "R" | "L";

export interface Cubie {
    home: number;             // 0-26, immutable
    position: Vector3;        // current grid position (integers -1/0/+1)
    orientation: Quaternion;  // accumulated rotation vs solved
}

export function slotIndex(x: number, y: number, z: number): number {
    return (x + 1) + 3 * (y + 1) + 9 * (z + 1);
}

export function positionOfSlot(slot: number): Vector3 {
    const x = (slot % 3) - 1;
    const y = (Math.floor(slot / 3) % 3) - 1;
    const z = Math.floor(slot / 9) - 1;
    return new Vector3(x, y, z);
}

export function solvedCube(): Cubie[] {
    const out: Cubie[] = [];
    for (let slot = 0; slot < 27; slot++) {
        out.push({
            home: slot,
            position: positionOfSlot(slot),
            orientation: new Quaternion(),   // identity
        });
    }
    return out;
}

export function homeStickerColors(home: number): readonly [string, string, string, string, string, string] {
    const p = positionOfSlot(home);
    return [
        p.x > 0 ? FACE_COLORS.R : FACE_COLORS.HIDDEN,
        p.x < 0 ? FACE_COLORS.L : FACE_COLORS.HIDDEN,
        p.y > 0 ? FACE_COLORS.U : FACE_COLORS.HIDDEN,
        p.y < 0 ? FACE_COLORS.D : FACE_COLORS.HIDDEN,
        p.z > 0 ? FACE_COLORS.F : FACE_COLORS.HIDDEN,
        p.z < 0 ? FACE_COLORS.B : FACE_COLORS.HIDDEN,
    ] as const;
}

export function cubiesOnFace(cubies: Cubie[], face: Face): Cubie[] {
    const [axis, sign] = FACE_AXIS[face];
    return cubies.filter(c => {
        const v = axis === "x" ? c.position.x : axis === "y" ? c.position.y : c.position.z;
        return Math.round(v) === sign;
    });
}

export const FACE_AXIS: Record<Face, ["x" | "y" | "z", number]> = {
    R: ["x", +1], L: ["x", -1],
    U: ["y", +1], D: ["y", -1],
    F: ["z", +1], B: ["z", -1],
};

export function rotateFace(cubies: Cubie[], face: Face, quarterTurns: number): Cubie[] {
    const [axis, sign] = FACE_AXIS[face];
    const angle = quarterTurns * -sign * Math.PI / 2;
    const rotAxis = axis === "x" ? new Vector3(1, 0, 0) : axis === "y" ? new Vector3(0, 1, 0) : new Vector3(0, 0, 1);
    const layerRot = new Quaternion().setFromAxisAngle(rotAxis, angle);

    return cubies.map(c => {
        const onLayer = Math.round(
            axis === "x" ? c.position.x : axis === "y" ? c.position.y : c.position.z
        ) === sign;
        if (!onLayer) return c;

        const newPos = c.position.clone().applyQuaternion(layerRot);
        // snap to integer grid to avoid drift from repeated floating-point rotations
        newPos.set(Math.round(newPos.x), Math.round(newPos.y), Math.round(newPos.z));

        // compose left: newOrientation = layerRot * oldOrientation
        const newOri = layerRot.clone().multiply(c.orientation);
        return { home: c.home, position: newPos, orientation: newOri };
    });
}

export function parseMove(token: string): { face: Face; quarterTurns: number } {
    const face = token[0] as Face;
    if (!(face in FACE_AXIS)) throw new Error(`invalid face in move: ${token}`);
    let qt: number;
    if (token.length === 1) qt = 1;
    else if (token[1] === "'") qt = -1;
    else if (token[1] === "2") qt = 2;
    else throw new Error(`invalid modifier in move: ${token}`);
    return { face, quarterTurns: qt };
}

export function applyMoves(cubies: Cubie[], moves: string[]): Cubie[] {
    let state = cubies;
    for (const token of moves) {
        const { face, quarterTurns } = parseMove(token);
        state = rotateFace(state, face, quarterTurns);
    }
    return state;
}