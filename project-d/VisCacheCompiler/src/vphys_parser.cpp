#include "vphys_parser.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <fstream>
#include <cstdlib>
#include <limits>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "math_util.h"

namespace vis {

namespace {

enum class BlobType {
    None = 0,
    LocalVertexPositions = 1,
    MeshVertices = 2,
    GlobalVertices = 3,
    Triangles = 4,
    HullEdges = 5,
    HullFaces = 6,
};

struct CollisionAttributeDesc {
    std::string collision_group{};
    std::unordered_set<std::string> semantic_tags{};
    bool seen = false;
};

struct HullHalfEdge {
    std::uint8_t next = 0;
    std::uint8_t twin = 0;
    std::uint8_t origin = 0;
    std::uint8_t face = 0;
};

enum class StringListMode {
    None = 0,
    InteractAs,
    InteractWith,
    InteractExclude,
};

struct VecBits {
    std::uint32_t x;
    std::uint32_t y;
    std::uint32_t z;

    bool operator==(const VecBits& other) const {
        return x == other.x && y == other.y && z == other.z;
    }
};

struct VecBitsHash {
    std::size_t operator()(const VecBits& v) const {
        const std::uint64_t h = (static_cast<std::uint64_t>(v.x) * 11400714819323198485ull) ^
                                (static_cast<std::uint64_t>(v.y) * 14029467366897019727ull) ^
                                (static_cast<std::uint64_t>(v.z) * 1609587929392839161ull);
        return static_cast<std::size_t>(h);
    }
};

std::string trim_ascii(std::string_view value) {
    std::size_t begin = 0;
    std::size_t end = value.size();
    while (begin < end && (value[begin] == ' ' || value[begin] == '\t' || value[begin] == '\r' || value[begin] == '\n')) {
        ++begin;
    }
    while (end > begin && (value[end - 1] == ' ' || value[end - 1] == '\t' || value[end - 1] == '\r' || value[end - 1] == '\n')) {
        --end;
    }
    return std::string(value.substr(begin, end - begin));
}

std::string to_lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string normalize_identifier(std::string_view raw) {
    std::string out;
    out.reserve(raw.size());
    for (unsigned char ch : raw) {
        if ((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')) {
            out.push_back(static_cast<char>(std::tolower(ch)));
        }
    }
    return out;
}

std::vector<std::string> extract_quoted_tokens(const std::string& line) {
    std::vector<std::string> out;
    std::size_t cursor = 0;
    while (cursor < line.size()) {
        const std::size_t q0 = line.find('"', cursor);
        if (q0 == std::string::npos) {
            break;
        }
        const std::size_t q1 = line.find('"', q0 + 1);
        if (q1 == std::string::npos) {
            break;
        }
        out.emplace_back(line.substr(q0 + 1, q1 - q0 - 1));
        cursor = q1 + 1;
    }
    return out;
}

bool has_excluded_semantic_tag(const CollisionAttributeDesc& desc) {
    static const std::unordered_set<std::string> kExcludedTags = {
        "passbullets",
        "npcclip",
        "playerclip",
        "csgogrenadeclip",
        "player",
        "sky",
    };

    for (const std::string& tag : desc.semantic_tags) {
        if (kExcludedTags.find(tag) != kExcludedTags.end()) {
            return true;
        }
    }
    return false;
}

bool env_flag_enabled(const char* name, const bool default_value) {
    std::string raw_value{};
#if defined(_WIN32)
    char* raw = nullptr;
    std::size_t raw_len = 0u;
    if (_dupenv_s(&raw, &raw_len, name) == 0 && raw != nullptr) {
        raw_value.assign(raw);
        std::free(raw);
    }
#else
    if (const char* raw = std::getenv(name); raw != nullptr) {
        raw_value.assign(raw);
    }
#endif

    if (raw_value.empty()) {
        return default_value;
    }

    std::string value = to_lower_ascii(trim_ascii(raw_value));
    if (value == "1" || value == "true" || value == "yes" || value == "on") {
        return true;
    }
    if (value == "0" || value == "false" || value == "no" || value == "off") {
        return false;
    }
    return default_value;
}

bool parse_collision_attributes_table(
    const std::string& vphys_path,
    std::vector<CollisionAttributeDesc>* out_descs,
    std::string* out_error) {
    if (out_descs == nullptr) {
        if (out_error != nullptr) {
            *out_error = "collision-attributes output is null";
        }
        return false;
    }
    out_descs->clear();

    std::ifstream in(vphys_path);
    if (!in) {
        if (out_error != nullptr) {
            *out_error = "failed to open vphys for attribute scan: " + vphys_path;
        }
        return false;
    }

    bool in_table = false;
    bool in_entry = false;
    StringListMode list_mode = StringListMode::None;
    CollisionAttributeDesc current{};

    std::string line;
    while (std::getline(in, line)) {
        const std::string trimmed = trim_ascii(line);

        if (!in_table) {
            if (trimmed.find("m_collisionAttributes") != std::string::npos) {
                in_table = true;
            }
            continue;
        }

        if (!in_entry) {
            if (trimmed.empty() || trimmed == "[" || trimmed == "],") {
                continue;
            }
            if (!trimmed.empty() && trimmed.front() == ']') {
                break;
            }
            if (!trimmed.empty() && trimmed.front() == '{') {
                in_entry = true;
                list_mode = StringListMode::None;
                current = {};
                current.seen = true;
            }
            continue;
        }

        if (trimmed.find("m_CollisionGroupString") != std::string::npos) {
            const std::vector<std::string> tokens = extract_quoted_tokens(trimmed);
            if (!tokens.empty()) {
                current.collision_group = to_lower_ascii(tokens.front());
            }
        }

        if (trimmed.find("m_InteractAsStrings") != std::string::npos) {
            list_mode = StringListMode::InteractAs;
        } else if (trimmed.find("m_InteractWithStrings") != std::string::npos) {
            list_mode = StringListMode::InteractWith;
        } else if (trimmed.find("m_InteractExcludeStrings") != std::string::npos) {
            list_mode = StringListMode::InteractExclude;
        }

        if (list_mode != StringListMode::None) {
            for (const std::string& token : extract_quoted_tokens(trimmed)) {
                const std::string normalized = normalize_identifier(token);
                if (!normalized.empty()) {
                    current.semantic_tags.insert(normalized);
                }
            }
            if (trimmed.find(']') != std::string::npos) {
                list_mode = StringListMode::None;
            }
        }

        if (trimmed == "}," || trimmed == "}") {
            out_descs->push_back(std::move(current));
            current = {};
            in_entry = false;
            list_mode = StringListMode::None;
        }
    }

    return true;
}

bool parse_uint_after(const std::string& line, const std::string& marker, std::uint32_t* out_value) {
    const std::size_t pos = line.find(marker);
    if (pos == std::string::npos) {
        return false;
    }
    std::size_t idx = pos + marker.size();
    while (idx < line.size() && (line[idx] == ' ' || line[idx] == '\t')) {
        ++idx;
    }
    if (idx >= line.size() || line[idx] < '0' || line[idx] > '9') {
        return false;
    }
    std::uint64_t value = 0;
    while (idx < line.size() && line[idx] >= '0' && line[idx] <= '9') {
        value = value * 10u + static_cast<std::uint64_t>(line[idx] - '0');
        if (value > std::numeric_limits<std::uint32_t>::max()) {
            return false;
        }
        ++idx;
    }
    *out_value = static_cast<std::uint32_t>(value);
    return true;
}

int hex_value(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return 10 + (c - 'a');
    }
    if (c >= 'A' && c <= 'F') {
        return 10 + (c - 'A');
    }
    return -1;
}

void append_hex_bytes(const std::string& line, std::vector<std::uint8_t>* out_bytes) {
    const auto is_token_boundary = [](const char c) {
        return c == ' ' || c == '\t' || c == '\r' || c == '\n' ||
               c == ',' || c == '[' || c == ']' || c == '#';
    };

    for (std::size_t i = 0; i + 1 < line.size(); ++i) {
        const int hi = hex_value(line[i]);
        const int lo = hex_value(line[i + 1]);
        if (hi >= 0 && lo >= 0) {
            const bool prev_ok = (i == 0u) || is_token_boundary(line[i - 1u]);
            const bool next_ok = (i + 2u >= line.size()) || is_token_boundary(line[i + 2u]);
            if (!prev_ok || !next_ok) {
                continue;
            }
            out_bytes->push_back(static_cast<std::uint8_t>((hi << 4) | lo));
            ++i;
        }
    }
}

bool decode_vertices(const std::vector<std::uint8_t>& bytes, std::vector<Vec3>* out_vertices) {
    constexpr std::size_t kStride = sizeof(float) * 3u;
    if (bytes.size() < kStride || (bytes.size() % kStride) != 0u) {
        return false;
    }

    const std::size_t count = bytes.size() / kStride;
    if (count == 0u) {
        return false;
    }
    out_vertices->resize(count);
    for (std::size_t i = 0; i < count; ++i) {
        float f[3];
        std::memcpy(&f[0], bytes.data() + i * 12u + 0u, sizeof(float));
        std::memcpy(&f[1], bytes.data() + i * 12u + 4u, sizeof(float));
        std::memcpy(&f[2], bytes.data() + i * 12u + 8u, sizeof(float));
        if (!std::isfinite(f[0]) || !std::isfinite(f[1]) || !std::isfinite(f[2])) {
            return false;
        }
        if (std::fabs(f[0]) > 1000000.0f || std::fabs(f[1]) > 1000000.0f || std::fabs(f[2]) > 1000000.0f) {
            return false;
        }
        (*out_vertices)[i] = {f[0], f[1], f[2]};
    }
    return true;
}

bool decode_triangles(const std::vector<std::uint8_t>& bytes, std::vector<std::uint32_t>* out_indices) {
    if (bytes.size() % sizeof(std::uint32_t) != 0u) {
        return false;
    }
    const std::size_t count = bytes.size() / sizeof(std::uint32_t);
    out_indices->resize(count);
    for (std::size_t i = 0; i < count; ++i) {
        std::uint32_t v = 0;
        std::memcpy(&v, bytes.data() + i * sizeof(std::uint32_t), sizeof(std::uint32_t));
        (*out_indices)[i] = v;
    }
    return true;
}

bool decode_hull_edges(const std::vector<std::uint8_t>& bytes, std::vector<HullHalfEdge>* out_edges) {
    if (bytes.empty() || (bytes.size() % 4u) != 0u) {
        return false;
    }

    const std::size_t count = bytes.size() / 4u;
    out_edges->resize(count);
    for (std::size_t i = 0; i < count; ++i) {
        HullHalfEdge edge{};
        edge.next = bytes[i * 4u + 0u];
        edge.twin = bytes[i * 4u + 1u];
        edge.origin = bytes[i * 4u + 2u];
        edge.face = bytes[i * 4u + 3u];
        (*out_edges)[i] = edge;
    }
    return true;
}

bool decode_hull_faces(const std::vector<std::uint8_t>& bytes, std::vector<std::uint8_t>* out_faces) {
    if (bytes.empty()) {
        return false;
    }
    *out_faces = bytes;
    return true;
}

std::vector<std::uint32_t> triangulate_hull_half_edges(
    const std::vector<Vec3>& vertices,
    const std::vector<HullHalfEdge>& edges,
    const std::vector<std::uint8_t>& face_start_edges) {
    std::vector<std::uint32_t> out;
    if (vertices.size() < 3u || edges.empty() || face_start_edges.empty()) {
        return out;
    }

    constexpr float kAreaEps = 1e-8f;
    out.reserve(face_start_edges.size() * 6u);

    for (const std::uint8_t start_edge_u8 : face_start_edges) {
        const std::uint32_t start_edge = static_cast<std::uint32_t>(start_edge_u8);
        if (start_edge >= edges.size()) {
            continue;
        }

        const HullHalfEdge edge0 = edges[start_edge];
        if (edge0.origin >= vertices.size() || edge0.next >= edges.size()) {
            continue;
        }

        const Vec3& v0 = vertices[edge0.origin];
        std::uint32_t edge_index = static_cast<std::uint32_t>(edge0.next);

        for (std::size_t guard = 0; guard < edges.size(); ++guard) {
            if (edge_index >= edges.size()) {
                break;
            }

            const HullHalfEdge edge1 = edges[edge_index];
            if (edge1.next >= edges.size()) {
                break;
            }
            const HullHalfEdge edge2 = edges[edge1.next];
            if (edge1.origin >= vertices.size() || edge2.origin >= vertices.size()) {
                break;
            }

            const Vec3& v1 = vertices[edge1.origin];
            const Vec3& v2 = vertices[edge2.origin];
            const Vec3 e1 = v1 - v0;
            const Vec3 e2 = v2 - v0;
            const float area2 = dot(cross(e1, e2), cross(e1, e2));
            if (area2 >= kAreaEps) {
                out.push_back(static_cast<std::uint32_t>(edge0.origin));
                out.push_back(static_cast<std::uint32_t>(edge1.origin));
                out.push_back(static_cast<std::uint32_t>(edge2.origin));
            }

            edge_index = static_cast<std::uint32_t>(edge1.next);
            if (edge2.next >= edges.size()) {
                break;
            }
            const HullHalfEdge edge3 = edges[edge2.next];
            if (edge3.origin == edge0.origin) {
                break;
            }
        }
    }

    return out;
}

std::vector<std::uint32_t> triangulate_convex_hull_bruteforce(const std::vector<Vec3>& vertices) {
    std::vector<std::uint32_t> out;
    const std::size_t n = vertices.size();
    if (n < 3u) {
        return out;
    }

    constexpr float kAreaEps = 1e-8f;
    constexpr float kPlaneEps = 1e-4f;
    std::unordered_set<std::string> emitted_faces;

    for (std::uint32_t i = 0; i < n; ++i) {
        for (std::uint32_t j = i + 1u; j < n; ++j) {
            for (std::uint32_t k = j + 1u; k < n; ++k) {
                const Vec3 e1 = vertices[j] - vertices[i];
                const Vec3 e2 = vertices[k] - vertices[i];
                const Vec3 normal = cross(e1, e2);
                const float area2 = dot(normal, normal);
                if (area2 < kAreaEps) {
                    continue;
                }

                bool has_pos = false;
                bool has_neg = false;
                std::vector<std::uint32_t> face_verts;
                face_verts.reserve(n);
                face_verts.push_back(i);
                face_verts.push_back(j);
                face_verts.push_back(k);

                for (std::uint32_t m = 0; m < n; ++m) {
                    if (m == i || m == j || m == k) {
                        continue;
                    }
                    const float d = dot(normal, vertices[m] - vertices[i]);
                    if (d > kPlaneEps) {
                        has_pos = true;
                    } else if (d < -kPlaneEps) {
                        has_neg = true;
                    } else {
                        face_verts.push_back(m);
                    }
                    if (has_pos && has_neg) {
                        break;
                    }
                }
                if (has_pos && has_neg) {
                    continue;
                }

                std::sort(face_verts.begin(), face_verts.end());
                face_verts.erase(std::unique(face_verts.begin(), face_verts.end()), face_verts.end());
                if (face_verts.size() < 3u) {
                    continue;
                }

                std::string key;
                key.reserve(face_verts.size() * 6u);
                for (const std::uint32_t idx : face_verts) {
                    key.append(std::to_string(idx));
                    key.push_back(',');
                }
                if (!emitted_faces.insert(key).second) {
                    continue;
                }

                Vec3 center{0.0f, 0.0f, 0.0f};
                for (const std::uint32_t idx : face_verts) {
                    center = center + vertices[idx];
                }
                center = center * (1.0f / static_cast<float>(face_verts.size()));

                const Vec3 nrm = normalize(normal);
                Vec3 u = std::abs(nrm.x) > 0.9f ? Vec3{0.0f, 1.0f, 0.0f} : Vec3{1.0f, 0.0f, 0.0f};
                u = normalize(cross(u, nrm));
                Vec3 v = cross(nrm, u);

                struct OrderedVertex {
                    std::uint32_t idx;
                    float angle;
                };
                std::vector<OrderedVertex> ordered;
                ordered.reserve(face_verts.size());
                for (const std::uint32_t idx : face_verts) {
                    const Vec3 rel = vertices[idx] - center;
                    const float x = dot(rel, u);
                    const float y = dot(rel, v);
                    ordered.push_back({idx, std::atan2(y, x)});
                }
                std::sort(ordered.begin(), ordered.end(), [](const OrderedVertex& a, const OrderedVertex& b) {
                    return a.angle < b.angle;
                });

                for (std::size_t t = 1; t + 1 < ordered.size(); ++t) {
                    const std::uint32_t a = ordered[0].idx;
                    const std::uint32_t b = ordered[t].idx;
                    const std::uint32_t c = ordered[t + 1].idx;
                    const Vec3 ce1 = vertices[b] - vertices[a];
                    const Vec3 ce2 = vertices[c] - vertices[a];
                    if (dot(cross(ce1, ce2), cross(ce1, ce2)) < kAreaEps) {
                        continue;
                    }
                    out.push_back(a);
                    out.push_back(b);
                    out.push_back(c);
                }
            }
        }
    }
    return out;
}

std::uint32_t index_vertex_dedup(
    const Vec3& v,
    std::unordered_map<VecBits, std::uint32_t, VecBitsHash>* dedup,
    std::vector<Vec3>* vertices) {
    VecBits key{};
    std::memcpy(&key.x, &v.x, sizeof(float));
    std::memcpy(&key.y, &v.y, sizeof(float));
    std::memcpy(&key.z, &v.z, sizeof(float));

    const auto it = dedup->find(key);
    if (it != dedup->end()) {
        return it->second;
    }
    const std::uint32_t idx = static_cast<std::uint32_t>(vertices->size());
    vertices->push_back(v);
    dedup->insert({key, idx});
    return idx;
}

bool is_collision_index_enabled(
    const std::uint32_t idx,
    const std::unordered_set<std::uint32_t>& allowed_collision_indices,
    const std::vector<CollisionAttributeDesc>& collision_descs) {
    static const bool kEnableSemanticFilter = env_flag_enabled("VIS_SEMANTIC_FILTER", true);

    if (!allowed_collision_indices.empty() && allowed_collision_indices.find(idx) == allowed_collision_indices.end()) {
        return false;
    }
    if (!kEnableSemanticFilter) {
        return true;
    }

    if (idx >= collision_descs.size() || !collision_descs[idx].seen) {
        return true;
    }

    const CollisionAttributeDesc& desc = collision_descs[idx];
    const std::string normalized_group = normalize_identifier(desc.collision_group);

    if (has_excluded_semantic_tag(desc)) {
        return false;
    }
    if (normalized_group == "default") {
        return true;
    }
    if (normalized_group == "conditionallysolid") {
        return false;
    }

    return true;
}

} // namespace

bool parse_vphys_file(
    const std::string& vphys_path,
    const std::unordered_set<std::uint32_t>& allowed_collision_indices,
    VphysParseResult* out_result,
    std::string* out_error) {
    if (out_result == nullptr) {
        if (out_error != nullptr) {
            *out_error = "out_result is null";
        }
        return false;
    }
    *out_result = {};

    std::vector<CollisionAttributeDesc> collision_descs{};
    if (!parse_collision_attributes_table(vphys_path, &collision_descs, out_error)) {
        return false;
    }

    std::ifstream in(vphys_path);
    if (!in) {
        if (out_error != nullptr) {
            *out_error = "failed to open vphys: " + vphys_path;
        }
        return false;
    }

    std::unordered_map<VecBits, std::uint32_t, VecBitsHash> dedup;
    dedup.reserve(1 << 20);

    std::string line;
    std::uint32_t current_collision_idx = 0;
    bool has_collision_idx = false;

    BlobType blob_type = BlobType::None;
    std::vector<std::uint8_t> blob_bytes;

    std::vector<Vec3> current_vertices;
    std::vector<Vec3> global_vertices;
    std::vector<std::uint32_t> current_triangles;
    std::vector<HullHalfEdge> current_hull_edges;
    std::vector<std::uint8_t> current_hull_faces;
    std::vector<std::uint8_t> pending_mesh_vertices_blob;

    bool have_vertices = false;
    bool have_global_vertices = false;
    bool have_triangles = false;
    bool have_hull_edges = false;
    bool have_hull_faces = false;
    bool have_pending_mesh_vertices_blob = false;

    const auto clear_current_mesh = [&]() {
        have_vertices = false;
        have_triangles = false;
        have_hull_edges = false;
        have_hull_faces = false;
        have_pending_mesh_vertices_blob = false;
        current_vertices.clear();
        current_triangles.clear();
        current_hull_edges.clear();
        current_hull_faces.clear();
        pending_mesh_vertices_blob.clear();
    };

    const auto finish_blob = [&](const BlobType type) -> bool {
        if (type == BlobType::LocalVertexPositions) {
            if (!decode_vertices(blob_bytes, &current_vertices)) {
                if (out_error != nullptr) {
                    *out_error = "failed to decode m_VertexPositions blob";
                }
                return false;
            }
            have_vertices = true;
            return true;
        }

        if (type == BlobType::MeshVertices) {
            // VRF reference:
            // If a hull has explicit m_VertexPositions, then m_Vertices is an index blob, not float3 positions.
            // Defer decoding until we know whether a triangle mesh follows.
            pending_mesh_vertices_blob = blob_bytes;
            have_pending_mesh_vertices_blob = !pending_mesh_vertices_blob.empty();
            return true;
        }

        if (type == BlobType::GlobalVertices) {
            if (!decode_vertices(blob_bytes, &global_vertices)) {
                if (out_error != nullptr) {
                    *out_error = "failed to decode m_vertices blob";
                }
                return false;
            }
            have_global_vertices = true;
            return true;
        }

        if (type == BlobType::Triangles) {
            if (!have_vertices && have_pending_mesh_vertices_blob) {
                std::vector<Vec3> decoded{};
                if (decode_vertices(pending_mesh_vertices_blob, &decoded) && !decoded.empty()) {
                    current_vertices = std::move(decoded);
                    have_vertices = true;
                }
            }
            if (!decode_triangles(blob_bytes, &current_triangles)) {
                if (out_error != nullptr) {
                    *out_error = "failed to decode m_Triangles blob";
                }
                return false;
            }
            have_triangles = true;
            return true;
        }

        if (type == BlobType::HullEdges) {
            std::vector<HullHalfEdge> decoded{};
            if (decode_hull_edges(blob_bytes, &decoded)) {
                current_hull_edges = std::move(decoded);
                have_hull_edges = true;
            } else {
                have_hull_edges = false;
                current_hull_edges.clear();
            }
            return true;
        }

        if (type == BlobType::HullFaces) {
            std::vector<std::uint8_t> decoded{};
            if (decode_hull_faces(blob_bytes, &decoded)) {
                current_hull_faces = std::move(decoded);
                have_hull_faces = true;
            } else {
                have_hull_faces = false;
                current_hull_faces.clear();
            }
            return true;
        }

        return true;
    };

    const auto flush_mesh = [&]() {
        static const bool kEmitMeshTriangles = env_flag_enabled("VIS_EMIT_MESHES", true);
        static const bool kEmitHullTriangles = env_flag_enabled("VIS_EMIT_HULLS", true);

        if (!have_vertices && !have_triangles && !have_pending_mesh_vertices_blob) {
            return;
        }

        if (!has_collision_idx) {
            clear_current_mesh();
            return;
        }

        if (!have_vertices && !have_global_vertices && have_pending_mesh_vertices_blob) {
            std::vector<Vec3> decoded{};
            if (decode_vertices(pending_mesh_vertices_blob, &decoded) && !decoded.empty()) {
                current_vertices = std::move(decoded);
                have_vertices = true;
            }
        }

        const bool can_resolve_vertices = have_vertices || have_global_vertices;
        bool generated_from_hull = false;
        if (!have_triangles || !can_resolve_vertices) {
            if (have_vertices && !have_triangles) {
                if (have_hull_edges && have_hull_faces) {
                    current_triangles = triangulate_hull_half_edges(current_vertices, current_hull_edges, current_hull_faces);
                    generated_from_hull = !current_triangles.empty();
                }
                if (current_triangles.empty() && current_vertices.size() >= 3u && current_vertices.size() <= 20u) {
                    // Slow fallback only for tiny legacy hull blocks that do not provide edge/face topology.
                    current_triangles = triangulate_convex_hull_bruteforce(current_vertices);
                    generated_from_hull = !current_triangles.empty();
                }
                have_triangles = !current_triangles.empty();
                if (!have_triangles) {
                    clear_current_mesh();
                    return;
                }
            } else {
                clear_current_mesh();
                return;
            }
        }

        out_result->stats.mesh_blocks_seen++;
        if (!is_collision_index_enabled(current_collision_idx, allowed_collision_indices, collision_descs)) {
            clear_current_mesh();
            return;
        }

        out_result->stats.mesh_blocks_accepted++;
        if (current_collision_idx < 32u) {
            out_result->stats.accepted_attr_mask |= (1u << current_collision_idx);
        }

        const std::vector<Vec3>& source_vertices = have_vertices ? current_vertices : global_vertices;

        const std::size_t tri_triplet_count = current_triangles.size() / 3u;
        const std::uint8_t source_kind = generated_from_hull
            ? static_cast<std::uint8_t>(ParsedTriangleSource::Hull)
            : static_cast<std::uint8_t>(ParsedTriangleSource::Mesh);

        for (std::size_t i = 0; i < tri_triplet_count; ++i) {
            const std::uint32_t l0 = current_triangles[i * 3u + 0u];
            const std::uint32_t l1 = current_triangles[i * 3u + 1u];
            const std::uint32_t l2 = current_triangles[i * 3u + 2u];
            if (l0 >= source_vertices.size() || l1 >= source_vertices.size() || l2 >= source_vertices.size()) {
                out_result->stats.triangles_skipped_invalid++;
                continue;
            }

            const Vec3& v0 = source_vertices[l0];
            const Vec3& v1 = source_vertices[l1];
            const Vec3& v2 = source_vertices[l2];
            const Vec3 e1 = v1 - v0;
            const Vec3 e2 = v2 - v0;
            const Vec3 c = cross(e1, e2);
            const float area2 = dot(c, c);
            if (area2 < 1e-8f) {
                out_result->stats.triangles_skipped_degenerate++;
                continue;
            }

            if ((generated_from_hull && !kEmitHullTriangles) || (!generated_from_hull && !kEmitMeshTriangles)) {
                continue;
            }

            const std::uint32_t g0 = index_vertex_dedup(v0, &dedup, &out_result->mesh.vertices);
            const std::uint32_t g1 = index_vertex_dedup(v1, &dedup, &out_result->mesh.vertices);
            const std::uint32_t g2 = index_vertex_dedup(v2, &dedup, &out_result->mesh.vertices);
            out_result->mesh.indices.push_back(g0);
            out_result->mesh.indices.push_back(g1);
            out_result->mesh.indices.push_back(g2);
            out_result->triangle_sources.push_back(source_kind);
            out_result->stats.triangles_emitted++;
            if (generated_from_hull) {
                out_result->stats.triangles_emitted_from_hull++;
            } else {
                out_result->stats.triangles_emitted_from_mesh++;
            }
        }

        clear_current_mesh();
    };

    while (std::getline(in, line)) {
        if (blob_type != BlobType::None) {
            append_hex_bytes(line, &blob_bytes);
            if (line.find(']') != std::string::npos) {
                if (!finish_blob(blob_type)) {
                    return false;
                }
                blob_type = BlobType::None;
                blob_bytes.clear();

                if (have_triangles && (have_vertices || have_global_vertices || have_pending_mesh_vertices_blob)) {
                    flush_mesh();
                }
            }
            continue;
        }

        std::uint32_t next_collision_idx = 0u;
        if (parse_uint_after(line, "m_nCollisionAttributeIndex =", &next_collision_idx)) {
            if (have_vertices || have_triangles || have_hull_edges || have_hull_faces || have_pending_mesh_vertices_blob) {
                flush_mesh();
            }
            current_collision_idx = next_collision_idx;
            has_collision_idx = true;
            continue;
        }

        if (line.find("m_vertices") != std::string::npos) {
            blob_type = BlobType::GlobalVertices;
            blob_bytes.clear();
            append_hex_bytes(line, &blob_bytes);
            if (line.find(']') != std::string::npos) {
                if (!finish_blob(blob_type)) {
                    return false;
                }
                blob_type = BlobType::None;
                blob_bytes.clear();
                if (have_triangles && (have_vertices || have_global_vertices || have_pending_mesh_vertices_blob)) {
                    flush_mesh();
                }
            }
            continue;
        }

        if (line.find("m_VertexPositions") != std::string::npos || line.find("m_Vertices") != std::string::npos) {
            const bool block_has_resolved_vertices = have_vertices || have_global_vertices || have_pending_mesh_vertices_blob;
            const bool block_ready_to_flush = have_vertices || (have_triangles && block_has_resolved_vertices);
            if (block_ready_to_flush) {
                flush_mesh();
            }

            blob_type = line.find("m_VertexPositions") != std::string::npos
                ? BlobType::LocalVertexPositions
                : BlobType::MeshVertices;

            blob_bytes.clear();
            append_hex_bytes(line, &blob_bytes);
            if (line.find(']') != std::string::npos) {
                if (!finish_blob(blob_type)) {
                    return false;
                }
                blob_type = BlobType::None;
                blob_bytes.clear();
                if (have_triangles && (have_vertices || have_global_vertices || have_pending_mesh_vertices_blob)) {
                    flush_mesh();
                }
            }
            continue;
        }

        if (line.find("m_Triangles") != std::string::npos) {
            blob_type = BlobType::Triangles;
            blob_bytes.clear();
            append_hex_bytes(line, &blob_bytes);
            if (line.find(']') != std::string::npos) {
                if (!finish_blob(blob_type)) {
                    return false;
                }
                blob_type = BlobType::None;
                blob_bytes.clear();
                if (have_triangles && (have_vertices || have_global_vertices || have_pending_mesh_vertices_blob)) {
                    flush_mesh();
                }
            }
            continue;
        }

        if (line.find("m_Edges") != std::string::npos) {
            blob_type = BlobType::HullEdges;
            blob_bytes.clear();
            append_hex_bytes(line, &blob_bytes);
            if (line.find(']') != std::string::npos) {
                if (!finish_blob(blob_type)) {
                    return false;
                }
                blob_type = BlobType::None;
                blob_bytes.clear();
            }
            continue;
        }

        if (line.find("m_Faces") != std::string::npos) {
            blob_type = BlobType::HullFaces;
            blob_bytes.clear();
            append_hex_bytes(line, &blob_bytes);
            if (line.find(']') != std::string::npos) {
                if (!finish_blob(blob_type)) {
                    return false;
                }
                blob_type = BlobType::None;
                blob_bytes.clear();
            }
            continue;
        }
    }

    if (have_vertices || have_triangles || have_hull_edges || have_hull_faces || have_pending_mesh_vertices_blob) {
        flush_mesh();
    }

    if (out_result->mesh.indices.empty() || out_result->mesh.vertices.empty()) {
        if (out_error != nullptr) {
            *out_error = "no valid occluder triangles extracted from vphys";
        }
        return false;
    }

    return true;
}

} // namespace vis

