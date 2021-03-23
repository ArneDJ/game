#include <iostream>
#include <unordered_map>
#include <chrono>
#include <map>
#include <vector>
#include <span>

#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <GL/gl.h>
#include <glm/glm.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "extern/recast/Recast.h"
#include "extern/recast/DetourNavMesh.h"
#include "extern/recast/DetourNavMeshQuery.h"
#include "extern/recast/ChunkyTriMesh.h"
#include "extern/recast/DetourCrowd.h"

#include "extern/imgui/imgui.h"
#include "extern/imgui/imgui_impl_sdl.h"
#include "extern/imgui/imgui_impl_opengl3.h"

#include "core/mesh.h"
#include "debugger.h"

void Debugger::render_navmeshes(void)
{
	for (const auto &mesh : navmeshes) {
		mesh->draw();
	}
}

void Debugger::add_navmesh(const dtNavMesh *mesh)
{
	std::vector<struct vertex> vertices;
	std::vector<uint16_t> indices;
	const glm::vec3 color = { 0.2f, 0.5f, 1.f };

	for (int i = 0; i < mesh->getMaxTiles(); i++) {
		const dtMeshTile *tile = mesh->getTile(i);
		if (!tile) { continue; }
		if (!tile->header) { continue; }
		dtPolyRef base = mesh->getPolyRefBase(tile);
		for (int i = 0; i < tile->header->polyCount; i++) {
			const dtPoly *p = &tile->polys[i];
			if (p->getType() == DT_POLYTYPE_OFFMESH_CONNECTION) { // Skip off-mesh links.
				continue;
			}
			const dtPolyDetail *pd = &tile->detailMeshes[i];
			for (int j = 0; j < pd->triCount; j++) {
				const uint8_t *t = &tile->detailTris[(pd->triBase+j)*4];
				for (int k = 0; k < 3; k++) {
					if (t[k] < p->vertCount) {
						float x = tile->verts[p->verts[t[k]]*3];
						float y = tile->verts[p->verts[t[k]]*3 + 1] + 0.05f;
						float z = tile->verts[p->verts[t[k]]*3 + 2];
						struct vertex v = {
							{ x, y, z},
							color
						};
						vertices.push_back(v);
					} else {
						float x = tile->detailVerts[(pd->vertBase+t[k]-p->vertCount)*3];
						float y = tile->detailVerts[(pd->vertBase+t[k]-p->vertCount)*3 + 1];
						float z = tile->detailVerts[(pd->vertBase+t[k]-p->vertCount)*3 + 2];
						struct vertex v = {
							{ x, y, z},
							color
						};
						vertices.push_back(v);
					}
				}
			}
		}
	}

	Mesh *navmesh = new Mesh { vertices, indices, GL_TRIANGLES, GL_STATIC_DRAW };
	navmeshes.push_back(navmesh);
}

void Debugger::delete_navmeshes(void)
{
	for (int i = 0; i < navmeshes.size(); i++) {
		delete navmeshes[i];
	}
	navmeshes.clear();
}
	
void Debugger::add_grid(const glm::vec2 &min, const glm::vec2 &max)
{
	std::vector<struct vertex> vertices;
	std::vector<uint16_t> indices;
	const glm::vec3 color = { 0.4f, 0.6f, 0.6f };

	for (int i = int(min.x); i < int(max.x)+1; i++) {
		struct vertex a = { { i, 0.f, min.y }, color };
		struct vertex b = { { i, 0.f, max.y }, color };
		vertices.push_back(a);
		vertices.push_back(b);
	}
	for (int i = int(min.y); i < int(max.y)+1; i++) {
		struct vertex a = { { min.x, 0.f, i }, color };
		struct vertex b = { { max.x, 0.f, i }, color };
		vertices.push_back(a);
		vertices.push_back(b);
	}
	Mesh *grid = new Mesh { vertices, indices, GL_LINES, GL_STATIC_DRAW };
	grids.push_back(grid);
}

void Debugger::render_grids(void)
{
	for (const auto &grid : grids) {
		grid->draw();
	}
}

void Debugger::teardown(void)
{
	delete_navmeshes();

	for (int i = 0; i < grids.size(); i++) {
		delete grids[i];
	}
	grids.clear();
}
