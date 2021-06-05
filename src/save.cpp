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

#include "extern/namegen/namegen.h"

#include "extern/aixlog/aixlog.h"

#include "util/geom.h"
#include "util/entity.h"
#include "util/image.h"
#include "util/voronoi.h"
#include "util/navigation.h"
#include "geography/terragen.h"
#include "geography/worldgraph.h"
#include "geography/mapfield.h"
#include "geography/atlas.h"

#include "save.h"
	
void Saver::change_directory(const std::string &dir) 
{ 
	directory = dir; 
}

void Saver::save(const std::string &filename, const Atlas *atlas, const UTIL::Navigation *landnav, const UTIL::Navigation *seanav, const long seed)
{
	const std::string filepath = directory + filename;

	const auto heightmap = atlas->get_heightmap();
	topology.width = heightmap->width;
	topology.height = heightmap->height;
	topology.channels = heightmap->channels;
	topology.size = heightmap->size;
	topology.data.resize(heightmap->size);
	std::copy(heightmap->data, heightmap->data + heightmap->size, topology.data.begin());

	const auto worldgraph = atlas->get_worldgraph();

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
			cereal::make_nvp("rain", atlas->terragen->rainmap), 
			cereal::make_nvp("temperature", atlas->terragen->tempmap),
			cereal::make_nvp("watermap", atlas->watermap),
			cereal::make_nvp("seed", seed),
			cereal::make_nvp("tiles", worldgraph->tiles),
			cereal::make_nvp("corners", worldgraph->corners),
			cereal::make_nvp("borders", worldgraph->borders),
			cereal::make_nvp("landnav", navmesh_land),
			cereal::make_nvp("seanav", navmesh_sea),
			cereal::make_nvp("holdings", atlas->holdings)
		);
	} else {
		LOG(ERROR, "Save") << "save file " + filepath + "could not be saved";
	}
}

void Saver::load(const std::string &filename, Atlas *atlas, UTIL::Navigation *landnav, UTIL::Navigation *seanav, long &seed)
{
	const std::string filepath = directory + filename;

	Worldgraph *worldgraph = atlas->worldgraph.get();

	std::ifstream stream(filepath, std::ios::binary);

	if (stream.is_open()) {
		cereal::BinaryInputArchive archive(stream);
		archive(
			cereal::make_nvp("topology", topology), 
			cereal::make_nvp("rain", atlas->terragen->rainmap), 
			cereal::make_nvp("temperature", atlas->terragen->tempmap),
			cereal::make_nvp("watermap", atlas->watermap),
			cereal::make_nvp("seed", seed),
			cereal::make_nvp("tiles", worldgraph->tiles),
			cereal::make_nvp("corners", worldgraph->corners),
			cereal::make_nvp("borders", worldgraph->borders),
			cereal::make_nvp("landnav", navmesh_land),
			cereal::make_nvp("seanav", navmesh_sea),
			cereal::make_nvp("holdings", atlas->holdings)
		);
	} else {
		LOG(ERROR, "Save") << "save file " + filepath + " could not be loaded";
		return;
	}

	atlas->load_heightmap(topology.width, topology.height, topology.data);

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
