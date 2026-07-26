import { Quaternion, Vector3 } from "three";

export const FACE_COLORS = {
    U: "#ffffff",
    D: "#ffd500",
    F: "#009e60",
    B: "#0051ba",
    R: "#c41e3a",
    L: "#ff9500",
    HIDDEN: "#111111",
} as const;

export type Face = "U" | "D" | "F" | "B" | "R" | "L";

export interface Cubie {
    home: number;
    homePosition: Vector3;
    position: Vector3;
    orientation: Quaternion;
}

export interface ParsedMove {
    face: Face;
    quarterTurns: number;
    outerDepth: number;
    innerDepth: number;
}

export const FACE_AXIS: Record<Face, ["x" | "y" | "z", number]> = {
    R: ["x", +1], L: ["x", -1],
    U: ["y", +1], D: ["y", -1],
    F: ["z", +1], B: ["z", -1],
};

export function cubeExtent(n: number): number {
    return (n - 1) / 2;
}

export function solvedCube(n: number): Cubie[] {
    const extent = cubeExtent(n);
    const out: Cubie[] = [];
    let home = 0;
    for (let xi = 0; xi < n; xi++) {
        for (let yi = 0; yi < n; yi++) {
            for (let zi = 0; zi < n; zi++) {
                const surface =
                    xi === 0 || xi === n - 1 ||
                    yi === 0 || yi === n - 1 ||
                    zi === 0 || zi === n - 1;
                if (!surface) continue;
                const position = new Vector3(
                    xi - extent,
                    yi - extent,
                    zi - extent,
                );
                out.push({
                    home: home++,
                    homePosition: position.clone(),
                    position,
                    orientation: new Quaternion(),
                });
            }
        }
    }
    return out;
}

export function homeStickerColors(
    cubie: Cubie,
    n: number,
): readonly [string, string, string, string, string, string] {
    const p = cubie.homePosition;
    const extent = cubeExtent(n);
    const at = (value: number, target: number) =>
        Math.abs(value - target) < 1e-6;
    return [
        at(p.x, extent) ? FACE_COLORS.R : FACE_COLORS.HIDDEN,
        at(p.x, -extent) ? FACE_COLORS.L : FACE_COLORS.HIDDEN,
        at(p.y, extent) ? FACE_COLORS.U : FACE_COLORS.HIDDEN,
        at(p.y, -extent) ? FACE_COLORS.D : FACE_COLORS.HIDDEN,
        at(p.z, extent) ? FACE_COLORS.F : FACE_COLORS.HIDDEN,
        at(p.z, -extent) ? FACE_COLORS.B : FACE_COLORS.HIDDEN,
    ] as const;
}

export function parseMove(token: string): ParsedMove {
    const match = /^(\d+)?([UDFBRL])(w)?(2|')?$/.exec(token);
    if (!match) throw new Error(`invalid move: ${token}`);

    const [, prefix, faceText, wide, modifier] = match;
    const face = faceText as Face;
    const layers = prefix ? Number(prefix) : wide ? 2 : 1;
    if (layers < 1 || (prefix && layers < 2)) {
        throw new Error(`invalid layer count in move: ${token}`);
    }

    return {
        face,
        quarterTurns: modifier === "'" ? -1 : modifier === "2" ? 2 : 1,
        outerDepth: wide ? 0 : prefix ? layers - 1 : 0,
        innerDepth: wide ? layers - 1 : prefix ? layers - 1 : 0,
    };
}

function oppositeFace(face: Face): Face {
    const opposite: Record<Face, Face> = {
        U: "D", D: "U",
        R: "L", L: "R",
        F: "B", B: "F",
    };
    return opposite[face];
}

function invertQuarterTurns(quarterTurns: number): number {
    return quarterTurns === 2 ? 2 : -quarterTurns;
}

function turnSuffix(quarterTurns: number): string {
    return quarterTurns === -1 ? "'" : quarterTurns === 2 ? "2" : "";
}

export function formatMoveVirtualCubeNet(token: string, n: number): string {
    if (n < 3 || n > 5) return token;

    const move = parseMove(token);
    let symbol: string;
    let quarterTurns = move.quarterTurns;
    let wide = false;

    if (move.outerDepth === 0 && move.innerDepth === 0) {
        symbol = move.face;
    } else if (move.outerDepth === 0 && move.innerDepth === 1) {
        symbol = move.face;
        wide = true;
    } else if (
        move.outerDepth === move.innerDepth &&
        move.outerDepth > 0
    ) {
        const depth = move.outerDepth;
        if (n % 2 === 1 && depth === Math.floor(n / 2)) {
            if (move.face === "L" || move.face === "R") symbol = "M";
            else if (move.face === "D" || move.face === "U") symbol = "E";
            else symbol = "S";

            if (
                move.face === "R" ||
                move.face === "U" ||
                move.face === "B"
            ) {
                quarterTurns = invertQuarterTurns(quarterTurns);
            }
        } else if (depth === 1) {
            symbol = move.face.toLowerCase();
        } else if (depth === n - 2) {
            symbol = oppositeFace(move.face).toLowerCase();
            quarterTurns = invertQuarterTurns(quarterTurns);
        } else {
            return token;
        }
    } else {
        return token;
    }

    return `${symbol}${wide ? "w" : ""}${turnSuffix(quarterTurns)}`;
}

export function formatSequenceVirtualCubeNet(
    tokens: string[],
    n: number,
): string[] {
    return tokens.map(token => formatMoveVirtualCubeNet(token, n));
}

function normalizeVirtualCubeNetToken(token: string, n: number): string {
    const alias = /^([udfbrlMES])(2|')?$/.exec(token);
    if (!alias) return token;

    const [, symbol, suffix = ""] = alias;
    if (symbol >= "a" && symbol <= "z") {
        return `2${symbol.toUpperCase()}${suffix}`;
    }
    if (n % 2 === 0) {
        throw new Error(`${symbol} is only available on odd-sized cubes`);
    }

    const face = symbol === "M" ? "L" : symbol === "E" ? "D" : "F";
    const middleLayer = Math.floor(n / 2) + 1;
    return `${middleLayer}${face}${suffix}`;
}

export function parseUserScramble(text: string, n: number): string[] {
    const rawTokens = text.trim() === "" ? [] : text.trim().split(/\s+/);
    if (rawTokens.length === 0) {
        throw new Error("enter at least one move");
    }
    if (rawTokens.length > 40) {
        throw new Error("scrambles are limited to 40 moves");
    }

    return rawTokens.map(rawToken => {
        const token = normalizeVirtualCubeNetToken(rawToken, n);
        let move: ParsedMove;
        try {
            move = parseMove(token);
        } catch {
            throw new Error(`invalid move: ${rawToken}`);
        }
        if (move.innerDepth >= n - 1) {
            throw new Error(`${rawToken} exceeds the usable layers of a ${n}×${n}`);
        }
        if (
            n % 2 === 1 &&
            move.outerDepth <= Math.floor(n / 2) &&
            move.innerDepth >= Math.floor(n / 2)
        ) {
            throw new Error(
                `${rawToken} moves the fixed middle slice on a ${n}×${n}; ` +
                "enter a WCA-style scramble",
            );
        }
        return token;
    });
}

function coordinate(cubie: Cubie, axis: "x" | "y" | "z"): number {
    return axis === "x"
        ? cubie.position.x
        : axis === "y"
          ? cubie.position.y
          : cubie.position.z;
}

export function cubieIsInMove(
    cubie: Cubie,
    move: ParsedMove,
    n: number,
): boolean {
    const [axis, sign] = FACE_AXIS[move.face];
    const extent = cubeExtent(n);
    const value = coordinate(cubie, axis);
    for (let depth = move.outerDepth; depth <= move.innerDepth; depth++) {
        if (Math.abs(value - sign * (extent - depth)) < 1e-6) return true;
    }
    return false;
}

function snapToGrid(value: number, extent: number): number {
    return Math.round(value + extent) - extent;
}

export function rotateMove(
    cubies: Cubie[],
    move: ParsedMove,
    n: number,
): Cubie[] {
    const [axis, sign] = FACE_AXIS[move.face];
    const angle = move.quarterTurns * -sign * Math.PI / 2;
    const rotationAxis =
        axis === "x" ? new Vector3(1, 0, 0) :
        axis === "y" ? new Vector3(0, 1, 0) :
        new Vector3(0, 0, 1);
    const layerRotation = new Quaternion().setFromAxisAngle(rotationAxis, angle);
    const extent = cubeExtent(n);

    return cubies.map(cubie => {
        if (!cubieIsInMove(cubie, move, n)) return cubie;

        const position = cubie.position.clone().applyQuaternion(layerRotation);
        position.set(
            snapToGrid(position.x, extent),
            snapToGrid(position.y, extent),
            snapToGrid(position.z, extent),
        );
        const orientation = layerRotation.clone().multiply(cubie.orientation);
        return { ...cubie, position, orientation };
    });
}

export function applyMoves(
    cubies: Cubie[],
    moves: string[],
    n: number,
): Cubie[] {
    let state = cubies;
    for (const token of moves) {
        state = rotateMove(state, parseMove(token), n);
    }
    return state;
}
