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

#include "extern/poisson/poisson_disk_sampling.h"

#include "core/voronoi.h"
#include "core/geom.h"
#include "core/image.h"
#include "module.h"
#include "terra.h"
#include "worldgraph.h"

enum TEMPERATURE { COLD, TEMPERATE, WARM };
enum VEGETATION { ARID, DRY, HUMID };

static struct branch *insert_branch(const struct corner *confluence);
static void delete_basin(struct basin *tree);
static void prune_branches(struct branch *root);
static bool prunable(const struct branch *node);
static void stream_postorder(struct basin *tree);
static enum BIOME pick_biome(enum RELIEF relief, enum TEMPERATURE temper, enum VEGETATION veg);
static enum TEMPERATURE pick_temperature(float warmth);
static void spawn_towns(std::vector<struct tile*> &candidates, std::unordered_map<const struct tile*, bool> &visited, std::unordered_map<const struct tile*, int> &depth);
static void spawn_castles(std::vector<struct tile*> &candidates, std::unordered_map<const struct tile*, bool> &visited, std::unordered_map<const struct tile*, int> &depth);
static void spawn_villages(std::vector<struct tile*> &candidates, std::unordered_map<const struct tile*, bool> &visited, std::unordered_map<const struct tile*, int> &depth, long seed);

static const uint8_t N_RELAXATIONS = 1;
static const float BOUNDS_OFFSET = 10.F;
static const float POISSON_DISK_RADIUS = 16.F;
static const int MIN_STREAM_ORDER = 4;
static const size_t MIN_WATER_BODY = 1024;
static const size_t MIN_MOUNTAIN_BODY = 128;
static const int TOWN_SPAWN_RADIUS = 8;
static const int CASTLE_SPAWN_RADIUS = 10;
static const float MIN_RIVER_DIST = 40.F;
static const int MIN_BRANCH_SIZE = 3;
static const int MIN_BASIN_SIZE = 4;

Worldgraph::Worldgraph(const struct rectangle bounds)
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

void Worldgraph::generate(long seedling, const struct worldparams *params, const Terragen *terra)
{
	// TODO pass this down instead of keeping it
	seed = seedling;

	// reset data
	tiles.clear();
	corners.clear();
	borders.clear();
	holdings.clear();

	prune_basins();
	basins.clear();

	// now do the world generation
	gen_diagram();

	gen_relief(terra->heightmap, params);

	gen_rivers(params->erode_mountains);

	// relief has been altered by rivers, remove small mountain chains
	// and apply corrections
	if (params->erode_mountains) {
		floodfill_relief(MIN_MOUNTAIN_BODY, HIGHLAND, UPLAND);
		correct_walls();
	}

	// assign local tile amplitude
	// higher amplitude means more mountain terrain
	// lower amplitude means more flat terrain
	for (struct tile &t : tiles) {
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

	gen_biomes(terra->tempmap, terra->rainmap);

	gen_sites();

	gen_holds(); 
	// resources always have to be part of a hold
	// we can't let the peasants be independent
	for (auto &t : tiles) {
		if (t.site == RESOURCE && t.hold == nullptr) {
			t.site = VACANT;
		}
	}
}

void Worldgraph::gen_diagram(void)
{
	float radius = POISSON_DISK_RADIUS;
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
		struct tile t;
		t.index = cell.index;
		t.frontier = false;
		t.land = false;
		t.coast = false;
		t.river = false;
		t.center = cell.center;
		t.amp = 0.f;
		t.relief = SEABED;
		t.biome = SEA;
		t.site = VACANT;

		for (const auto &neighbor : cell.neighbors) {
			t.neighbors.push_back(&tiles[neighbor->index]);
		}
		for (const auto &vertex : cell.vertices) {
			t.corners.push_back(&corners[vertex->index]);
		}
		for (const auto &edge : cell.edges) {
			t.borders.push_back(&borders[edge->index]);
		}

		tiles[cell.index] = t;
	}

	// adapt vertex structures
	for (const auto &vertex : voronoi.vertices) {
		struct corner c;
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
		}

		for (const auto &cell : vertex.cells) {
			c.touches.push_back(&tiles[cell->index]);
		}

		corners[vertex.index] = c;
	}

	// adapt edge structures
	for (const auto &edge : voronoi.edges) {
		const int index = edge.index;
		borders[index].index = index;
		borders[index].c0 = &corners[edge.v0->index];
		borders[index].c1 = &corners[edge.v1->index];
		borders[index].coast = false;
		borders[index].river = false;
		borders[index].frontier = false;
		borders[index].wall = false;
		if (edge.c0 != nullptr) {
			borders[index].t0 = &tiles[edge.c0->index];
		} else {
			borders[index].t0 = &tiles[edge.c1->index];
			borders[index].t0->frontier = true;
			borders[index].frontier = true;
			borders[index].c0->frontier = true;
			borders[index].c1->frontier = true;
		}
		if (edge.c1 != nullptr) {
			borders[index].t1 = &tiles[edge.c1->index];
		} else {
			borders[index].t1 = &tiles[edge.c0->index];
			borders[index].t1->frontier = true;
			borders[index].frontier = true;
			borders[index].c0->frontier = true;
			borders[index].c1->frontier = true;
		}
	}
}

void Worldgraph::gen_relief(const FloatImage *heightmap, const struct worldparams *params)
{
	const float scale_x = float(heightmap->width) / area.max.x;
	const float scale_y = float(heightmap->height) / area.max.y;

	for (struct tile &t : tiles) {
		float height = heightmap->sample(scale_x*t.center.x, scale_y*t.center.y, CHANNEL_RED);
		t.land = (height < params->lowland) ? false : true;
		t.amp = glm::smoothstep(params->lowland, params->highland, height);
		if (height < params->lowland) {
			t.relief = SEABED;
		} else if (height < params->upland) {
			t.relief = LOWLAND;
		} else if (height < params->highland) {
			t.relief = UPLAND;
		} else {
			t.relief = HIGHLAND;
		}
	}

	//floodfill_relief(MIN_WATER_BODY, SEABED, LOWLAND);
	floodfill_relief(MIN_MOUNTAIN_BODY, HIGHLAND, UPLAND);
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
	std::unordered_map<const struct tile*, bool> umap;
	for (struct tile &t : tiles) {
		umap[&t] = false;
	}

	for (struct tile &root : tiles) {
		std::vector<struct tile*> marked;
		if (umap[&root] == false && root.relief == target) {
			std::queue<const struct tile*> queue;
			umap[&root] = true;
			queue.push(&root);
			marked.push_back(&root);

			while (queue.empty() == false) {
				const struct tile *v = queue.front();
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
			for (struct tile *t : marked) {
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
	std::unordered_map<const struct tile*, bool> candidate;
	for (struct tile &t : tiles) {
		candidate[&t] = false;
		if (t.frontier && (t.relief == LOWLAND || t.relief == UPLAND)) {
			for (const auto neighbor : t.neighbors) {
				if (neighbor->relief == HIGHLAND) { candidate[&t] = true; }
			}
		}
	}
	for (struct tile &t : tiles) {
		if (candidate[&t]) { t.relief = HIGHLAND; }
	}

	std::unordered_map<const struct tile*, bool> umap;
	for (struct tile &t : tiles) {
		umap[&t] = false;
	}

	for (struct tile &root : tiles) {
		bool foundwater = false;
		std::vector<struct tile*> marked;
		bool target = (root.relief == LOWLAND) || (root.relief == UPLAND);
		if (umap[&root] == false && target == true) {
			std::queue<const struct tile*> queue;
			umap[&root] = true;
			queue.push(&root);
			marked.push_back(&root);

			while (queue.empty() == false) {
				const struct tile *v = queue.front();
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
			for (struct tile *t : marked) {
				t->relief = HIGHLAND;
			}
		}
	}
}

void Worldgraph::gen_rivers(bool erodable)
{
	// construct the drainage basin candidate graph
	// only land and coast corners not on the edge of the map can be candidates for the graph
	std::vector<const struct corner*> graph;
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
	if (erodable) {
		erode_mountains();
	}

	trim_river_basins();

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
		struct basin &bas = *it;
		std::queue<struct branch*> queue;
		queue.push(bas.mouth);
		while (!queue.empty()) {
			struct branch *cur = queue.front();
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

	trim_stubby_rivers();

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

void Worldgraph::gen_drainage_basins(std::vector<const struct corner*> &graph)
{
	struct meta {
		bool visited;
		int elevation;
		int score;
	};

	std::unordered_map<const struct corner*, struct meta> umap;
	for (auto node : graph) {
		int weight = 0;
		for (const auto &t : node->touches) {
			if (t->relief == UPLAND) {
				weight += 3;
			} else if (t->relief == HIGHLAND) {
				weight += 4;
			}
		}
		struct meta data = { false, weight, 0 };
		umap[node] = data;
	}

	// breadth first search
	for (auto root : graph) {
		if (root->coast) {
			std::queue<const struct corner*> frontier;
			umap[root].visited = true;
			frontier.push(root);
			while (!frontier.empty()) {
				const struct corner *v = frontier.front();
				frontier.pop();
				struct meta &vdata = umap[v];
				int depth = vdata.score + vdata.elevation + 1;
				for (auto neighbor : v->adjacent) {
					if (neighbor->river == true && neighbor->coast == false) {
						struct meta &ndata = umap[neighbor];
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
			struct basin basn;
			struct branch *mouth = insert_branch(root);
			basn.mouth = mouth;
			std::queue<struct branch*> frontier;
			frontier.push(mouth);
			while (!frontier.empty()) {
				struct branch *fork = frontier.front();
				const struct corner *v = fork->confluence;
				frontier.pop();
				struct meta &vdata = umap[v];
				for (auto neighbor : v->adjacent) {
					struct meta &ndata = umap[neighbor];
					bool valid = ndata.visited == false && neighbor->coast == false;
					if (valid) {
						if (ndata.score > vdata.score && ndata.elevation >= vdata.elevation) {
							ndata.visited = true;
							struct branch *child = insert_branch(neighbor);
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

void Worldgraph::trim_river_basins(void)
{
	// prune binary tree branch if the stream order is too low
	for (auto it = basins.begin(); it != basins.end(); ) {
		struct basin &bas = *it;
		std::queue<struct branch*> queue;
		queue.push(bas.mouth);
		while (!queue.empty()) {
			struct branch *cur = queue.front();
			queue.pop();

			if (cur->right != nullptr) {
				if (prunable(cur->right)) {
					prune_branches(cur->right);
					cur->right = nullptr;
				} else {
					queue.push(cur->right);
				}
			}
			if (cur->left != nullptr) {
				if (prunable(cur->left)) {
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

void Worldgraph::trim_stubby_rivers(void)
{
	std::unordered_map<const struct branch*, int> depth;
	std::unordered_map<const struct branch*, bool> removable;
	std::unordered_map<struct branch*, struct branch*> parents;
	std::vector<struct branch*> endnodes;

	// find river end nodes
	for (auto it = basins.begin(); it != basins.end(); ) {
		struct basin &bas = *it;
		std::queue<struct branch*> queue;
		queue.push(bas.mouth);
		parents[bas.mouth] = nullptr;
		removable[bas.mouth] = false;
		while (!queue.empty()) {
			struct branch *cur = queue.front();
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
		std::queue<struct branch*> queue;
		queue.push(node);
		while (!queue.empty()) {
			struct branch *cur = queue.front();
			queue.pop();
			struct branch *parent = parents[cur];
			if (parent) {
				depth[parent] = depth[cur] + 1;
				if (parent->left != nullptr && parent->right != nullptr) {
				// reached a branch
					if (depth[cur] > -1 && depth[cur] < MIN_BRANCH_SIZE) {
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
			} else if (depth[cur] < MIN_BASIN_SIZE) {
			// reached the river mouth
			// river is simply too small so mark it for deletion
				removable[cur] = true;
			}
		}
	}

	// remove river basins if they are too small
	for (auto it = basins.begin(); it != basins.end(); ) {
		struct basin &bas = *it;
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
	std::map<std::pair<int, int>, struct border*> link;
	for (auto &b : borders) {
		b.river = false;
		link[std::minmax(b.c0->index, b.c1->index)] = &b;
	}

	for (const auto &bas : basins) {
		if (bas.mouth != nullptr) {
			std::queue<struct branch*> queue;
			queue.push(bas.mouth);
			while (!queue.empty()) {
				struct branch *cur = queue.front();
				queue.pop();
				corners[cur->confluence->index].river = true;
				corners[cur->confluence->index].depth = cur->depth;
				if (cur->right != nullptr) {
					struct border *bord = link[std::minmax(cur->confluence->index, cur->right->confluence->index)];
					if (bord) { bord->river = true; }
					queue.push(cur->right);
				}
				if (cur->left != nullptr) {
					struct border *bord = link[std::minmax(cur->confluence->index, cur->left->confluence->index)];
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
		struct basin &bas = *it;
		std::queue<struct branch*> queue;
		queue.push(bas.mouth);
		while (!queue.empty()) {
			struct branch *cur = queue.front();
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

void Worldgraph::gen_biomes(const Image *tempmap, const Image *rainmap)
{
	std::mt19937 gen(seed);
	const glm::vec2 scale_temp = {
		float(tempmap->width) / area.max.x,
		float(tempmap->height) / area.max.y
	};
	const glm::vec2 scale_rain = {
		float(rainmap->width) / area.max.x,
		float(rainmap->height) / area.max.y
	};

	for (struct tile &t : tiles) {
		float warmth = tempmap->sample(scale_temp.x*t.center.x, scale_temp.y*t.center.y, CHANNEL_RED) / 255.f;
		float rain = rainmap->sample(scale_rain.x*t.center.x, scale_rain.y*t.center.y, CHANNEL_RED) / 255.f;
		enum TEMPERATURE temper = (warmth > 0.5f) ? TEMPERATE : COLD;
		//enum TEMPERATURE temper = pick_temperature(warmth);
		enum VEGETATION veg;
		if (rain < 0.25f) {
			veg = ARID;
		} else {
			float p = glm::smoothstep(0.25f, 0.6f, rain);
			std::bernoulli_distribution d(p);
			veg = d(gen) ? HUMID : DRY;
		}
		t.biome = pick_biome(t.relief, temper, veg);
		if (t.biome == DESERT && t.relief == LOWLAND && t.river == true) {
			t.biome = FLOODPLAIN;
		}
		// alpine biomes
		if (t.relief == UPLAND) {
			for (const auto &neighbor : t.neighbors) {
				if (neighbor->relief == HIGHLAND) {
					if (veg == DRY) {
						t.biome = PINE_GRASSLAND;
					} else if (veg == HUMID) {
						t.biome = PINE_FOREST;
					}
					break;
				}
			}
		}
	}
}

void Worldgraph::gen_sites(void)
{
	// add candidate tiles that can have a site on them
	std::unordered_map<const struct tile*, bool> visited;
	std::unordered_map<const struct tile*, int> depth;
	std::vector<struct tile*> candidates;
	for (auto &t : tiles) {
		visited[&t] = false;
		depth[&t] = 0;
		if (t.land == true && t.frontier == false && t.relief != HIGHLAND) {
			switch (t.biome) {
			case STEPPE :
			case PINE_GRASSLAND :
			case BROADLEAF_GRASSLAND :
			case SAVANNA :
			case SHRUBLAND :
			case FLOODPLAIN :
				candidates.push_back(&t);
			}
		}
	}

	// first priority goes to towns
	spawn_towns(candidates, visited, depth);

	// second priority goes to castles
	spawn_castles(candidates, visited, depth);

	// third priority to villages
	spawn_villages(candidates, visited, depth, seed);

	// reject sites based on chance if they're in harsh biomes
	std::mt19937 gen(seed);
	for (auto root : candidates) {
		if (root->site != VACANT && root->biome == STEPPE) {
			if (root->site == TOWN) {
				root->site = VACANT;
			} else {
				float p = root->site == RESOURCE ? 0.25f : 0.75f;
				std::bernoulli_distribution d(p);
				if (d(gen) == false) {
					root->site = VACANT;
				}
			}
		}
	}
}

void Worldgraph::gen_holds(void)
{
	int index = 0;
	std::vector<struct tile*> candidates;
	std::unordered_map<const struct tile*, bool> visited;
	std::unordered_map<const struct tile*, int> depth;
	for (auto &t : tiles) {
		visited[&t] = false;
		depth[&t] = 0;
		if (t.site == TOWN || t.site == CASTLE) {
			candidates.push_back(&t);
			struct holding hold;
			hold.index = index++;
			hold.name = "unnamed";
			hold.center = &t;
			holdings.push_back(hold);
		}
	}

	// find the nearest hold center for each tile
	for (auto &hold : holdings) {
		hold.center->hold = &hold; // mkay
		std::queue<const struct tile*> queue;
		queue.push(hold.center);
		while (!queue.empty()) {
			const struct tile *node = queue.front();
			queue.pop();
			int layer = depth[node] + 1;
			for (auto border : node->borders) {
				if (border->frontier == false && border->river == false) {
					const struct tile *neighbor = border->t0 == node ? border->t1 : border->t0;
					bool valid = neighbor->relief == LOWLAND || neighbor->relief == UPLAND;
					if ((neighbor->site == VACANT || neighbor->site == RESOURCE) && valid == true) {
						if (visited[neighbor] == false) {
							visited[neighbor] = true;
							depth[neighbor] = layer;
							queue.push(neighbor);
							tiles[neighbor->index].hold = &hold;
						} else if (depth[neighbor] > layer) {
							depth[neighbor] = layer;
							queue.push(neighbor);
							tiles[neighbor->index].hold = &hold;
						}
					}
				}
			}
		}
	}

	for (auto &t : tiles) {
		if (t.hold != nullptr) {
			t.hold->lands.push_back(&t);
		}
	}

	// find neighbors
	std::map<std::pair<int, int>, bool> link;
	for (auto &bord : borders) {
		if (bord.t0->hold != nullptr && bord.t1->hold != nullptr) {
			if (bord.t0->hold != bord.t1->hold) {
				if (link[std::minmax(bord.t0->hold->index, bord.t1->hold->index)] == false) {
					link[std::minmax(bord.t0->hold->index, bord.t1->hold->index)] = true;
					bord.t0->hold->neighbors.push_back(bord.t1->hold);
					bord.t1->hold->neighbors.push_back(bord.t0->hold);
				}
			}
		}
	}
}

static void prune_branches(struct branch *root)
{
	if (root == nullptr) { return; }

	std::queue<struct branch*> queue;
	queue.push(root);

	while (!queue.empty()) {
		struct branch *front = queue.front();
		queue.pop();
		if (front->left) { queue.push(front->left); }
		if (front->right) { queue.push(front->right); }

		delete front;
	}

	root = nullptr;
}

static void delete_basin(struct basin *tree)
{
	if (tree->mouth == nullptr) { return; }

	prune_branches(tree->mouth);

	tree->mouth = nullptr;
}

static bool prunable(const struct branch *node)
{
	// prune rivers right next to mountains
	for (const auto t : node->confluence->touches) {
		if (t->relief == HIGHLAND) { return true; }
	}

	if (node->streamorder < MIN_STREAM_ORDER) { return true; }

	return false;
}

static struct branch *insert_branch(const struct corner *confluence)
{
	struct branch *node = new branch;
	node->confluence = confluence;
	node->left = nullptr;
	node->right = nullptr;
	node->streamorder = 1;

	return node;
}

// Strahler stream order
// https://en.wikipedia.org/wiki/Strahler_number
static inline int strahler(const struct branch *node)
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
static inline int shreve(const struct branch *node)
{
	// if node has no children it is a leaf with stream order 1
	if (node->left == nullptr && node->right == nullptr) {
		return 1;
	}

	int left = (node->left != nullptr) ? node->left->streamorder : 0;
	int right = (node->right != nullptr) ? node->right->streamorder : 0;

	return left + right;
}

static inline int postorder_level(struct branch *node)
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
static void stream_postorder(struct basin *tree)
{
	std::list<struct branch*> stack;
	struct branch *prev = nullptr;
	stack.push_back(tree->mouth);

	while (!stack.empty()) {
		struct branch *current = stack.back();

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

static enum BIOME pick_biome(enum RELIEF relief, enum TEMPERATURE temper, enum VEGETATION veg)
{
	if (relief == SEABED) { return SEA; } // pretty obvious

	// mountain biomes
	if (relief == HIGHLAND) {
		if (temper == WARM && veg == ARID) {
			return BADLANDS;
		} else {
			return GLACIER;
		}
	}

	if (temper == COLD) {
		switch (veg) {
		case ARID: return STEPPE;
		case DRY: return PINE_GRASSLAND;
		case HUMID: return PINE_FOREST;
		};
	} else if (temper == TEMPERATE) {
		switch (veg) {
		case ARID: return STEPPE;
		case DRY: return BROADLEAF_GRASSLAND;
		case HUMID: return BROADLEAF_FOREST;
		};
	} else if (temper == WARM) {
		switch (veg) {
		case ARID: return DESERT;
		case DRY: return SAVANNA;
		case HUMID: return SHRUBLAND;
		};
	}

	return GLACIER; // the impossible happened
}

static void spawn_towns(std::vector<struct tile*> &candidates, std::unordered_map<const struct tile*, bool> &visited, std::unordered_map<const struct tile*, int> &depth)
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
				std::queue<const struct tile*> queue;
				queue.push(root);
				while (!queue.empty()) {
					const struct tile *node = queue.front();
					queue.pop();
					int layer = depth[node] + 1;
					for (auto neighbor : node->neighbors) {
						if (visited[neighbor] == false) {
							visited[neighbor] = true;
							if (layer < TOWN_SPAWN_RADIUS) {
								depth[neighbor] = layer;
								queue.push(neighbor);
							}
						}
					}
				}
				root->site = TOWN;
			}
		}
	}

	// second priority goes to cities inland
	for (auto root : candidates) {
		if (root->river && visited[root] == false) {
			bool valid = false;
			std::queue<const struct tile*> queue;
			queue.push(root);
			while (!queue.empty()) {
				const struct tile *node = queue.front();
				queue.pop();
				int layer = depth[node] + 1;
				for (auto neighbor : node->neighbors) {
					if (visited[neighbor] == false) {
						visited[neighbor] = true;
						if (layer < TOWN_SPAWN_RADIUS) {
							depth[neighbor] = layer;
							queue.push(neighbor);
						}
					}
				}
			}
			valid = true;

			if (valid) {
				root->site = TOWN;
			}
		}
	}
}

static void spawn_castles(std::vector<struct tile*> &candidates, std::unordered_map<const struct tile*, bool> &visited, std::unordered_map<const struct tile*, int> &depth)
{
	for (auto root : candidates) {
		if (visited[root] == false) {
			std::queue<const struct tile*> queue;
			queue.push(root);
			int max = 0;
			while (!queue.empty()) {
				const struct tile *node = queue.front();
				queue.pop();
				int layer = depth[node] + 1;
				if (layer > max) { max = layer; }
				for (auto neighbor : node->neighbors) {
					if (visited[neighbor] == false) {
						visited[neighbor] = true;
						if (layer < CASTLE_SPAWN_RADIUS) {
							depth[neighbor] = layer;
							queue.push(neighbor);
						}
					}
				}
			}
			if (max >= CASTLE_SPAWN_RADIUS) {
				root->site = CASTLE;
			}
		}
	}
}

static void spawn_villages(std::vector<struct tile*> &candidates, std::unordered_map<const struct tile*, bool> &visited, std::unordered_map<const struct tile*, int> &depth, long seed)
{
	std::mt19937 gen(seed);
	for (auto root : candidates) {
		if (root->site == VACANT) {
			bool valid = true;
			for (auto neighbor : root->neighbors) {
				if (neighbor->site != VACANT) {
					valid = false;
					break;
				}
			}
			if (valid) {
				float p = root->relief == LOWLAND ? 0.1f : 0.025f;
				if (root->biome == FLOODPLAIN) {
					p *= 2.f;
				}
				std::bernoulli_distribution d(p);
				if (d(gen) == true) {
					root->site = RESOURCE;
				}
			}
		}
	}
}

