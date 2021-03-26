#include <iostream>
#include <fstream>
#include <vector>
#include <random>
#include <algorithm>
#include <list>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "core/logger.h"
#include "core/geom.h"
#include "core/image.h"
#include "core/voronoi.h"
#include "terra.h"
#include "worldgraph.h"
#include "atlas.h"
#include "save.h"
	
void Saver::save(const std::string &filepath, const Atlas *atlas)
{
	const FloatImage *heightmap = atlas->get_heightmap();
	topology.width = heightmap->width;
	topology.height = heightmap->height;
	topology.channels = heightmap->channels;
	topology.size = heightmap->size;
	topology.data.resize(heightmap->size);

	std::copy(heightmap->data, heightmap->data + heightmap->size, topology.data.begin());

	const Image *tempmap = atlas->get_tempmap();
	temperature.width = tempmap->width;
	temperature.height = tempmap->height;
	temperature.channels = tempmap->channels;
	temperature.size = tempmap->size;
	temperature.data.resize(tempmap->size);

	std::copy(tempmap->data, tempmap->data + tempmap->size, temperature.data.begin());

	const Image *rainmap = atlas->get_rainmap();
	rain.width = rainmap->width;
	rain.height = rainmap->height;
	rain.channels = rainmap->channels;
	rain.size = rainmap->size;
	rain.data.resize(rainmap->size);

	std::copy(rainmap->data, rainmap->data + rainmap->size, rain.data.begin());

	// save the graph data
	tile_records.clear();
	corner_records.clear();
	border_records.clear();

	const Worldgraph *worldgraph = atlas->get_worldgraph();

	uint32_t tilecount = worldgraph->tiles.size();
	uint32_t cornercount = worldgraph->corners.size();
	uint32_t bordercount = worldgraph->borders.size();

		// the tiles
	for (const auto &til : worldgraph->tiles) {
		struct tile_record record;
		record.index = til.index;
		record.frontier = til.frontier;
		record.land = til.land;
		record.coast = til.coast;
		record.center_x = til.center.x;
		record.center_y = til.center.y;
		for (const auto &neighbor : til.neighbors) {
			record.neighbors.push_back(neighbor->index);
		}
		for (const auto &corner : til.corners) {
			record.corners.push_back(corner->index);
		}
		for (const auto &border : til.borders) {
			record.borders.push_back(border->index);
		}
		//record.amp = til.amp;
		record.relief = uint8_t(til.relief);
		record.biome = uint8_t(til.biome);
		record.site = uint8_t(til.site);
		if (til.hold) {
			record.holding = til.hold->index;
		} else {
			record.holding = -1;
		}
		//
		tile_records.push_back(record);
	}

	// the corners
	for (const auto &corn : worldgraph->corners) {
		struct corner_record record;
		record.index = corn.index;
		record.position_x = corn.position.x;
		record.position_y = corn.position.y;
		for (const auto &adj : corn.adjacent) {
			record.adjacent.push_back(adj->index);
		}
		for (const auto &til : corn.touches) {
			record.touches.push_back(til->index);
		}
		// world data
		record.frontier = corn.frontier;
		record.coast = corn.coast;
		record.river = corn.river;
		record.wall = corn.wall;
		record.depth = corn.depth;
		//
		corner_records.push_back(record);
	}

	// the borders
	for (const auto &bord : worldgraph->borders) {
		struct border_record record;
		record.index = bord.index;
		record.c0 = bord.c0->index;
		record.c1 = bord.c1->index;
		record.t0 = bord.t0->index;
		record.t1 = bord.t1->index;
		// world data
		record.frontier = bord.frontier;
		record.coast = bord.coast;
		record.river = bord.river;
		record.wall = bord.wall;
		//
		border_records.push_back(record);
	}

	std::ofstream stream(filepath, std::ios::binary);

	if (stream.is_open()) {
		cereal::BinaryOutputArchive archive(stream);
		archive(
			cereal::make_nvp("topology", topology), 
			cereal::make_nvp("rain", rain), 
			cereal::make_nvp("temperature", temperature),
			cereal::make_nvp("seed", atlas->seed),
			cereal::make_nvp("tilecount", tilecount),
			cereal::make_nvp("cornercount", cornercount),
			cereal::make_nvp("bordercount", bordercount),
			cereal::make_nvp("tiles", tile_records),
			cereal::make_nvp("corners", corner_records),
			cereal::make_nvp("borders", border_records)
		);
	} else {
		write_log(LogType::ERROR, "Save error: save file " + filepath + "could not be saved");
	}
}

void Saver::load(const std::string &filepath, Atlas *atlas)
{
	tile_records.clear();
	corner_records.clear();
	border_records.clear();

	tiles.clear();
	corners.clear();
	borders.clear();

	uint32_t tilecount = 0;
	uint32_t cornercount = 0;
	uint32_t bordercount = 0;

	long seed;

	std::ifstream stream(filepath, std::ios::binary);

	if (stream.is_open()) {
		cereal::BinaryInputArchive archive(stream);
		archive(
			cereal::make_nvp("topology", topology), 
			cereal::make_nvp("rain", rain), 
			cereal::make_nvp("temperature", temperature),
			cereal::make_nvp("seed", seed),
			cereal::make_nvp("tilecount", tilecount),
			cereal::make_nvp("cornercount", cornercount),
			cereal::make_nvp("bordercount", bordercount),
			cereal::make_nvp("tiles", tile_records),
			cereal::make_nvp("corners", corner_records),
			cereal::make_nvp("borders", border_records)
		);
	} else {
		write_log(LogType::ERROR, "Save error: save file " + filepath + " could not be loaded");
		return;
	}

	atlas->seed = seed;

	atlas->load_heightmap(topology.width, topology.height, topology.data);

	atlas->load_rainmap(rain.width, rain.height, rain.data);

	atlas->load_tempmap(temperature.width, temperature.height, temperature.data);

	// import the world graph data
	tiles.resize(tilecount);
	corners.resize(cornercount);
	borders.resize(bordercount);

	// the tiles
	for (const auto &record : tile_records) {
		struct tile til;
		til.index = record.index;
		til.frontier = record.frontier;
		til.land = record.land;
		til.coast = record.coast;
		til.center.x = record.center_x;
		til.center.y = record.center_y;
		for (const auto &neighbor : record.neighbors) {
			til.neighbors.push_back(&tiles[neighbor]);
		}
		for (const auto &corner : record.corners) {
			til.corners.push_back(&corners[corner]);
		}
		for (const auto &border : record.borders) {
			til.borders.push_back(&borders[border]);
		}
		//record.amp = til.amp;
		til.relief = static_cast<enum RELIEF>(record.relief);
		til.biome = static_cast<enum BIOME>(record.biome);
		til.site = static_cast<enum SITE>(record.site);
		til.hold = nullptr;
		//
		tiles[til.index] = til;
	}

	// the corners
	for (const auto &record : corner_records) {
		struct corner corn;
		corn.index = record.index;
		corn.position.x = record.position_x;
		corn.position.y = record.position_y;
		for (const auto &adj : record.adjacent) {
			corn.adjacent.push_back(&corners[adj]);
		}
		for (const auto &index : record.touches) {
			corn.touches.push_back(&tiles[index]);
		}
		// world data
		corn.frontier = record.frontier;
		corn.coast = record.coast;
		corn.river = record.river;
		corn.wall = record.wall;
		corn.depth = record.depth;
		//
		corners[corn.index] = corn;
	}

	// the borders
	for (const auto &record : border_records) {
		struct border bord;
		bord.index = record.index;
		bord.c0 = &corners[record.c0];
		bord.c1 = &corners[record.c1];
		bord.t0 = &tiles[record.t0];
		bord.t1 = &tiles[record.t1];
		// world data
		bord.frontier = record.frontier;
		bord.coast = record.coast;
		bord.river = record.river;
		bord.wall = record.wall;
		//
		borders[bord.index] = bord;
	}

	atlas->load_worldgraph(tiles, corners, borders);
}
