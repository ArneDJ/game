#include <iostream>
#include <vector>
#include <algorithm>
#include <random>
#include <map>
#include <unordered_map>
#include <list>
#include <queue>
#include <chrono>
#include <glm/glm.hpp>
#include <glm/vec3.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../extern/cereal/types/vector.hpp"
#include "../extern/cereal/types/memory.hpp"

#include "../extern/poisson/poisson_disk_sampling.h"

#include "../geometry/geom.h"
#include "../geometry/voronoi.h"
#include "../util/image.h"
#include "../module/module.h"
#include "terragen.h"
#include "worldgraph.h"

namespace geography {

static branch_t *insert_branch(const corner_t *confluence);
static void delete_basin(basin_t *tree);
static void prune_branches(branch_t *root);
static bool prunable(const branch_t *node, uint8_t min_stream);
static void stream_postorder(basin_t *tree);
static void spawn_towns(std::vector<tile_t*> &candidates, std::unordered_map<const tile_t*, bool> &visited, std::unordered_map<const tile_t*, int> &depth, uint8_t radius);
static void spawn_castles(std::vector<tile_t*> &candidates, std::unordered_map<const tile_t*, bool> &visited, std::unordered_map<const tile_t*, int> &depth, uint8_t radius);
static void spawn_resources(std::vector<tile_t*> &candidates, std::unordered_map<const tile_t*, bool> &visited, std::unordered_map<const tile_t*, int> &depth, long seed);

static enum tile_regolith pick_regolith(enum RELIEF relief, uint8_t precipitation, uint8_t temperature);

static const uint8_t N_RELAXATIONS = 1;
static const float BOUNDS_OFFSET = 10.F;
static const float MIN_RIVER_DIST = 40.F;

Worldgraph::Worldgraph(const geom::rectangle_t bounds)
{
	area = bounds;
}

Worldgraph::~Worldgraph(void)
{
	prune_basins();
}

void Worldgraph::prune_basins(void)
{
	for (auto &bas : basins) {
		delete_basin(&bas);
	}
}
	
void Worldgraph::reload_references(void)
{
	for (auto &til : tiles) {
		til.neighbors.clear();
		til.corners.clear();
		til.borders.clear();
		for (uint32_t ID : til.neighborIDs) {
			til.neighbors.push_back(&tiles[ID]);
		}
		for (uint32_t ID : til.cornerIDs) {
			til.corners.push_back(&corners[ID]);
		}
		for (uint32_t ID : til.borderIDs) {
			til.borders.push_back(&borders[ID]);
		}
	}
	
	for (auto &corn : corners) {
		corn.adjacent.clear();
		corn.touches.clear();
		for (uint32_t ID : corn.adjacentIDs) {
			corn.adjacent.push_back(&corners[ID]);
		}

		for (uint32_t ID : corn.touchesIDs) {
			corn.touches.push_back(&tiles[ID]);
		}
	}

	for (auto &bord : borders) {
		bord.t0 = &tiles[bord.t0ID];
		bord.t1 = &tiles[bord.t1ID];
		bord.c0 = &corners[bord.c0ID];
		bord.c1 = &corners[bord.c1ID];
	}
}

void Worldgraph::generate(long seed, const module::worldgen_parameters_t *params, const Terragen *terra)
{
	// reset data
	tiles.clear();
	corners.clear();
	borders.clear();

	prune_basins();
	basins.clear();

	// now do the world generation
	gen_diagram(seed, params->graph.poisson_disk_radius);

	gen_relief(&terra->heightmap, params);

	gen_rivers(params);

	// relief has been altered by rivers, remove small mountain chains
	// and apply corrections
	if (params->graph.erode_mountains) {
		floodfill_relief(params->graph.min_mountain_body, HIGHLAND, UPLAND);
		correct_walls();
	}

	gen_properties(&terra->tempmap, &terra->rainmap);
	
	add_primitive_features(&terra->forestation, &terra->rainmap);

	gen_sites(seed, params);
}

void Worldgraph::gen_diagram(long seed, float radius)
{
	auto min = std::array<float, 2>{{area.min.x + BOUNDS_OFFSET, area.min.y + BOUNDS_OFFSET}};
	auto max = std::array<float, 2>{{area.max.x - BOUNDS_OFFSET, area.max.y - BOUNDS_OFFSET}};

	std::vector<std::array<float, 2>> candidates = thinks::PoissonDiskSampling(radius, min, max, 30, seed);
	std::vector<glm::vec2> locations;
	for (const auto &point : candidates) {
		locations.push_back(glm::vec2(point[0], point[1]));
	}

	// copy voronoi graph
	voronoi.gen_diagram(locations, area.min, area.max, N_RELAXATIONS);

	tiles.resize(voronoi.cells.size());
	corners.resize(voronoi.vertices.size());
	borders.resize(voronoi.edges.size());

	// adopt cell structures
	for (const auto &cell : voronoi.cells) {
		tile_t t;
		t.index = cell.index;
		t.frontier = false;
		t.land = false;
		t.coast = false;
		t.river = false;
		t.center = cell.center;
		t.amp = 0.f;
		t.precipitation = 0;
		t.temperature = 0;
		t.relief = SEABED;

		for (const auto &neighbor : cell.neighbors) {
			t.neighbors.push_back(&tiles[neighbor->index]);
			t.neighborIDs.push_back(neighbor->index);
		}
		for (const auto &vertex : cell.vertices) {
			t.corners.push_back(&corners[vertex->index]);
			t.cornerIDs.push_back(vertex->index);
		}
		for (const auto &edge : cell.edges) {
			t.borders.push_back(&borders[edge->index]);
			t.borderIDs.push_back(edge->index);
		}

		tiles[cell.index] = t;
	}

	// adapt vertex structures
	for (const auto &vertex : voronoi.vertices) {
		corner_t c;
		c.index = vertex.index;
		c.frontier = false;
		c.coast = false;
		c.river = false;
		c.wall = false;
		c.position = vertex.position;
		c.depth = 0;
		//
		for (const auto &neighbor : vertex.adjacent) {
			c.adjacent.push_back(&corners[neighbor->index]);
			c.adjacentIDs.push_back(neighbor->index);
		}

		for (const auto &cell : vertex.cells) {
			c.touches.push_back(&tiles[cell->index]);
			c.touchesIDs.push_back(cell->index);
		}

		corners[vertex.index] = c;
	}

	// adapt edge structures
	for (const auto &edge : voronoi.edges) {
		const int index = edge.index;
		borders[index].index = index;
		borders[index].c0 = &corners[edge.v0->index];
		borders[index].c1 = &corners[edge.v1->index];
		borders[index].c0ID = edge.v0->index;
		borders[index].c1ID = edge.v1->index;
		borders[index].coast = false;
		borders[index].river = false;
		borders[index].frontier = false;
		borders[index].wall = false;
		if (edge.c0 != nullptr) {
			borders[index].t0 = &tiles[edge.c0->index];
			borders[index].t0ID = edge.c0->index;
		} else {
			borders[index].t0 = &tiles[edge.c1->index];
			borders[index].t0ID = edge.c1->index;
			borders[index].t0->frontier = true;
			borders[index].frontier = true;
			borders[index].c0->frontier = true;
			borders[index].c1->frontier = true;
		}
		if (edge.c1 != nullptr) {
			borders[index].t1 = &tiles[edge.c1->index];
			borders[index].t1ID = edge.c1->index;
		} else {
			borders[index].t1 = &tiles[edge.c0->index];
			borders[index].t1ID = edge.c0->index;
			borders[index].t1->frontier = true;
			borders[index].frontier = true;
			borders[index].c0->frontier = true;
			borders[index].c1->frontier = true;
		}
	}
}

void Worldgraph::gen_relief(const util::Image<float> *heightmap, const module::worldgen_parameters_t *params)
{
	const float scale_x = float(heightmap->width()) / area.max.x;
	const float scale_y = float(heightmap->height()) / area.max.y;

	for (tile_t &t : tiles) {
		float height = heightmap->sample(scale_x*t.center.x, scale_y*t.center.y, util::CHANNEL_RED);
		t.land = (height < params->graph.lowland) ? false : true;
		t.amp = glm::smoothstep(params->graph.lowland, params->graph.highland, height);
		if (height < params->graph.lowland) {
			t.relief = SEABED;
		} else if (height < params->graph.upland) {
			t.relief = LOWLAND;
		} else if (height < params->graph.highland) {
			t.relief = UPLAND;
		} else {
			t.relief = HIGHLAND;
		}
	}

	floodfill_relief(params->graph.min_water_body, SEABED, LOWLAND);
	floodfill_relief(params->graph.min_mountain_body, HIGHLAND, UPLAND);
	remove_echoriads();

	// find coastal tiles
	for (auto &b : borders) {
		// use XOR to determine if land is different
		b.coast = b.t0->land ^ b.t1->land;

		if (b.coast == true) {
			b.t0->coast = b.coast;
			b.t1->coast = b.coast;
			b.c0->coast = b.coast;
			b.c1->coast = b.coast;
		}
	}
	// find mountain borders
	correct_walls();
}

void Worldgraph::correct_walls(void)
{
	for (auto &c : corners) {
		c.wall = false;
		bool walkable = false;
		bool nearmountain = false;
		for (const auto &t : c.touches) {
			if (t->relief == HIGHLAND)  {
				nearmountain = true;
			} else if (t->relief == UPLAND || t->relief == LOWLAND) {
				walkable = true;
			}
		}
		if (nearmountain == true && walkable == true) {
			c.wall = true;
		}
		if (nearmountain == true && c.frontier == true) {
			c.wall = true;
		}
	}
	for (auto &b : borders) {
		if (b.frontier && (b.t0->relief == HIGHLAND || b.t1->relief == HIGHLAND)) {
			b.wall = true;
		} else {
			b.wall = (b.t0->relief == HIGHLAND) ^ (b.t1->relief == HIGHLAND);
		}
	}
}

void Worldgraph::floodfill_relief(unsigned int minsize, enum RELIEF target, enum RELIEF replacement)
{
	std::unordered_map<const tile_t*, bool> umap;
	for (tile_t &t : tiles) {
		umap[&t] = false;
	}

	for (tile_t &root : tiles) {
		std::vector<tile_t*> marked;
		if (umap[&root] == false && root.relief == target) {
			std::queue<const tile_t*> queue;
			umap[&root] = true;
			queue.push(&root);
			marked.push_back(&root);

			while (queue.empty() == false) {
				const tile_t *v = queue.front();
				queue.pop();

				for (const auto &neighbor : v->neighbors) {
					if (umap[neighbor] == false) {
						umap[neighbor] = true;
						if (neighbor->relief == target) {
							queue.push(neighbor);
							marked.push_back(&tiles[neighbor->index]);
						}
					}
				}
			}
		}

		if (marked.size() > 0 && marked.size() < minsize) {
			for (tile_t *t : marked) {
				t->relief = replacement;
				if (target == SEABED) {
					t->land = true;
				}
			}
		}
	}
}

// removes encircling mountains from the worldmap
// uses a slightly different version of the floodfill algorithm
void Worldgraph::remove_echoriads(void)
{
	// add extra mountains to borders of the map
	std::unordered_map<const tile_t*, bool> candidate;
	for (tile_t &t : tiles) {
		candidate[&t] = false;
		if (t.frontier && (t.relief == LOWLAND || t.relief == UPLAND)) {
			for (const auto neighbor : t.neighbors) {
				if (neighbor->relief == HIGHLAND) { candidate[&t] = true; }
			}
		}
	}
	for (tile_t &t : tiles) {
		if (candidate[&t]) { t.relief = HIGHLAND; }
	}

	std::unordered_map<const tile_t*, bool> umap;
	for (tile_t &t : tiles) {
		umap[&t] = false;
	}

	for (tile_t &root : tiles) {
		bool foundwater = false;
		std::vector<tile_t*> marked;
		bool target = (root.relief == LOWLAND) || (root.relief == UPLAND);
		if (umap[&root] == false && target == true) {
			std::queue<const tile_t*> queue;
			umap[&root] = true;
			queue.push(&root);
			marked.push_back(&root);

			while (queue.empty() == false) {
				const tile_t *v = queue.front();
				queue.pop();

				for (const auto &neighbor : v->neighbors) {
					if (neighbor->relief == SEABED) {
						foundwater = true;
						break;
					}
					if (umap[neighbor] == false) {
						umap[neighbor] = true;
						if (neighbor->relief == LOWLAND || neighbor->relief == UPLAND) {
							queue.push(neighbor);
							marked.push_back(&tiles[neighbor->index]);
						}
					}
				}
			}
		}

		if (marked.size() > 0 && foundwater == false) {
			for (tile_t *t : marked) {
				t->relief = HIGHLAND;
			}
		}
	}
}

void Worldgraph::gen_rivers(const module::worldgen_parameters_t *params)
{
	// construct the drainage basin candidate graph
	// only land and coast corners not on the edge of the map can be candidates for the graph
	std::vector<const corner_t*> graph;
	for (auto &c : corners) {
		if (c.coast && c.frontier == false) {
			graph.push_back(&c);
			c.river = true;
		} else {
			bool land = true;
			for (const auto &t : c.touches) {
				if (t->relief == SEABED) {
					land = false;
					break;
				}
			}
			if (land && c.frontier == false) {
				graph.push_back(&c);
				c.river = true;
			}
		}
	}

	gen_drainage_basins(graph);

	// assign stream order numbers
	for (auto &bas : basins) {
		stream_postorder(&bas);
	}

	// rivers will erode some mountains
	if (params->graph.erode_mountains) {
		erode_mountains();
	}

	trim_river_basins(params->graph.min_stream_order);

	// after trimming make sure river properties of corners are correct
	for (auto &c : corners) { c.river = false; }

	correct_border_rivers();

	// remove rivers too close to each other
	for (auto &b : borders) {
		if (b.river == false && b.coast == false) {
			float d = glm::distance(b.c0->position, b.c1->position);
			// river with the smallest stream order is trimmed
			// if they have the same stream order do a coin flip
			if (b.c0->river == true && b.c1->river == true && d < MIN_RIVER_DIST) {
				if (b.c0->depth > b.c1->depth) {
					b.c1->river = false;
				} else {
					b.c0->river = false;
				}
			}
		}
		b.river = false;
	}
	// remove rivers too close to map edges or mountains
	for (auto &c : corners) {
		for (const auto &adj : c.adjacent) {
			if (adj->frontier == true || adj->wall == true) {
				c.river = false;
				break;
			}
		}
	}

	for (auto it = basins.begin(); it != basins.end(); ) {
		basin_t &bas = *it;
		std::queue<branch_t*> queue;
		queue.push(bas.mouth);
		while (!queue.empty()) {
			branch_t *cur = queue.front();
			queue.pop();

			if (cur->right != nullptr) {
				if (cur->right->confluence->river == false) {
					prune_branches(cur->right);
					cur->right = nullptr;
				} else {
					queue.push(cur->right);
				}
			}
			if (cur->left != nullptr) {
				if (cur->left->confluence->river == false) {
					prune_branches(cur->left);
					cur->left = nullptr;
				} else {
					queue.push(cur->left);
				}
			}
		}
		if (bas.mouth->right == nullptr && bas.mouth->left == nullptr) {
			delete bas.mouth;
			bas.mouth = nullptr;
			it = basins.erase(it);
		} else {
			++it;
		}
	}

	trim_stubby_rivers(params->graph.min_branch_size, params->graph.min_basin_size);

	// after trimming correct rivers again
	for (auto &c : corners) { c.river = false; }
	correct_border_rivers();

	for (auto &b : borders) {
		if (b.river) {
			b.t0->river = b.river;
			b.t1->river = b.river;
		}
	}
}

void Worldgraph::gen_drainage_basins(std::vector<const corner_t*> &graph)
{
	struct meta {
		bool visited;
		int elevation;
		int score;
	};

	std::unordered_map<const corner_t*, meta> umap;
	for (auto node : graph) {
		int weight = 0;
		for (const auto &t : node->touches) {
			if (t->relief == UPLAND) {
				weight += 3;
			} else if (t->relief == HIGHLAND) {
				weight += 4;
			}
		}
		meta data = { false, weight, 0 };
		umap[node] = data;
	}

	// breadth first search
	for (auto root : graph) {
		if (root->coast) {
			std::queue<const corner_t*> frontier;
			umap[root].visited = true;
			frontier.push(root);
			while (!frontier.empty()) {
				const corner_t *v = frontier.front();
				frontier.pop();
				meta &vdata = umap[v];
				int depth = vdata.score + vdata.elevation + 1;
				for (auto neighbor : v->adjacent) {
					if (neighbor->river == true && neighbor->coast == false) {
						meta &ndata = umap[neighbor];
						if (ndata.visited == false) {
							ndata.visited = true;
							ndata.score = depth;
							frontier.push(neighbor);
						} else if (ndata.score > depth && ndata.elevation >= vdata.elevation) {
							ndata.score = depth;
							frontier.push(neighbor);
						}
					}
				}
			}
		}
	}

	// create the drainage basin binary tree
	for (auto node : graph) {
		umap[node].visited = false;
	}
	for (auto root : graph) {
		if (root->coast) {
			umap[root].visited = true;
			basin_t basn;
			branch_t *mouth = insert_branch(root);
			basn.mouth = mouth;
			std::queue<branch_t*> frontier;
			frontier.push(mouth);
			while (!frontier.empty()) {
				branch_t *fork = frontier.front();
				const corner_t *v = fork->confluence;
				frontier.pop();
				meta &vdata = umap[v];
				for (auto neighbor : v->adjacent) {
					meta &ndata = umap[neighbor];
					bool valid = ndata.visited == false && neighbor->coast == false;
					if (valid) {
						if (ndata.score > vdata.score && ndata.elevation >= vdata.elevation) {
							ndata.visited = true;
							branch_t *child = insert_branch(neighbor);
							frontier.push(child);
							if (fork->left == nullptr) {
								fork->left = child;
							} else if (fork->right == nullptr) {
								fork->right = child;
							}
						}
					}
				}
			}
			basins.push_back(basn);
		}
	}
}

void Worldgraph::trim_river_basins(uint8_t min_stream)
{
	// prune binary tree branch if the stream order is too low
	for (auto it = basins.begin(); it != basins.end(); ) {
		basin_t &bas = *it;
		std::queue<branch_t*> queue;
		queue.push(bas.mouth);
		while (!queue.empty()) {
			branch_t *cur = queue.front();
			queue.pop();

			if (cur->right != nullptr) {
				if (prunable(cur->right, min_stream)) {
					prune_branches(cur->right);
					cur->right = nullptr;
				} else {
					queue.push(cur->right);
				}
			}
			if (cur->left != nullptr) {
				if (prunable(cur->left, min_stream)) {
					prune_branches(cur->left);
					cur->left = nullptr;
				} else {
					queue.push(cur->left);
				}
			}
		}
		if (bas.mouth->right == nullptr && bas.mouth->left == nullptr) {
			delete bas.mouth;
			bas.mouth = nullptr;
			it = basins.erase(it);
		} else {
			++it;
		}
	}
}

void Worldgraph::trim_stubby_rivers(uint8_t min_branch, uint8_t min_basin)
{
	std::unordered_map<const branch_t*, int> depth;
	std::unordered_map<const branch_t*, bool> removable;
	std::unordered_map<branch_t*, branch_t*> parents;
	std::vector<branch_t*> endnodes;

	// find river end nodes
	for (auto it = basins.begin(); it != basins.end(); ) {
		basin_t &bas = *it;
		std::queue<branch_t*> queue;
		queue.push(bas.mouth);
		parents[bas.mouth] = nullptr;
		removable[bas.mouth] = false;
		while (!queue.empty()) {
			branch_t *cur = queue.front();
			depth[cur] = -1;
			queue.pop();
			if (cur->right == nullptr && cur->left == nullptr) {
				endnodes.push_back(cur);
				depth[cur] = 0;
			} else {
				if (cur->right != nullptr) {
					queue.push(cur->right);
					parents[cur->right] = cur;
				}
				if (cur->left != nullptr) {
					queue.push(cur->left);
					parents[cur->left] = cur;
				}
			}

		}
		++it;
	}

	// starting from end nodes assign depth to nodes until they reach a branch
	for (auto &node : endnodes) {
		std::queue<branch_t*> queue;
		queue.push(node);
		while (!queue.empty()) {
			branch_t *cur = queue.front();
			queue.pop();
			branch_t *parent = parents[cur];
			if (parent) {
				depth[parent] = depth[cur] + 1;
				if (parent->left != nullptr && parent->right != nullptr) {
				// reached a branch
					if (depth[cur] > -1 && depth[cur] < min_branch) {
						prune_branches(cur);
						if (cur == parent->left) {
							parent->left = nullptr;
						} else if (cur == parent->right) {
							parent->right = nullptr;
						}
					}
				} else {
					queue.push(parent);
				}
			} else if (depth[cur] < min_basin) {
			// reached the river mouth
			// river is simply too small so mark it for deletion
				removable[cur] = true;
			}
		}
	}

	// remove river basins if they are too small
	for (auto it = basins.begin(); it != basins.end(); ) {
		basin_t &bas = *it;
		if (removable[bas.mouth]) {
			delete_basin(&bas);
			it = basins.erase(it);
		} else {
			it++;
		}
	}
}

void Worldgraph::correct_border_rivers(void)
{
	// link the borders with the river corners
	std::map<std::pair<int, int>, border_t*> link;
	for (auto &b : borders) {
		b.river = false;
		link[std::minmax(b.c0->index, b.c1->index)] = &b;
	}

	for (const auto &bas : basins) {
		if (bas.mouth != nullptr) {
			std::queue<branch_t*> queue;
			queue.push(bas.mouth);
			while (!queue.empty()) {
				branch_t *cur = queue.front();
				queue.pop();
				corners[cur->confluence->index].river = true;
				corners[cur->confluence->index].depth = cur->depth;
				if (cur->right != nullptr) {
					border_t *bord = link[std::minmax(cur->confluence->index, cur->right->confluence->index)];
					if (bord) { bord->river = true; }
					queue.push(cur->right);
				}
				if (cur->left != nullptr) {
					border_t *bord = link[std::minmax(cur->confluence->index, cur->left->confluence->index)];
					if (bord) { bord->river = true; }
					queue.push(cur->left);
				}
			}
		}
	}
}

void Worldgraph::erode_mountains(void)
{
	for (auto it = basins.begin(); it != basins.end(); ) {
		basin_t &bas = *it;
		std::queue<branch_t*> queue;
		queue.push(bas.mouth);
		while (!queue.empty()) {
			branch_t *cur = queue.front();
			queue.pop();

			for (const auto t : cur->confluence->touches) {
				if (t->relief == HIGHLAND && cur->streamorder > 2) {
					t->relief = UPLAND;
				}
			}

			if (cur->right != nullptr) {
				queue.push(cur->right);
			}
			if (cur->left != nullptr) {
				queue.push(cur->left);
			}
		}
		++it;
	}
}

void Worldgraph::gen_properties(const util::Image<uint8_t> *temperatures, const util::Image<uint8_t> *rainfall)
{
	// assign local tile amplitude
	// higher amplitude means more mountain terrain
	// lower amplitude means more flat terrain
	for (tile_t &t : tiles) {
		bool near_mountain = false;
		for (const auto &n : t.neighbors) {
			if (n->relief == HIGHLAND) {
				near_mountain = true;
				break;
			}
		}
		if (near_mountain == false) {
			t.amp = glm::clamp(t.amp, 0.25f, 0.5f);
		}
	}
	
	// assign local tile temperature and precipitation
	const glm::vec2 scale_temp = { 
		float(temperatures->width()) / area.max.x,
		float(temperatures->height()) / area.max.y
	};
	const glm::vec2 scale_rain = { 
		float(rainfall->width()) / area.max.x,
		float(rainfall->height()) / area.max.y
	};

	for (tile_t &t : tiles) {
		t.precipitation = rainfall->sample(scale_rain.x*t.center.x, scale_rain.y*t.center.y, util::CHANNEL_RED);
		t.temperature = temperatures->sample(scale_temp.x*t.center.x, scale_temp.y*t.center.y, util::CHANNEL_RED);
	}
	
	// assign regolith types
	for (tile_t &t : tiles) {
		t.regolith = pick_regolith(t.relief, t.precipitation, t.temperature);
	}
}
	
void Worldgraph::add_primitive_features(const util::Image<uint8_t> *forestation, const util::Image<uint8_t> *rainfall)
{
	for (tile_t &t : tiles) {
		if (t.river) {
			if (t.regolith == tile_regolith::SAND) {
				t.feature = tile_feature::FLOODPLAIN;
			}
		}
		if (t.regolith == tile_regolith::GRASS && t.precipitation > 200) {
			glm::vec2 center = { 
				t.center.x / area.max.x, 
				t.center.y / area.max.y 
			};
			uint8_t noise = forestation->sample_scaled(center.x, center.y, util::CHANNEL_RED);
			if (noise > 150) {
				t.feature = tile_feature::WOODS;
			}
		}
	}
}

void Worldgraph::gen_sites(long seed, const module::worldgen_parameters_t *params)
{
	// add candidate tiles that can have a site on them
	std::unordered_map<const tile_t*, bool> visited;
	std::unordered_map<const tile_t*, int> depth;
	std::vector<tile_t*> candidates;
	for (auto &t : tiles) {
		visited[&t] = false;
		depth[&t] = 0;
		bool valid_land = t.land == true && t.frontier == false && t.relief != HIGHLAND;
		// reject site on forest tile unless it is near coast or river
		bool valid_site = t.feature != tile_feature::WOODS;
		if (valid_site == false) {
			valid_site = t.coast || t.river;
		}
		if (valid_land && valid_site) {
			if (t.regolith == tile_regolith::GRASS || t.feature == tile_feature::FLOODPLAIN) {
				candidates.push_back(&t);
			}
		}
	}

	// first priority goes to towns
	spawn_towns(candidates, visited, depth, params->graph.town_spawn_radius);

	// second priority goes to castles
	spawn_castles(candidates, visited, depth, params->graph.castle_spawn_radius);

	// third priority to villages
	spawn_resources(candidates, visited, depth, seed);
}

static void prune_branches(branch_t *root)
{
	if (root == nullptr) { return; }

	std::queue<branch_t*> queue;
	queue.push(root);

	while (!queue.empty()) {
		branch_t *front = queue.front();
		queue.pop();
		if (front->left) { queue.push(front->left); }
		if (front->right) { queue.push(front->right); }

		delete front;
	}

	root = nullptr;
}

static void delete_basin(basin_t *tree)
{
	if (tree->mouth == nullptr) { return; }

	prune_branches(tree->mouth);

	tree->mouth = nullptr;
}

static bool prunable(const branch_t *node, uint8_t min_stream)
{
	// prune rivers right next to mountains
	for (const auto t : node->confluence->touches) {
		if (t->relief == HIGHLAND) { return true; }
	}

	if (node->streamorder < min_stream) { return true; }

	return false;
}

static branch_t *insert_branch(const corner_t *confluence)
{
	branch_t *node = new branch_t;
	node->confluence = confluence;
	node->left = nullptr;
	node->right = nullptr;
	node->streamorder = 1;

	return node;
}

// Strahler stream order
// https://en.wikipedia.org/wiki/Strahler_number
static inline int strahler(const branch_t *node)
{
	// if node has no children it is a leaf with stream order 1
	if (node->left == nullptr && node->right == nullptr) {
		return 1;
	}

	int left = (node->left != nullptr) ? node->left->streamorder : 0;
	int right = (node->right != nullptr) ? node->right->streamorder : 0;

	if (left == right) {
		return std::max(left, right) + 1;
	} else {
		return std::max(left, right);
	}
}

// Shreve stream order
// https://en.wikipedia.org/wiki/Stream_order#Shreve_stream_order
static inline int shreve(const branch_t *node)
{
	// if node has no children it is a leaf with stream order 1
	if (node->left == nullptr && node->right == nullptr) {
		return 1;
	}

	int left = (node->left != nullptr) ? node->left->streamorder : 0;
	int right = (node->right != nullptr) ? node->right->streamorder : 0;

	return left + right;
}

static inline int postorder_level(branch_t *node)
{
	if (node->left == nullptr && node->right == nullptr) {
		return 0;
	}

	if (node->left != nullptr && node->right != nullptr) {
		return std::max(node->left->depth, node->right->depth) + 1;
	}

	if (node->left) { return node->left->depth + 1; }
	if (node->right) { return node->right->depth + 1; }

	return 0;
}

// post order tree traversal
static void stream_postorder(basin_t *tree)
{
	std::list<branch_t*> stack;
	branch_t *prev = nullptr;
	stack.push_back(tree->mouth);

	while (!stack.empty()) {
		branch_t *current = stack.back();

		if (prev == nullptr || prev->left == current || prev->right == current) {
			if (current->left != nullptr) {
				stack.push_back(current->left);
			} else if (current->right != nullptr) {
				stack.push_back(current->right);
			} else {
				current->streamorder = strahler(current);
    				current->depth = postorder_level(current);
				stack.pop_back();
			}
		} else if (current->left == prev) {
			if (current->right != nullptr) {
				stack.push_back(current->right);
			} else {
				current->streamorder = strahler(current);
    				current->depth = postorder_level(current);
				stack.pop_back();
			}
		} else if (current->right == prev) {
			current->streamorder = strahler(current);
    			current->depth = postorder_level(current);
			stack.pop_back();
		}

		prev = current;
	}
}

static void spawn_towns(std::vector<tile_t*> &candidates, std::unordered_map<const tile_t*, bool> &visited, std::unordered_map<const tile_t*, int> &depth, uint8_t radius)
{
	// use breadth first search to mark tiles within a certain radius around a site as visited so other sites won't spawn near them
	// first priority goes to cities near the coast
	for (auto root : candidates) {
		if (root->river && root->coast && visited[root] == false) {
			bool valid = false;
			for (auto c : root->corners) {
				if (c->river && c->coast) {
					valid = true;
					break;
				}
			}
			if (valid == true) {
				std::queue<const tile_t*> queue;
				queue.push(root);
				while (!queue.empty()) {
					const tile_t *node = queue.front();
					queue.pop();
					int layer = depth[node] + 1;
					for (auto neighbor : node->neighbors) {
						if (visited[neighbor] == false) {
							visited[neighbor] = true;
							if (layer < radius) {
								depth[neighbor] = layer;
								queue.push(neighbor);
							}
						}
					}
				}
				//root->site = TOWN;
				root->feature = tile_feature::SETTLEMENT;
			}
		}
	}

	// second priority goes to cities inland
	for (auto root : candidates) {
		if (root->river && visited[root] == false) {
			bool valid = false;
			std::queue<const tile_t*> queue;
			queue.push(root);
			while (!queue.empty()) {
				const tile_t *node = queue.front();
				queue.pop();
				int layer = depth[node] + 1;
				for (auto neighbor : node->neighbors) {
					if (visited[neighbor] == false) {
						visited[neighbor] = true;
						if (layer < radius) {
							depth[neighbor] = layer;
							queue.push(neighbor);
						}
					}
				}
			}
			valid = true;

			if (valid) {
				//root->site = TOWN;
				root->feature = tile_feature::SETTLEMENT;
			}
		}
	}
}

static void spawn_castles(std::vector<tile_t*> &candidates, std::unordered_map<const tile_t*, bool> &visited, std::unordered_map<const tile_t*, int> &depth, uint8_t radius)
{
	for (auto root : candidates) {
		if (visited[root] == false) {
			std::queue<const tile_t*> queue;
			queue.push(root);
			int max = 0;
			while (!queue.empty()) {
				const tile_t *node = queue.front();
				queue.pop();
				int layer = depth[node] + 1;
				if (layer > max) { max = layer; }
				for (auto neighbor : node->neighbors) {
					if (visited[neighbor] == false) {
						visited[neighbor] = true;
						if (layer < radius) {
							depth[neighbor] = layer;
							queue.push(neighbor);
						}
					}
				}
			}
			if (max >= radius) {
				//root->site = CASTLE;
				root->feature = tile_feature::SETTLEMENT;
			}
		}
	}
}

static void spawn_resources(std::vector<tile_t*> &candidates, std::unordered_map<const tile_t*, bool> &visited, std::unordered_map<const tile_t*, int> &depth, long seed)
{
	std::mt19937 gen(seed);
	for (auto root : candidates) {
		if (root->feature != tile_feature::SETTLEMENT) {
			bool valid = true;
			for (auto neighbor : root->neighbors) {
				if (neighbor->feature == tile_feature::SETTLEMENT) {
					valid = false;
					break;
				}
			}
			if (valid) {
				float p = root->relief == LOWLAND ? 0.1f : 0.025f;
				p *= 2.f;
				std::bernoulli_distribution d(p);
				if (d(gen) == true) {
					//root->site = RESOURCE;
					root->feature = tile_feature::RESOURCE;
				}
			}
		}
	}
}

static enum tile_regolith pick_regolith(enum RELIEF relief, uint8_t precipitation, uint8_t temperature)
{
	static const uint8_t ABSOLUTE_SNOW_TEMPERATURE = 16;
	static const uint8_t ABSOLUTE_DESERT_TEMPERATURE = 240;

	static const uint8_t MIN_DESERT_PRECIPITATION = 64;
	static const uint8_t MIN_DESERT_TEMPERATURE = 160;

	// regolith caused by extreme reliefs 
	if (relief == SEABED) { 
		return tile_regolith::SAND; 
	} else if (relief == HIGHLAND) {
		if (temperature > ABSOLUTE_DESERT_TEMPERATURE) {
			return tile_regolith::STONE; 
		} else if (temperature < MIN_DESERT_TEMPERATURE || precipitation > MIN_DESERT_PRECIPITATION) { 
			return tile_regolith::SNOW; 
		}
			
		return tile_regolith::STONE; 
	}

	// extreme temperatures
	if (temperature > ABSOLUTE_DESERT_TEMPERATURE) {
		return tile_regolith::SAND;
	} else if (temperature < ABSOLUTE_SNOW_TEMPERATURE) {
		return tile_regolith::SNOW;
	}

	if (temperature > MIN_DESERT_TEMPERATURE && precipitation < MIN_DESERT_PRECIPITATION) {
		return tile_regolith::SAND;
	}

	return tile_regolith::GRASS;
}

};
