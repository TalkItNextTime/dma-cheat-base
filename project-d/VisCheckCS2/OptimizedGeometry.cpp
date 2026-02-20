#include "OptimizedGeometry.h"
#include "Parser.h"

#include <fstream>
#include <iostream>

bool OptimizedGeometry::CreateOptimizedFile(const std::string& rawFile, const std::string& optimizedFile) {
    Parser parser(rawFile);
    meshes = parser.GetCombinedList();

    std::ofstream out(optimizedFile, std::ios::binary);
    if (!out) {
        std::cerr << "Failed to open optimized output file: " << optimizedFile << std::endl;
        return false;
    }

    const size_t numMeshes = meshes.size();
    out.write(reinterpret_cast<const char*>(&numMeshes), sizeof(size_t));

    for (const auto& mesh : meshes) {
        const size_t numTris = mesh.size();
        out.write(reinterpret_cast<const char*>(&numTris), sizeof(size_t));
        for (const auto& tri : mesh) {
            out.write(reinterpret_cast<const char*>(&tri.v0), sizeof(Vector3));
            out.write(reinterpret_cast<const char*>(&tri.v1), sizeof(Vector3));
            out.write(reinterpret_cast<const char*>(&tri.v2), sizeof(Vector3));
        }
    }

    out.close();
    return true;
}

bool OptimizedGeometry::LoadFromFile(const std::string& optimizedFile) {
    std::ifstream in(optimizedFile, std::ios::binary);
    if (!in) {
        std::cerr << "Failed to open optimized geometry file: " << optimizedFile << std::endl;
        return false;
    }

    in.seekg(0, std::ios::end);
    const std::streamoff fileSize = in.tellg();
    in.seekg(0, std::ios::beg);
    if (fileSize <= 0)
        return false;

    meshes.clear();

    size_t numMeshes = 0;
    in.read(reinterpret_cast<char*>(&numMeshes), sizeof(size_t));
    if (!in || numMeshes == 0)
        return false;

    meshes.reserve(numMeshes);

    const size_t bytesPerTriangle = sizeof(Vector3) * 3;
    const std::streamoff payloadBytes = fileSize - static_cast<std::streamoff>(sizeof(size_t) + numMeshes * sizeof(size_t));
    const size_t estimatedTotalTriangles = payloadBytes > 0
        ? static_cast<size_t>(payloadBytes / static_cast<std::streamoff>(bytesPerTriangle))
        : 0;
    const size_t averageTriangles = numMeshes > 0 ? (estimatedTotalTriangles / numMeshes) : 0;

    for (size_t i = 0; i < numMeshes; ++i) {
        size_t numTris = 0;
        in.read(reinterpret_cast<char*>(&numTris), sizeof(size_t));
        if (!in)
            return false;

        std::vector<TriangleCombined> mesh;
        mesh.reserve((std::max)(numTris, averageTriangles));

        for (size_t j = 0; j < numTris; ++j) {
            TriangleCombined tri{};
            in.read(reinterpret_cast<char*>(&tri.v0), sizeof(Vector3));
            in.read(reinterpret_cast<char*>(&tri.v1), sizeof(Vector3));
            in.read(reinterpret_cast<char*>(&tri.v2), sizeof(Vector3));
            if (!in)
                return false;

            mesh.push_back(tri);
        }

        meshes.push_back(std::move(mesh));
    }

    return true;
}
