// PEO-081, PEO-087: what the pull shaders share. Mask word per cell: bit 0 open, bits 1-4
// the diagonals that may offer into it (NE, SE, SW, NW), bits 5-12 the neighbours a step
// may reach (the direction bytes' rule), bits 13-20 its openness to the wind (0-255).

const int kUnreached = -1073741824; // INT32_MIN / 2, as kWaveUnreached
const uint kDiagShift = 1u;
const uint kOpennessShift = 13u;
const uint kOpennessMax = 255u;

// The offer a neighbour holding `v` makes into a cell, through neighbour slot k
// (kNeighbours8 order): v less the distance cost, or under a wind (windy != 0) less the
// cost with the step for k taken off, scaled by the sender's openness / 255, never below 1
// (ScentWave's wind_cost). A slot the round does not carry (bit k of `allowed` clear), or
// in a gust round (need_open) an indoor sender, offers nothing.
int offer(int v, uint sender_mask, int k, int cost, int step, uint allowed, uint need_open, uint windy) {
    if (windy == 0u) {
        return v - cost;
    }
    const uint op = (sender_mask >> kOpennessShift) & kOpennessMax;
    if (((allowed >> uint(k)) & 1u) == 0u || (need_open != 0u && op == 0u)) {
        return kUnreached;
    }
    const int sign = step > 0 ? 1 : (step < 0 ? -1 : 0);
    const int scaled = int((uint(abs(step)) * op) / kOpennessMax);
    return v - max(1, cost - sign * scaled);
}
