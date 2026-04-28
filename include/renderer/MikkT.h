#pragma once
#include "Mesh.h"

#include <vector>

class MikkT
{
public:
    static bool GenerateTangents(std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);

	static void GenerateNormals(std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
};
