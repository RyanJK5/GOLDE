#include "HashLife.hpp"
#include "Plane.hpp"
#include "Torus.hpp"

namespace gol {
namespace {
const LifeNode* CenteredHorizontal(const HashQuadtree& data,
                                   const LifeNode& west, const LifeNode& east) {
    // Imagine these two nodes:
    // west:  east:
    // -----  -----
    // |A|B|  |E|F|
    // -----  -----
    // |C|D|  |G|H|
    // -----  -----
    // We return:
    // -----
    // |B|E|
    // -----
    // |D|G|
    // -----
    return data.FindOrCreate(west.NorthEast, east.NorthWest, west.SouthEast,
                             east.SouthWest);
}

const LifeNode* CenteredVertical(const HashQuadtree& data,
                                 const LifeNode& north, const LifeNode& south) {
    // Imagine these two nodes:
    // north: south:
    // -----  -----
    // |A|B|  |E|F|
    // -----  -----
    // |C|D|  |G|H|
    // -----  -----
    // We return:
    // -----
    // |C|D|
    // -----
    // |E|F|
    // -----

    return data.FindOrCreate(north.SouthWest, north.SouthEast, south.NorthWest,
                             south.NorthEast);
}

const LifeNode* CenteredSubNode(const HashQuadtree& data,
                                const LifeNode& node) {
    // Imagine this node:
    // ---------
    // |A|B|C|D|
    // ---------
    // |E|F|G|H|
    // ---------
    // |I|J|K|L|
    // ---------
    // |M|N|O|P|
    // ---------
    // We return:
    // -----
    // |F|G|
    // -----
    // |J|K|
    // -----
    return data.FindOrCreate(
        node.NorthWest->SouthEast, node.NorthEast->SouthWest,
        node.SouthWest->NorthEast, node.SouthEast->NorthWest);
}
} // namespace

namespace {
// ============================================================================
// Base case infrastructure for the 8x8 HashLife leaf computation.
//
// Bit layout for a 4x4 grid stored as a 16-bit value:
//   Row 0 (top):    bits 15, 14, 13, 12
//   Row 1:          bits 11, 10,  9,  8
//   Row 2:          bits  7,  6,  5,  4
//   Row 3 (bottom): bits  3,  2,  1,  0
//
// The 2x2 quadrants within this layout:
//   NW = bits {15,14,11,10} = 0xCC00
//   NE = bits {13,12, 9, 8} = 0x3300
//   SW = bits { 7, 6, 3, 2} = 0x00CC
//   SE = bits { 5, 4, 1, 0} = 0x0033
// ============================================================================

// Total number of possible 4x4 cell configurations.
constexpr uint32_t NumLeafPatterns = 65536;

// Bitmasks for extracting 2x2 quadrants from a 16-bit 4x4 grid.
constexpr uint16_t MaskNW = 0xCC00;
constexpr uint16_t MaskNE = 0x3300;
constexpr uint16_t MaskSW = 0x00CC;
constexpr uint16_t MaskSE = 0x0033;

// Precomputed lookup table for Conway's Game of Life (B3/S23).
// Maps each 16-bit 4x4 pattern to the next-generation state of the center 2x2,
// encoded in bits {5, 4, 1, 0} (the SE quadrant positions). This encoding
// allows the results of 9 overlapping lookups to be efficiently assembled
// via shifts into a full 4x4 result.

// Directly encodes a level-1 quadrant's cells into the known bit positions
// for each 2x2 sub-quadrant of the 4x4 grid.
uint16_t EncodeQuadrantNW(const LifeNode* q) {
    if (q == FalseNode)
        return 0;
    uint16_t bits = 0;
    if (q->NorthWest == TrueNode)
        bits |= (1 << 15);
    if (q->NorthEast == TrueNode)
        bits |= (1 << 14);
    if (q->SouthWest == TrueNode)
        bits |= (1 << 11);
    if (q->SouthEast == TrueNode)
        bits |= (1 << 10);
    return bits;
}

uint16_t EncodeQuadrantNE(const LifeNode* q) {
    if (q == FalseNode)
        return 0;
    uint16_t bits = 0;
    if (q->NorthWest == TrueNode)
        bits |= (1 << 13);
    if (q->NorthEast == TrueNode)
        bits |= (1 << 12);
    if (q->SouthWest == TrueNode)
        bits |= (1 << 9);
    if (q->SouthEast == TrueNode)
        bits |= (1 << 8);
    return bits;
}

uint16_t EncodeQuadrantSW(const LifeNode* q) {
    if (q == FalseNode)
        return 0;
    uint16_t bits = 0;
    if (q->NorthWest == TrueNode)
        bits |= (1 << 7);
    if (q->NorthEast == TrueNode)
        bits |= (1 << 6);
    if (q->SouthWest == TrueNode)
        bits |= (1 << 3);
    if (q->SouthEast == TrueNode)
        bits |= (1 << 2);
    return bits;
}

uint16_t EncodeQuadrantSE(const LifeNode* q) {
    if (q == FalseNode)
        return 0;
    uint16_t bits = 0;
    if (q->NorthWest == TrueNode)
        bits |= (1 << 5);
    if (q->NorthEast == TrueNode)
        bits |= (1 << 4);
    if (q->SouthWest == TrueNode)
        bits |= (1 << 1);
    if (q->SouthEast == TrueNode)
        bits |= (1 << 0);
    return bits;
}

// Encodes a level-2 node (4x4 grid of leaf cells) as a 16-bit value.
uint16_t EncodeLevel2(const LifeNode* node) {
    if (node == FalseNode ||
        (node->NorthEast == FalseNode && node->NorthWest == FalseNode &&
         node->SouthEast == FalseNode && node->SouthWest == FalseNode))
        return 0;
    return EncodeQuadrantNW(node->NorthWest) |
           EncodeQuadrantNE(node->NorthEast) |
           EncodeQuadrantSW(node->SouthWest) |
           EncodeQuadrantSE(node->SouthEast);
}

// Converts a single bit to a leaf cell pointer.
const LifeNode* BitToCell(uint16_t bits, int position) {
    return ((bits >> position) & 1) != 0 ? TrueNode : FalseNode;
}

// Decodes a 16-bit 4x4 value back to a level-2 LifeNode.
// The result is a node with four level-1 quadrant children, each
// containing four leaf cells.
const LifeNode* DecodeLevel2(const HashQuadtree& data, uint16_t bits) {
    const auto* quadrantNW =
        data.FindOrCreate(BitToCell(bits, 15), BitToCell(bits, 14),
                          BitToCell(bits, 11), BitToCell(bits, 10));
    const auto* quadrantNE =
        data.FindOrCreate(BitToCell(bits, 13), BitToCell(bits, 12),
                          BitToCell(bits, 9), BitToCell(bits, 8));
    const auto* quadrantSW =
        data.FindOrCreate(BitToCell(bits, 7), BitToCell(bits, 6),
                          BitToCell(bits, 3), BitToCell(bits, 2));
    const auto* quadrantSE =
        data.FindOrCreate(BitToCell(bits, 5), BitToCell(bits, 4),
                          BitToCell(bits, 1), BitToCell(bits, 0));
    return data.FindOrCreate(quadrantNW, quadrantNE, quadrantSW, quadrantSE);
}

// The 8x8 grid formed by 4 quadrants can be sliced into 9 overlapping 4x4
// windows. Each window consists of bits extracted and shifted from adjacent
// quadrants. These helpers construct the indices into the rule table.
//
// Window layout (each window is centered on the grid intersection shown):
//   NW   N    NE
//    W   C    E
//   SW   S    SE
//
// The bitwise shifts correspond to how Golly's `leafres` extracts overlapping
// 4x4 regions from the four 4x4 quadrant values.

uint16_t WindowN(uint16_t nw, uint16_t ne) {
    return ((nw << 2) & 0xCCCC) | ((ne >> 2) & 0x3333);
}

uint16_t WindowW(uint16_t nw, uint16_t sw) {
    return ((nw << 8) & 0xFF00) | ((sw >> 8) & 0x00FF);
}

uint16_t WindowE(uint16_t ne, uint16_t se) {
    return ((ne << 8) & 0xFF00) | ((se >> 8) & 0x00FF);
}

uint16_t WindowS(uint16_t sw, uint16_t se) {
    return ((sw << 2) & 0xCCCC) | ((se >> 2) & 0x3333);
}

uint16_t WindowCenter(uint16_t nw, uint16_t ne, uint16_t sw, uint16_t se) {
    return ((nw << 10) & MaskNW) | ((ne << 6) & MaskNE) | ((sw >> 6) & MaskSW) |
           ((se >> 10) & MaskSE);
}
} // namespace

HashLife::LeafQuadrants HashLife::EncodeLevel3(const LifeNode* node) const {
    return {EncodeLevel2(node->NorthWest), EncodeLevel2(node->NorthEast),
            EncodeLevel2(node->SouthWest), EncodeLevel2(node->SouthEast)};
}

HashLife::FirstGenResults
HashLife::ComputeFirstGeneration(const LeafQuadrants& q) const {
    return {
        s_Rule.Table()[q.nw],
        s_Rule.Table()[WindowN(q.nw, q.ne)],
        s_Rule.Table()[q.ne],
        s_Rule.Table()[WindowW(q.nw, q.sw)],
        s_Rule.Table()[WindowCenter(q.nw, q.ne, q.sw, q.se)],
        s_Rule.Table()[WindowE(q.ne, q.se)],
        s_Rule.Table()[q.sw],
        s_Rule.Table()[WindowS(q.sw, q.se)],
        s_Rule.Table()[q.se],
    };
}

namespace {
// Returns the exponent of the greatest power of two less than `stepSize`.
constexpr int32_t Log2MaxAdvanceOf(const BigInt& stepSize) {
    if (stepSize.is_zero())
        return 0;

    return static_cast<int32_t>(boost::multiprecision::msb(stepSize));
}

constexpr BigInt BigPow2(int32_t exponent) { return BigOne << exponent; }

} // namespace

namespace {
// Assembles a 4x4 result (16 bits) from four 2x2 sub-results.
// Each sub-result occupies the SE-quadrant bits {5,4,1,0} and is shifted
// into its respective quadrant position.
uint16_t AssembleQuadrants(uint16_t resultNW, uint16_t resultNE,
                           uint16_t resultSW, uint16_t resultSE) {
    return static_cast<uint16_t>(
        ((resultNW << 10) & MaskNW) | ((resultNE << 8) & MaskNE) |
        ((resultSW << 2) & MaskSW) | (resultSE & MaskSE));
}

// Combines four 2x2 results (in SE-quadrant encoding) into a 16-bit index
// for a second rule table lookup.
uint16_t Combine2x2ForLookup(uint16_t topLeft, uint16_t topRight,
                             uint16_t bottomLeft, uint16_t bottomRight) {
    return static_cast<uint16_t>((topLeft << 10) | (topRight << 8) |
                                 (bottomLeft << 2) | bottomRight);
}

} // namespace

// Assembles the 6x6 one-generation result (as a centered 4x4) from the
// nine 2x2 sub-results. Each sub-result encodes a 2x2 in bits {5,4,1,0}.
// This is the "combine9" operation from Golly.
uint16_t HashLife::AssembleCentered6x6(const FirstGenResults& gen1) const {
    return static_cast<uint16_t>(
        (gen1.nw << 15) | (gen1.n << 13) | ((gen1.ne << 11) & 0x1000) |
        ((gen1.w << 7) & 0x0880) | (gen1.center << 5) |
        ((gen1.e << 3) & 0x0110) | ((gen1.sw >> 1) & 0x0008) | (gen1.s >> 3) |
        (gen1.se >> 5));
}

// 8x8 base case for HashLife. Advances a level-3 node by 2 generations,
// returning a level-2 node (center 4x4).
const LifeNode* HashLife::AdvanceBase(const HashQuadtree& data,
                                      const LifeNode* node) const {
    const auto quadrants = EncodeLevel3(node);
    const auto gen1 = ComputeFirstGeneration(quadrants);

    // Second generation: combine adjacent 2x2 results into four overlapping
    // 4x4 windows, look up each to get a 2x2 result, then assemble.
    const auto secondGenNW =
        s_Rule
            .Table()[Combine2x2ForLookup(gen1.nw, gen1.n, gen1.w, gen1.center)];
    const auto secondGenNE =
        s_Rule
            .Table()[Combine2x2ForLookup(gen1.n, gen1.ne, gen1.center, gen1.e)];
    const auto secondGenSW =
        s_Rule
            .Table()[Combine2x2ForLookup(gen1.w, gen1.center, gen1.sw, gen1.s)];
    const auto secondGenSE =
        s_Rule
            .Table()[Combine2x2ForLookup(gen1.center, gen1.e, gen1.s, gen1.se)];

    const auto resultBits =
        AssembleQuadrants(secondGenNW, secondGenNE, secondGenSW, secondGenSE);

    return DecodeLevel2(data, resultBits);
}

// 8x8 base case for 1-generation advancement. Advances a level-3 node
// by 1 generation, returning a level-2 node (center 4x4).
const LifeNode* HashLife::AdvanceBaseOneGen(const HashQuadtree& data,
                                            const LifeNode* node) const {
    const auto quadrants = EncodeLevel3(node);
    const auto gen1 = ComputeFirstGeneration(quadrants);
    const auto resultBits = AssembleCentered6x6(gen1);

    return DecodeLevel2(data, resultBits);
}

NodeUpdateInfo HashLife::AdvanceNode(const HashQuadtree& data,
                                     std::stop_token stopToken,
                                     const LifeNode* node, int32_t level,
                                     int32_t advanceDepth) const {
    if (stopToken.stop_requested())
        return {node, 0};
    if (node == FalseNode || level < 3)
        return {node, 0};

    if (advanceDepth >= 0) {
        if (level - 2 > advanceDepth)
            return AdvanceSlow(data, stopToken, node, level, advanceDepth);
    }
    return AdvanceFast(data, stopToken, node, level, advanceDepth);
}

NodeUpdateInfo HashLife::AdvanceSlow(const HashQuadtree& data,
                                     std::stop_token stopToken,
                                     const LifeNode* node, int32_t level,
                                     int32_t advanceLevel) const {
    // At a high level, AdvanceSlow will split `node` into an 8x8 grid of
    // subnodes. Then, we can use overlapping components to take the 4x4 grids
    // aligned with each corner, and then advance them into a 2x2 nodes.
    // Finally, we assemble these 2x2 nodes into the centered 4x4 node, which is
    // the result of advancing the original node. Unlike HashQuadtree, we're
    // only making recursive calls one level down, and thus have more
    // fine-grained control over how many generations we advance.

    if (node == FalseNode)
        return {FalseNode, 0};

    // At the 8x8 base case, choose between 1-gen and 2-gen advancement.
    const auto actualLevel = (advanceLevel >= 1) ? 1 : 0;

    if (level <= 3) {
        const auto* result = (advanceLevel >= 1)
                                 ? AdvanceBase(data, node)
                                 : AdvanceBaseOneGen(data, node);

        if (stopToken.stop_requested()) {
            return {node, 0};
        }
        // Store under the requested maxAdvance so the same request hits.
        s_SlowCache[{node, advanceLevel}] = result;
        // Also store under the actual generations for cross-request reuse.
        return {result, actualLevel};
    }
    if (const auto it = s_SlowCache.find({node, advanceLevel});
        it != s_SlowCache.end()) {
        return {it->second, advanceLevel};
    }

    constexpr static auto subdivisions = 8;
    constexpr static auto index = [](int32_t x, int32_t y) {
        return y * subdivisions + x;
    };

    std::array<const LifeNode*, subdivisions * subdivisions> segments{};
    const auto fetchSegment = [&](int32_t x, int32_t y) {
        const auto* current = node;
        for (auto bit = 2; bit >= 0 && current != FalseNode; --bit) {
            const bool east = (x >> bit) & 1;
            const bool south = (y >> bit) & 1;
            if (south) {
                current = east ? current->SouthEast : current->SouthWest;
            } else {
                current = east ? current->NorthEast : current->NorthWest;
            }
            if (current == nullptr) {
                break;
            }
        }
        return current;
    };

    for (auto y = 0; y < subdivisions; ++y) {
        for (auto x = 0; x < subdivisions; ++x) {
            segments[index(x, y)] = fetchSegment(x, y);
        }
    }

    const auto combine2x2 = [&](int32_t startX, int32_t startY) {
        return data.FindOrCreate(segments[index(startX, startY)],
                                 segments[index(startX + 1, startY)],
                                 segments[index(startX, startY + 1)],
                                 segments[index(startX + 1, startY + 1)]);
    };

    const auto buildWindow = [&](int32_t startX, int32_t startY) {
        const auto* nw = combine2x2(startX, startY);
        const auto* ne = combine2x2(startX + 2, startY);
        const auto* sw = combine2x2(startX, startY + 2);
        const auto* se = combine2x2(startX + 2, startY + 2);
        return data.FindOrCreate(nw, ne, sw, se);
    };

    const auto* window00 = buildWindow(1, 1);
    const auto* window01 = buildWindow(3, 1);
    const auto* window10 = buildWindow(1, 3);
    const auto* window11 = buildWindow(3, 3);

    const auto result00 =
        AdvanceNode(data, stopToken, window00, level - 1, advanceLevel);
    const auto result01 =
        AdvanceNode(data, stopToken, window01, level - 1, advanceLevel);
    const auto result10 =
        AdvanceNode(data, stopToken, window10, level - 1, advanceLevel);
    const auto result11 =
        AdvanceNode(data, stopToken, window11, level - 1, advanceLevel);

    const auto newAdvanceLevel = result00.AdvanceLevel;
    const auto* combined = data.FindOrCreate(result00.Node, result01.Node,
                                             result10.Node, result11.Node);

    if (stopToken.stop_requested()) {
        return {node, 0};
    }

    // Store under the requested maxAdvance so the same request hits next time.
    s_SlowCache[{node, advanceLevel}] = combined;
    return {combined, newAdvanceLevel};
}

NodeUpdateInfo HashLife::AdvanceFast(const HashQuadtree& data,
                                     std::stop_token stopToken,
                                     const LifeNode* node, int32_t level,
                                     int32_t advanceLevel) const {
    // At a high level, we want to assemble a node that is half the size of
    // `node`, but centered at the same point. By following this logic all the
    // way down the recursion, we are able to safely advance the entire universe
    // without having to worry about any cells on the boundary of the universe.
    // To achieve this end, we create a grid of overlapping cells centered
    // around `node`'s center, and tactically combine them to form the four
    // quadrants of the center node that is half the size. THe key to this
    // process is that the two levels are made by recursively calling
    // AdvanceFast, which allows for logarithmic time progression.

    if (node == FalseNode)
        return {FalseNode, 0};

    if (const auto result = data.Find(node)) {
        return {*result, level - 2};
    }

    if (level == 3) {
        const auto* base = AdvanceBase(data, node);
        data.CacheResult(node, base);
        return {base, 1};
    }

    const auto n00 =
        AdvanceNode(data, stopToken, node->NorthWest, level - 1, advanceLevel);
    const auto n01 = AdvanceNode(
        data, stopToken,
        CenteredHorizontal(data, *node->NorthWest, *node->NorthEast), level - 1,
        advanceLevel);
    const auto n02 =
        AdvanceNode(data, stopToken, node->NorthEast, level - 1, advanceLevel);
    const auto n10 =
        AdvanceNode(data, stopToken,
                    CenteredVertical(data, *node->NorthWest, *node->SouthWest),
                    level - 1, advanceLevel);
    const auto n11 = AdvanceNode(data, stopToken, CenteredSubNode(data, *node),
                                 level - 1, advanceLevel);
    const auto n12 =
        AdvanceNode(data, stopToken,
                    CenteredVertical(data, *node->NorthEast, *node->SouthEast),
                    level - 1, advanceLevel);
    const auto n20 =
        AdvanceNode(data, stopToken, node->SouthWest, level - 1, advanceLevel);
    const auto n21 = AdvanceNode(
        data, stopToken,
        CenteredHorizontal(data, *node->SouthWest, *node->SouthEast), level - 1,
        advanceLevel);
    const auto n22 =
        AdvanceNode(data, stopToken, node->SouthEast, level - 1, advanceLevel);

    const auto topLeft =
        AdvanceNode(data, stopToken,
                    data.FindOrCreate(n00.Node, n01.Node, n10.Node, n11.Node),
                    level - 1, advanceLevel);
    const auto topRight =
        AdvanceNode(data, stopToken,
                    data.FindOrCreate(n01.Node, n02.Node, n11.Node, n12.Node),
                    level - 1, advanceLevel);
    const auto bottomLeft =
        AdvanceNode(data, stopToken,
                    data.FindOrCreate(n10.Node, n11.Node, n20.Node, n21.Node),
                    level - 1, advanceLevel);
    const auto bottomRight =
        AdvanceNode(data, stopToken,
                    data.FindOrCreate(n11.Node, n12.Node, n21.Node, n22.Node),
                    level - 1, advanceLevel);

    const auto* result = data.FindOrCreate(topLeft.Node, topRight.Node,
                                           bottomLeft.Node, bottomRight.Node);

    if (stopToken.stop_requested()) {
        return {node, 0};
    }

    data.CacheResult(node, result);
    return {result, level - 2};
}

static bool NeedsExpansion(const LifeNode* node, int32_t level) {
    if (node == FalseNode)
        return false;
    if (level <= 3)
        return true;

    const auto notEmpty = [&](const LifeNode* n) {
        return n != FalseNode && !n->IsEmpty;
    };

    // We simply want to consider if the outer rim of cells is completely empty,
    // and if the next level down is completely empty. This may sometimes cause
    // unnecessary expansion, but it is a fairly low-overhead procedure that may
    // actually result in better performance when hyper speed is enabled.

    const auto* nw = node->NorthWest;
    if (notEmpty(nw)) {
        if (notEmpty(nw->NorthWest) || notEmpty(nw->NorthEast) ||
            notEmpty(nw->SouthWest))
            return true;

        const auto* nwSe = nw->SouthEast;
        if (notEmpty(nwSe) &&
            (notEmpty(nwSe->NorthWest) || notEmpty(nwSe->NorthEast) ||
             notEmpty(nwSe->SouthWest)))
            return true;
    }

    const auto* ne = node->NorthEast;
    if (notEmpty(ne)) {
        if (notEmpty(ne->NorthWest) || notEmpty(ne->NorthEast) ||
            notEmpty(ne->SouthEast))
            return true;

        const auto* neSw = ne->SouthWest;
        if (notEmpty(neSw) &&
            (notEmpty(neSw->NorthWest) || notEmpty(neSw->NorthEast) ||
             notEmpty(neSw->SouthEast)))
            return true;
    }

    const auto* sw = node->SouthWest;
    if (notEmpty(sw)) {
        if (notEmpty(sw->NorthWest) || notEmpty(sw->SouthWest) ||
            notEmpty(sw->SouthEast))
            return true;

        const auto* swNe = sw->NorthEast;
        if (notEmpty(swNe) &&
            (notEmpty(swNe->NorthWest) || notEmpty(swNe->SouthWest) ||
             notEmpty(swNe->SouthEast)))
            return true;
    }

    const auto* se = node->SouthEast;
    if (notEmpty(se)) {
        if (notEmpty(se->NorthEast) || notEmpty(se->SouthWest) ||
            notEmpty(se->SouthEast))
            return true;

        const auto* seNw = se->NorthWest;
        if (notEmpty(seNw) &&
            (notEmpty(seNw->NorthEast) || notEmpty(seNw->SouthWest) ||
             notEmpty(seNw->SouthEast)))
            return true;
    }

    return false;
}

std::string_view HashLife::Identifier = "HashLife";

thread_local LifeRule HashLife::s_Rule = *LifeRule::Make("B3/S23");

// The cache for the HashLife algorithm when the step size is bounded.
thread_local ankerl::unordered_dense::map<SlowKey, const LifeNode*, SlowHash>
    HashLife::s_SlowCache{};

HashLife::HashLife() : m_Topology(std::make_unique<Plane>()) {
    // Reserve space for 1 million nodes to avoid rehashing
    // during early stages of the simulation.
    s_SlowCache.reserve(std::max(s_SlowCache.size(), 1UZ << 20UZ));
}

HashLife::HashLife(std::unique_ptr<Topology> topology)
    : m_Topology(std::move(topology)) {}

void HashLife::SetTopology(std::unique_ptr<Topology> topology) {
    m_Topology = std::move(topology);
}

void HashLife::SetRule(const LifeRule& rule) {
    s_Rule = rule;
    s_SlowCache.clear();

    if (rule.Bounds()) {

        m_Topology = [&] -> std::unique_ptr<Topology> {
            switch (rule.GetTopology()) {
            case TopologyKind::Plane:
                return std::make_unique<Plane>(*rule.Bounds());
            case TopologyKind::Torus:
                return std::make_unique<Torus>(*rule.Bounds());
            default:
                return std::make_unique<Plane>(*rule.Bounds());
            }
        }();
    }
}

bool HashLife::CompatibleWith(const LifeDataStructure& data) const {
    return typeid(HashQuadtree) == typeid(data);
}

std::string_view HashLife::GetIdentifier() const { return "HashLife"; }

std::unique_ptr<LifeAlgorithm> HashLife::Clone() const {
    return std::make_unique<HashLife>(m_Topology->Clone());
}

BigInt HashLife::Step(LifeDataStructure& data, const BigInt& numSteps,
                      std::stop_token stopToken) {
    auto& hashQuadtree = dynamic_cast<HashQuadtree&>(data);

    if (numSteps.is_zero())
        return BigPow2(DoOneJump(
            hashQuadtree, m_Topology->Log2MaxIncrement(numSteps), stopToken));

    BigInt generation{};
    while (generation < numSteps) {
        const auto advanceLevel =
            m_Topology->Log2MaxIncrement(numSteps - generation);

        const auto gens =
            BigPow2(DoOneJump(hashQuadtree, advanceLevel, stopToken));
        if (stopToken.stop_requested())
            return generation;
        generation += gens;
    }

    return generation;
}

int32_t HashLife::DoOneJump(HashQuadtree& data, int32_t advanceLevel,
                            std::stop_token stopToken) {
    if (data.Data() == FalseNode)
        return {};

    m_Topology->PrepareBorderCells(data);

    // The condition in this while loop is to prevent freezing when the
    // user asks for a large step size on a small pattern. For example, running
    // hyperspeed on a 2x2 block will not cause it to advance particularly fast
    // since it exhibits no expansion, but if maxAdvance is specified to 2^32,
    // we can make it happen instantly.
    const auto* root = data.Data();
    auto depth = data.CalculateDepth();
    while (NeedsExpansion(root, depth) || depth - 2 < advanceLevel) {
        root = data.ExpandUniverse(root, depth);
        depth++;
    }

    const auto advanced =
        AdvanceNode(data, stopToken, root, depth, advanceLevel);

    data.OverwriteData(advanced.Node, depth - 1);
    m_Topology->CleanupBorderCells(data);

    return advanced.AdvanceLevel;
}
} // namespace gol