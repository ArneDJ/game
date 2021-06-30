#include <iostream>
#include <list>
#include <vector>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../geometry/geom.h"

#include "mapfield.h"

bool Mapfield::triangle_in_area(glm::vec2 a, glm::vec2 b, glm::vec2 c, const geom::rectangle_t *area)
{
	// check if grid points overlap with triangle
	glm::vec2 points[4] = {
		area->min,
		{area->max.x, area->min.y},
		{area->min.x, area->max.y},
		area->max
	};
	for (int i = 0; i < 4; i++) {
		if (geom::triangle_overlaps_point(a, b, c, points[i])) {
			return true;
		}
	}
	// check if grid line segments overlap with triangle
	geom::segment_t segments[4] = {
		{points[0], points[1]},
		{points[1], points[3]},
		{points[3], points[2]},
		{points[2], points[0]}
	};
	for (int i = 0; i < 4; i++) {
		if (geom::segment_intersects_segment(a, b, segments[i].P0, segments[i].P1)) {
			return true;
		}
		if (geom::segment_intersects_segment(b, c, segments[i].P0, segments[i].P1)) {
			return true;
		}
		if (geom::segment_intersects_segment(c, a, segments[i].P0, segments[i].P1)) {
			return true;
		}
	}

	return false;
}

void Mapfield::triangle_in_regions(const struct mosaictriangle *tess, uint32_t id)
{
	glm::vec2 a = vertices[tess->a];
	glm::vec2 b = vertices[tess->b];
	glm::vec2 c = vertices[tess->c];

	// triangle bounding box
	float bbx_min_x = std::min(a.x, std::min(b.x, c.x));
	float bbx_min_y = std::min(a.y, std::min(b.y, c.y));
	float bbx_max_x = std::max(a.x, std::max(b.x, c.x));
	float bbx_max_y = std::max(a.y, std::max(b.y, c.y));

	// bounds in the grid
	int min_x = floor(bbx_min_x / regionscale);
	int min_y = floor(bbx_min_y / regionscale);
	int max_x = floor(bbx_max_x / regionscale);
	int max_y = floor(bbx_max_y / regionscale);

	int index_min = min_x + min_y * REGION_RES;
	int index_max = max_x + max_y * REGION_RES;
	if (index_min < 0 || index_min >= REGION_RES*REGION_RES || index_max < 0 || index_max >= REGION_RES*REGION_RES) {
		return;
	}

	// triangle bounding box is in a single grid region
	if (index_min == index_max) {
		regions[index_min].triangles.push_back(id);
		return;
	}

	// triangle bounding box is in a multiple grid regions
	for (int py = min_y; py <= max_y; py++) {
		for (int px = min_x; px <= max_x; px++) {
			int index = px + py * REGION_RES;
			if (index >= 0 && index < REGION_RES*REGION_RES) {
				if (triangle_in_area(a, b, c, &regions[index].area)) {
					regions[index].triangles.push_back(id);
				}
			}
		}
	}
}

void Mapfield::generate(const std::vector<glm::vec2> &vertdata, const std::vector<struct mosaictriangle> &mosaictriangles, geom::rectangle_t anchors)
{
	regions.clear();
	triangles.clear();
	vertices.clear();
	//regions = new struct mosaicregion[REGION_RES*REGION_RES];
	regions.resize(REGION_RES*REGION_RES);
	area = anchors;
	regionscale = area.max.x / REGION_RES;

	vertices.insert(vertices.end(), vertdata.begin(), vertdata.end());

	float offset = float(regionscale);
	int index_x = 0;
	for (float x = 0.f; x < area.max.x; x += offset) {
		int index_y = 0;
		for (float y = 0.f; y < area.max.y; y += offset) {
			struct mosaicregion region;
			region.area = {
				{x, y},
				{x+offset, y+offset}
			};
			regions[index_x+index_y*REGION_RES] = region;
			index_y++;
		}
		index_x++;
	}

	// copy incoming data
	triangles.resize(mosaictriangles.size());
	for (uint32_t i = 0; i < triangles.size(); i++) {
		triangles[i].index = mosaictriangles[i].index;
		triangles[i].a = mosaictriangles[i].a;
		triangles[i].b = mosaictriangles[i].b;
		triangles[i].c = mosaictriangles[i].c;
		if (geom::clockwise(vertices[triangles[i].a], vertices[triangles[i].b], vertices[triangles[i].c])) {
			triangles[i].a = mosaictriangles[i].b;
			triangles[i].b = mosaictriangles[i].a;
			triangles[i].c = mosaictriangles[i].c;
		}
		triangle_in_regions(&triangles[i], i);
	}
}

struct mapfield_result Mapfield::index_in_field(const glm::vec2 &position) const
{
	struct mapfield_result result = { false, 0 };

	int x = floor(position.x / regionscale);
	int y = floor(position.y / regionscale);

	if (x < 0 || y < 0 || x >= REGION_RES || y >= REGION_RES) {
		return result;
	}

	const struct mosaicregion *region = &regions[y * REGION_RES + x];
	for (const uint32_t index : region->triangles) {
		const struct mosaictriangle *tri = &triangles[index];
		if (geom::triangle_overlaps_point(vertices[tri->a], vertices[tri->b], vertices[tri->c], position)) {
			result.found = true;
			result.index = tri->index;
			return result;
		}
	}

	return result;
}
