#include <iostream>
#include <vector>
#include <random>
#include <queue>
#include <map>
#include <unordered_map>
#include <algorithm>
#include <list>
#include <chrono>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "extern/cereal/types/vector.hpp"
#include "extern/cereal/types/memory.hpp"

#include "extern/cdt/CDT.h"

#include "core/logger.h"
#include "core/geom.h"
#include "core/image.h"
#include "core/voronoi.h"
#include "module.h"
#include "terra.h"
#include "worldgraph.h"
#include "atlas.h"

static const glm::vec2 CAMPAIGN_NAVMESH_SCALE = {2048.F, 2048.F};

Atlas::Atlas(uint16_t heightres, uint16_t rainres, uint16_t tempres)
{
	terragen = new Terragen { heightres, rainres, tempres };

	struct rectangle area = {
		glm::vec2(0.F, 0.F),
		glm::vec2(scale.x, scale.z)
	};
	worldgraph = new Worldgraph { area };

	relief = new Image { 2048, 2048, COLORSPACE_GRAYSCALE };
	biomes = new Image { 2048, 2048, COLORSPACE_RGB };
}

Atlas::~Atlas(void)
{
	delete terragen;
	delete worldgraph;
	delete relief;
	delete biomes;
}

void Atlas::generate(long seedling, const struct worldparams *params)
{
	holdings.clear();
	holding_tiles.clear();

	seed = seedling;
	// first generate the world heightmap, rain and temperature data
	terragen->generate(seed, params);

	// then generate the world graph data (mountains, seas, rivers, etc)
	worldgraph->generate(seed, params, terragen);

	// generate holds based on generated world data
	gen_holds();
}
	
void Atlas::create_maps(void)
{
auto start = std::chrono::steady_clock::now();
	// create relief texture
	const glm::vec2 mapscale = {
		float(relief->width) / scale.x,
		float(relief->height) / scale.z
	};

	relief->clear();
	biomes->clear();

	#pragma omp parallel for
	for (const auto &t : worldgraph->tiles) {
		uint8_t color = 0;
		switch (t.relief) {
		case SEABED: color = 10; break;
		case LOWLAND: color = 128; break;
		case UPLAND: color = 160; break;
		case HIGHLAND: color = 250; break;
		}
		glm::vec2 a = mapscale * t.center;
		for (const auto &bord : t.borders) {
			glm::vec2 b = mapscale * bord->c0->position;
			glm::vec2 c = mapscale * bord->c1->position;
			relief->draw_triangle(a, b, c, CHANNEL_RED, color);
		}
	}

	/*
	std::random_device rd;
	std::mt19937 gen(seed);
	#pragma omp parallel for
	for (const auto &hold : holdings) {
		glm::vec3 rgb = {1.f, 1.f, 1.f};
		std::uniform_real_distribution<float> distrib(0.f, 1.f);
		rgb.x = distrib(gen);
		rgb.y = distrib(gen);
		rgb.z = distrib(gen);
		for (const auto &land : hold.lands) {
			glm::vec2 a = mapscale * land->center;
			for (const auto &bord : land->borders) {
				glm::vec2 b = mapscale * bord->c0->position;
				glm::vec2 c = mapscale * bord->c1->position;
				biomes->draw_triangle(a, b, c, CHANNEL_RED, 255 * rgb.x);
				biomes->draw_triangle(a, b, c, CHANNEL_GREEN, 255 * rgb.y);
				biomes->draw_triangle(a, b, c, CHANNEL_BLUE, 255 * rgb.z);
			}
		}
	}
	*/

	#pragma omp parallel for
	for (const auto &bord : worldgraph->borders) {
		if (bord.river) {
			glm::vec2 a = mapscale * bord.c0->position;
			glm::vec2 b = mapscale * bord.c1->position;
			biomes->draw_thick_line(a.x, a.y, b.x, b.y, 1, CHANNEL_RED, 0);
			biomes->draw_thick_line(a.x, a.y, b.x, b.y, 1, CHANNEL_GREEN, 0);
			biomes->draw_thick_line(a.x, a.y, b.x, b.y, 1, CHANNEL_BLUE, 255);
		}
	}

auto end = std::chrono::steady_clock::now();
std::chrono::duration<double> elapsed_seconds = end-start;
std::cout << "elapsed rasterization time: " << elapsed_seconds.count() << "s\n";

}
	
const FloatImage* Atlas::get_heightmap(void) const
{
	return terragen->heightmap;
}

const Image* Atlas::get_rainmap(void) const
{
	return terragen->rainmap;
}

const Image* Atlas::get_tempmap(void) const
{
	return terragen->tempmap;
}
	
const Image* Atlas::get_relief(void) const
{
	return relief;
}
	
const Image* Atlas::get_biomes(void) const
{
	return biomes;
}
	
void Atlas::load_heightmap(uint16_t width, uint16_t height, const std::vector<float> &data)
{
	if (width == terragen->heightmap->width && height == terragen->heightmap->height && data.size() == terragen->heightmap->size) {
		std::copy(data.begin(), data.end(), terragen->heightmap->data);
	} else {
		write_log(LogType::ERROR, "World error: could not load height map");
	}
}

void Atlas::load_rainmap(uint16_t width, uint16_t height, const std::vector<uint8_t> &data)
{
	if (width == terragen->rainmap->width && height == terragen->rainmap->height && data.size() == terragen->rainmap->size) {
		std::copy(data.begin(), data.end(), terragen->rainmap->data);
	} else {
		write_log(LogType::ERROR, "World error: could not load rain map");
	}
}

void Atlas::load_tempmap(uint16_t width, uint16_t height, const std::vector<uint8_t> &data)
{
	if (width == terragen->tempmap->width && height == terragen->tempmap->height && data.size() == terragen->tempmap->size) {
		std::copy(data.begin(), data.end(), terragen->tempmap->data);
	} else {
		write_log(LogType::ERROR, "World error: could not load temperature map");
	}
}

void Atlas::gen_holds(void)
{
	uint32_t index = 0;
	std::vector<struct tile*> candidates;
	std::unordered_map<const struct tile*, bool> visited;
	std::unordered_map<const struct tile*, int> depth;

	// create the holds
	for (auto &t : worldgraph->tiles) {
		visited[&t] = false;
		depth[&t] = 0;
		if (t.site == TOWN || t.site == CASTLE) {
			candidates.push_back(&t);
			struct holding hold;
			hold.ID = index++;
			hold.name = "unnamed";
			hold.center = &t;
			holdings.push_back(hold);
		}
	}

	// find the nearest hold center for each tile
	for (auto &hold : holdings) {
		holding_tiles[hold.center->index] = hold.ID;
		std::queue<const struct tile*> queue;
		queue.push(hold.center);
		while (!queue.empty()) {
			const struct tile *node = queue.front();
			queue.pop();
			int layer = depth[node] + 1;
			for (auto border : node->borders) {
				if (border->frontier == false && border->river == false) {
					struct tile *neighbor = border->t0 == node ? border->t1 : border->t0;
					bool valid = neighbor->relief == LOWLAND || neighbor->relief == UPLAND;
					if ((neighbor->site == VACANT || neighbor->site == RESOURCE) && valid == true) {
						if (visited[neighbor] == false) {
							visited[neighbor] = true;
							depth[neighbor] = layer;
							queue.push(neighbor);
							holding_tiles[neighbor->index] = hold.ID;
						} else if (depth[neighbor] > layer) {
							depth[neighbor] = layer;
							queue.push(neighbor);
							holding_tiles[neighbor->index] = hold.ID;
						}
					}
				}
			}
		}
	}

	// add tiles to holding
	for (auto &t : worldgraph->tiles) {
		if (holding_tiles.find(t.index) != holding_tiles.end()) {
			uint32_t ID = holding_tiles[t.index];
			holdings[ID].lands.push_back(&t);
		}
	}

	// find neighbor holdings
	std::map<std::pair<uint32_t, uint32_t>, bool> link;
	for (auto &bord : worldgraph->borders) {
		if (holding_tiles.find(bord.t0->index) != holding_tiles.end() && holding_tiles.find(bord.t1->index) != holding_tiles.end()) {
			uint32_t ID0 = holding_tiles[bord.t0->index];
			uint32_t ID1 = holding_tiles[bord.t1->index];
			if (ID0 != ID1) {
				if (link[std::minmax(ID0, ID1)] == false) {
					link[std::minmax(ID0, ID1)] = true;
					holdings[ID0].neighbors.push_back(&holdings[ID1]);
					holdings[ID1].neighbors.push_back(&holdings[ID0]);
				}
			}
		}
	}

	// sites always have to be part of a hold
	for (auto &t : worldgraph->tiles) {
		if (holding_tiles.find(t.index) == holding_tiles.end()) {
			t.site = VACANT;
		}
	}
}
	
void Atlas::create_land_navigation(void)
{
	vertex_soup.clear();
	index_soup.clear();

	struct customedge {
		std::pair<size_t, size_t> vertices;
	};

	const glm::vec2 realscale = {
		CAMPAIGN_NAVMESH_SCALE.x / worldgraph->area.max.x,
		CAMPAIGN_NAVMESH_SCALE.y / worldgraph->area.max.y,
	};

	using Triangulation = CDT::Triangulation<float>;
	Triangulation cdt = Triangulation(CDT::FindingClosestPoint::ClosestRandom, 10);

	std::vector<glm::vec2> points;
	std::vector<struct customedge> edges;

	// add polygon points for  triangulation
	std::unordered_map<uint32_t, size_t> umap;
	std::unordered_map<uint32_t, bool> marked;
	size_t index = 0;
	for (const auto &c : worldgraph->corners) {
		marked[c.index] = false;
		if (c.coast == true && c.river == false) {
			points.push_back(c.position);
			umap[c.index] = index++;
			marked[c.index] = true;
		} else if (c.frontier == true) {
			bool land = false;
			for (const auto &t : c.touches) {
				if (t->land) { land = true; }
			}
			if (land == true && c.wall == false) {
				points.push_back(c.position);
				umap[c.index] = index++;
				marked[c.index] = true;
			}
		} else if (c.wall == true) {
			points.push_back(c.position);
			umap[c.index] = index++;
			marked[c.index] = true;
		}
	}
	// add special case corners
	for (const auto &c : worldgraph->corners) {
		bool walkable = false;
		if (c.wall) {
			for (const auto &t : c.touches) {
				if (t->relief == LOWLAND || t->relief == HIGHLAND) {
					walkable = true;
				}
			}
		}
		if (marked[c.index] == false && c.frontier == true && c.wall == true && walkable == true) {
			points.push_back(c.position);
			umap[c.index] = index++;
			marked[c.index] = true;
		}
	}

	// make river polygons
	std::map<std::pair<uint32_t, uint32_t>, size_t> tilevertex;
	for (const auto &t : worldgraph->tiles) {
		for (const auto &c : t.corners) {
			if (c->river) {
				glm::vec2 vertex = segment_midpoint(t.center, c->position);
				points.push_back(vertex);
				tilevertex[std::minmax(t.index, c->index)] = index++;
			}
		}
	}
	for (const auto &b : worldgraph->borders) {
		if (b.river) {
			size_t left_t0 = tilevertex[std::minmax(b.t0->index, b.c0->index)];
			size_t right_t0 = tilevertex[std::minmax(b.t0->index, b.c1->index)];
			size_t left_t1 = tilevertex[std::minmax(b.t1->index, b.c0->index)];
			size_t right_t1 = tilevertex[std::minmax(b.t1->index, b.c1->index)];
			struct customedge edge;
			edge.vertices = std::make_pair(left_t0, right_t0);
			edges.push_back(edge);
			edge.vertices = std::make_pair(left_t1, right_t1);
			edges.push_back(edge);
		}
	}

	std::unordered_map<uint32_t, bool> marked_edges;
	std::unordered_map<uint32_t, size_t> edge_vertices;

	// add rivers
	for (const auto &b : worldgraph->borders) {
		bool half_river = b.c0->river ^ b.c1->river;
		marked_edges[b.index] = half_river;
		if (half_river) {
			glm::vec2 vertex = segment_midpoint(b.c0->position, b.c1->position);
			points.push_back(vertex);
			edge_vertices[b.index] = index++;
		} else if (b.river == false && b.c0->river && b.c1->river) {
			glm::vec2 vertex = segment_midpoint(b.c0->position, b.c1->position);
			points.push_back(vertex);
			edge_vertices[b.index] = index++;
			marked_edges[b.index] = true;
		}
	}
	for (const auto &t : worldgraph->tiles) {
		if (t.land) {
			for (const auto &b : t.borders) {
				if (marked_edges[b->index] == true) {
					if (b->c0->river) {
						size_t left = tilevertex[std::minmax(t.index, b->c0->index)];
						size_t right = edge_vertices[b->index];
						struct customedge edge;
						edge.vertices = std::make_pair(left, right);
						edges.push_back(edge);
					}
					if (b->c1->river) {
						size_t left = tilevertex[std::minmax(t.index, b->c1->index)];
						size_t right = edge_vertices[b->index];
						struct customedge edge;
						edge.vertices = std::make_pair(left, right);
						edges.push_back(edge);
					}
				}
			}
		}
	}
	for (const auto &b : worldgraph->borders) {
		if (b.coast) {
			bool half_river = b.c0->river ^ b.c1->river;
			if (half_river) {
				uint32_t index = (b.c0->river == false) ? b.c0->index : b.c1->index; 
				size_t left = umap[index];
				size_t right = edge_vertices[b.index];
				struct customedge edge;
				edge.vertices = std::make_pair(left, right);
				edges.push_back(edge);
			}
		}
	}

	// add coast and mountain edges
	for (const auto &b : worldgraph->borders) {
		size_t left = umap[b.c0->index];
		size_t right = umap[b.c1->index];
		if (marked[b.c0->index] == true && marked[b.c1->index] == true) {
			if (b.coast == true) {
				struct customedge edge;
				edge.vertices = std::make_pair(left, right);
				edges.push_back(edge);
			} else if (b.wall == true) {
				if (b.t0->relief == HIGHLAND ^ b.t1->relief  == HIGHLAND) {
					struct customedge edge;
					edge.vertices = std::make_pair(left, right);
					edges.push_back(edge);
				}
			} else if (b.frontier == true) {
				if (b.t0->land == true && b.t1->land == true) {
					struct customedge edge;
					edge.vertices = std::make_pair(left, right);
					edges.push_back(edge);
				}
			}
		}
	}

	// deluanay triangulation
	cdt.insertVertices(
		points.begin(),
		points.end(),
		[](const glm::vec2& p){ return p[0]; },
		[](const glm::vec2& p){ return p[1]; }
	);
	cdt.insertEdges(
		edges.begin(),
		edges.end(),
		[](const customedge& e){ return e.vertices.first; },
		[](const customedge& e){ return e.vertices.second; }
	);
	cdt.eraseOuterTrianglesAndHoles();

	for (int i = 0; i < cdt.vertices.size(); i++) {
		const auto pos = cdt.vertices[i].pos;
		vertex_soup.push_back(realscale.x*pos.x);
		vertex_soup.push_back(0.f);
		vertex_soup.push_back(realscale.y*pos.y);

	}
	for (const auto &triangle : cdt.triangles) {
		glm::vec2 vertices[3];
		for (int i = 0; i < 3; i++) {
			size_t index = triangle.vertices[i];
			const auto pos = cdt.vertices[index].pos;
			vertices[i].x = pos.x;
			vertices[i].y = pos.y;
		}
		if (clockwise(vertices[0], vertices[1], vertices[2])) {
			index_soup.push_back(triangle.vertices[0]);
			index_soup.push_back(triangle.vertices[1]);
			index_soup.push_back(triangle.vertices[2]);
		} else {
			index_soup.push_back(triangle.vertices[0]);
			index_soup.push_back(triangle.vertices[2]);
			index_soup.push_back(triangle.vertices[1]);
		}
	}
}
