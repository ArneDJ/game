#include <iostream>
#include <fstream>
#include <vector>
#include <random>
#include <algorithm>
#include <list>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "extern/cereal/types/unordered_map.hpp"
#include "extern/cereal/types/vector.hpp"
#include "extern/cereal/types/memory.hpp"
#include "extern/cereal/archives/binary.hpp"

#include "extern/recast/Recast.h"
#include "extern/recast/DetourNavMesh.h"
#include "extern/recast/DetourNavMeshQuery.h"
#include "extern/recast/ChunkyTriMesh.h"

#include "core/logger.h"
#include "core/geom.h"
#include "core/entity.h"
#include "core/image.h"
#include "core/voronoi.h"
#include "core/navigation.h"
#include "terragen.h"
#include "worldgraph.h"
#include "mapfield.h"
#include "atlas.h"
#include "save.h"
	
void Saver::change_directory(const std::string &dir) 
{ 
	directory = dir; 
}

void Saver::save(const std::string &filename, const Atlas *atlas, const Navigation *landnav, const Navigation *seanav, const long seed)
{
	const std::string filepath = directory + filename;

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

	const Image *waterimage = atlas->get_watermap();
	watermap.width = waterimage->width;
	watermap.height = waterimage->height;
	watermap.channels = waterimage->channels;
	watermap.size = waterimage->size;
	watermap.data.resize(waterimage->size);

	std::copy(waterimage->data, waterimage->data + waterimage->size, watermap.data.begin());

	const Worldgraph *worldgraph = atlas->worldgraph;

	// save the campaign navigation data
	{
		navmesh_land.tilemeshes.clear();

		const dtNavMesh *navmesh = landnav->get_navmesh();
		if (navmesh) {
			const dtNavMeshParams *navparams = navmesh->getParams();
			navmesh_land.origin = { navparams->orig[0], navparams->orig[1], navparams->orig[2] };
			navmesh_land.tilewidth = navparams->tileWidth;
			navmesh_land.tileheight = navparams->tileHeight;
			navmesh_land.maxtiles = navparams->maxTiles;
			navmesh_land.maxpolys = navparams->maxPolys;
			for (int i = 0; i < navmesh->getMaxTiles(); i++) {
				const dtMeshTile *tile = navmesh->getTile(i);
				if (!tile) { continue; }
				if (!tile->header) { continue; }
				const dtMeshHeader *tileheader = tile->header;
				struct nav_tilemesh_record navrecord;
				navrecord.x = tileheader->x;
				navrecord.y = tileheader->y;
				navrecord.data.insert(navrecord.data.end(), tile->data, tile->data  + tile->dataSize);
				navmesh_land.tilemeshes.push_back(navrecord);
			}
		}
	}

	{
		navmesh_sea.tilemeshes.clear();

		const dtNavMesh *navmesh = seanav->get_navmesh();
		if (navmesh) {
			const dtNavMeshParams *navparams = navmesh->getParams();
			navmesh_sea.origin = { navparams->orig[0], navparams->orig[1], navparams->orig[2] };
			navmesh_sea.tilewidth = navparams->tileWidth;
			navmesh_sea.tileheight = navparams->tileHeight;
			navmesh_sea.maxtiles = navparams->maxTiles;
			navmesh_sea.maxpolys = navparams->maxPolys;
			for (int i = 0; i < navmesh->getMaxTiles(); i++) {
				const dtMeshTile *tile = navmesh->getTile(i);
				if (!tile) { continue; }
				if (!tile->header) { continue; }
				const dtMeshHeader *tileheader = tile->header;
				struct nav_tilemesh_record navrecord;
				navrecord.x = tileheader->x;
				navrecord.y = tileheader->y;
				navrecord.data.insert(navrecord.data.end(), tile->data, tile->data  + tile->dataSize);
				navmesh_sea.tilemeshes.push_back(navrecord);
			}
		}
	}

	std::ofstream stream(filepath, std::ios::binary);

	if (stream.is_open()) {
		cereal::BinaryOutputArchive archive(stream);
		archive(
			cereal::make_nvp("topology", topology), 
			cereal::make_nvp("rain", rain), 
			cereal::make_nvp("temperature", temperature),
			cereal::make_nvp("watermap", watermap),
			cereal::make_nvp("seed", seed),
			cereal::make_nvp("tiles", worldgraph->tiles),
			cereal::make_nvp("corners", worldgraph->corners),
			cereal::make_nvp("borders", worldgraph->borders),
			cereal::make_nvp("landnav", navmesh_land),
			cereal::make_nvp("seanav", navmesh_sea)
		);
	} else {
		write_log(LogType::ERROR, "Save error: save file " + filepath + "could not be saved");
	}
}

void Saver::load(const std::string &filename, Atlas *atlas, Navigation *landnav, Navigation *seanav, long &seed)
{
	const std::string filepath = directory + filename;

	Worldgraph *worldgraph = atlas->worldgraph;

	std::ifstream stream(filepath, std::ios::binary);

	if (stream.is_open()) {
		cereal::BinaryInputArchive archive(stream);
		archive(
			cereal::make_nvp("topology", topology), 
			cereal::make_nvp("rain", rain), 
			cereal::make_nvp("temperature", temperature),
			cereal::make_nvp("watermap", watermap),
			cereal::make_nvp("seed", seed),
			cereal::make_nvp("tiles", worldgraph->tiles),
			cereal::make_nvp("corners", worldgraph->corners),
			cereal::make_nvp("borders", worldgraph->borders),
			cereal::make_nvp("landnav", navmesh_land),
			cereal::make_nvp("seanav", navmesh_sea)
		);
	} else {
		write_log(LogType::ERROR, "Save error: save file " + filepath + " could not be loaded");
		return;
	}

	atlas->load_heightmap(topology.width, topology.height, topology.data);

	atlas->load_rainmap(rain.width, rain.height, rain.data);

	atlas->load_tempmap(temperature.width, temperature.height, temperature.data);

	atlas->load_watermap(watermap.width, watermap.height, watermap.data);
	
	// load campaign the navigation data
	landnav->alloc(navmesh_land.origin, navmesh_land.tilewidth, navmesh_land.tileheight, navmesh_land.maxtiles, navmesh_land.maxpolys);
	for (const auto &tilemesh : navmesh_land.tilemeshes) {
		landnav->load_tilemesh(tilemesh.x, tilemesh.y, tilemesh.data);
	}
	seanav->alloc(navmesh_sea.origin, navmesh_sea.tilewidth, navmesh_sea.tileheight, navmesh_sea.maxtiles, navmesh_sea.maxpolys);
	for (const auto &tilemesh : navmesh_sea.tilemeshes) {
		seanav->load_tilemesh(tilemesh.x, tilemesh.y, tilemesh.data);
	}

	worldgraph->reload_references();
}
