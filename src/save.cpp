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

#include "geometry/geom.h"
#include "geometry/voronoi.h"
#include "util/entity.h"
#include "util/image.h"
#include "util/navigation.h"
#include "module/module.h"
#include "geography/terragen.h"
#include "geography/worldgraph.h"
#include "geography/mapfield.h"
#include "geography/atlas.h"

#include "save.h"
	
void Saver::change_directory(const std::string &dir) 
{ 
	directory = dir; 
}

void Saver::save(const std::string &filename, const geography::Atlas &atlas, const util::Navigation *landnav, const util::Navigation *seanav, const long seed)
{
	const std::string filepath = directory + filename;

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
		archive(atlas, seed, navmesh_land, navmesh_sea);
	} else {
		LOG(ERROR, "Save") << "save file " + filepath + "could not be saved";
	}
}

void Saver::load(const std::string &filename, geography::Atlas &atlas, util::Navigation *landnav, util::Navigation *seanav, long &seed)
{
	const std::string filepath = directory + filename;

	geography::Worldgraph *worldgraph = atlas.get_worldgraph();

	std::ifstream stream(filepath, std::ios::binary);

	if (stream.is_open()) {
		cereal::BinaryInputArchive archive(stream);
		archive(atlas, seed, navmesh_land, navmesh_sea);
	} else {
		LOG(ERROR, "Save") << "save file " + filepath + " could not be loaded";
		return;
	}

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
