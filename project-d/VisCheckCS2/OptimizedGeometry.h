#pragma once
#include <string>
#include <vector>
#include "Math.hpp"

class OptimizedGeometry {
public:
    // Combined triangle meshes grouped by sub-mesh.
    std::vector<std::vector<TriangleCombined>> meshes;

    // Load pre-optimized geometry from a .opt file.
    bool LoadFromFile(const std::string& optimizedFile);

    // Build optimized geometry from raw .vphys input and save to .opt.
    // Callers are responsible for selecting input/output paths.
    bool CreateOptimizedFile(const std::string& rawFile, const std::string& optimizedFile);
};
