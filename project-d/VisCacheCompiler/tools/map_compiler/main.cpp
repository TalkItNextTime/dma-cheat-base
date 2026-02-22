#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>
#include <algorithm>
#include <cctype>

#include "compiler_pipeline.h"

namespace fs = std::filesystem;

namespace {

std::string normalize_output_path_no_extension(const std::string& raw_path) {
    fs::path p(raw_path);
    if (p.has_extension()) {
        p.replace_extension("");
    }
    return p.string();
}

void print_usage() {
    std::cout
        << "map_compiler usage:\n"
        << "  Single file:\n"
        << "    map_compiler --vphys <path> --out <cache_path_without_extension> [--map <map_name>] [--allow 0,1,2,4]\n"
        << "  Batch mode:\n"
        << "    map_compiler --vphys-dir <dir> --out-dir <dir> [--allow 0,1,2,4]\n"
        << "Options:\n"
        << "  --allow <csv>         Optional index allow-list. Empty/all means no numeric restriction\n"
        << "                        (semantic filtering from m_collisionAttributes is always applied)\n"
        << "  --leaf-size <n>       BVH leaf size (default 8)\n"
        << "  --delete-source       Delete source vphys after successful compile\n"
        << "Note:\n"
        << "  Output files are generated WITHOUT extension by default.\n";
}

std::string trim_ascii(const std::string& text) {
    std::size_t begin = 0;
    std::size_t end = text.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(text[begin])) != 0) {
        ++begin;
    }
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
        --end;
    }
    return text.substr(begin, end - begin);
}

std::unordered_set<std::uint32_t> parse_allow_list(const std::string& csv) {
    std::unordered_set<std::uint32_t> out;
    std::stringstream ss(csv);
    std::string token;
    while (std::getline(ss, token, ',')) {
        token = trim_ascii(token);
        if (token.empty()) {
            continue;
        }

        std::string lower = token;
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        if (lower == "all" || lower == "*") {
            out.clear();
            return out;
        }

        const std::size_t dash = token.find('-');
        if (dash != std::string::npos && dash > 0 && dash + 1 < token.size()) {
            const std::uint32_t a = static_cast<std::uint32_t>(std::stoul(token.substr(0, dash)));
            const std::uint32_t b = static_cast<std::uint32_t>(std::stoul(token.substr(dash + 1)));
            const std::uint32_t lo = std::min(a, b);
            const std::uint32_t hi = std::max(a, b);
            for (std::uint32_t v = lo; v <= hi; ++v) {
                out.insert(v);
            }
            continue;
        }

        out.insert(static_cast<std::uint32_t>(std::stoul(token)));
    }
    return out;
}

int compile_one(const std::string& vphys, const std::string& out_cache, const std::string& map_name, const vis::CompileOptions& options, bool delete_source) {
    vis::CompileReport report{};
    std::string err;
    if (!vis::compile_vphys_to_cache(vphys, out_cache, map_name, options, &report, &err)) {
        std::cerr << "[error] " << err << "\n";
        return 1;
    }

    std::error_code ec;
    const std::uint64_t source_size = fs::file_size(vphys, ec);
    const std::uint64_t cache_size = fs::file_size(out_cache, ec);
    const double ratio = source_size > 0u ? (100.0 * static_cast<double>(cache_size) / static_cast<double>(source_size)) : 0.0;

    std::cout
        << "[ok] map=" << report.map_name
        << " tri=" << report.cache_triangles
        << " vtx=" << report.cache_vertices
        << " bvh_nodes=" << report.cache_bvh_nodes
        << " attr_mask=0x" << std::hex << report.cache_attr_mask << std::dec
        << " blocks=" << report.parse_stats.mesh_blocks_accepted << "/" << report.parse_stats.mesh_blocks_seen
        << " emitted=" << report.parse_stats.triangles_emitted
        << " mesh=" << report.parse_stats.triangles_emitted_from_mesh
        << " hull=" << report.parse_stats.triangles_emitted_from_hull
        << " skip_invalid=" << report.parse_stats.triangles_skipped_invalid
        << " skip_degen=" << report.parse_stats.triangles_skipped_degenerate
        << " time_ms=" << report.elapsed_ms
        << " size=" << cache_size << "/" << source_size << " (" << ratio << "%)\n";

    if (delete_source) {
        std::error_code rm_ec;
        fs::remove(vphys, rm_ec);
        if (rm_ec) {
            std::cerr << "[warn] failed to delete source: " << vphys << " : " << rm_ec.message() << "\n";
        } else {
            std::cout << "[ok] deleted source " << vphys << "\n";
        }
    }
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (argc <= 1) {
        print_usage();
        return 1;
    }

    std::string vphys_path;
    std::string out_path;
    std::string map_name;
    std::string vphys_dir;
    std::string out_dir;
    std::string allow_csv;
    std::uint32_t leaf_size = 8u;
    bool delete_source = false;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const auto next = [&](std::string* out) -> bool {
            if (i + 1 >= argc) {
                return false;
            }
            *out = argv[++i];
            return true;
        };
        if (arg == "--vphys") {
            if (!next(&vphys_path)) {
                std::cerr << "--vphys requires a value\n";
                return 1;
            }
        } else if (arg == "--out") {
            if (!next(&out_path)) {
                std::cerr << "--out requires a value\n";
                return 1;
            }
        } else if (arg == "--map") {
            if (!next(&map_name)) {
                std::cerr << "--map requires a value\n";
                return 1;
            }
        } else if (arg == "--vphys-dir") {
            if (!next(&vphys_dir)) {
                std::cerr << "--vphys-dir requires a value\n";
                return 1;
            }
        } else if (arg == "--out-dir") {
            if (!next(&out_dir)) {
                std::cerr << "--out-dir requires a value\n";
                return 1;
            }
        } else if (arg == "--allow") {
            if (!next(&allow_csv)) {
                std::cerr << "--allow requires csv value\n";
                return 1;
            }
        } else if (arg == "--leaf-size") {
            std::string tmp;
            if (!next(&tmp)) {
                std::cerr << "--leaf-size requires value\n";
                return 1;
            }
            leaf_size = static_cast<std::uint32_t>(std::stoul(tmp));
        } else if (arg == "--delete-source") {
            delete_source = true;
        } else if (arg == "--help" || arg == "-h") {
            print_usage();
            return 0;
        } else {
            std::cerr << "unknown argument: " << arg << "\n";
            return 1;
        }
    }

    vis::CompileOptions options{};
    options.allowed_collision_indices = parse_allow_list(allow_csv);
    options.bvh_leaf_size = leaf_size;

    if (!vphys_path.empty()) {
        if (out_path.empty()) {
            std::cerr << "single-file mode requires --out\n";
            return 1;
        }
        if (map_name.empty()) {
            map_name = fs::path(vphys_path).stem().string();
        }
        const std::string normalized_out = normalize_output_path_no_extension(out_path);
        return compile_one(vphys_path, normalized_out, map_name, options, delete_source);
    }

    if (vphys_dir.empty() || out_dir.empty()) {
        std::cerr << "batch mode requires --vphys-dir and --out-dir\n";
        return 1;
    }

    std::error_code ec;
    fs::create_directories(out_dir, ec);
    int failures = 0;

    for (const auto& entry : fs::directory_iterator(vphys_dir)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        if (entry.path().extension() != ".vphys") {
            continue;
        }
        const std::string in = entry.path().string();
        const std::string map = entry.path().stem().string();
        const std::string out = (fs::path(out_dir) / map).string();
        const int rc = compile_one(in, out, map, options, delete_source);
        if (rc != 0) {
            failures++;
        }
    }

    if (failures > 0) {
        std::cerr << "[done] completed with " << failures << " failure(s)\n";
        return 2;
    }
    std::cout << "[done] all maps compiled\n";
    return 0;
}

