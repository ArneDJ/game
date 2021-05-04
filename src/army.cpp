#include <list>

#include <glm/glm.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "core/geom.h"
#include "core/entity.h"
#include "army.h"

Pathfinder::Pathfinder(const glm::vec2 &start) 
{
	location = start;
	destination = start;
	origin = start;
	state = PATH_STATE_FINISHED;
	radius = 0.f;
	velocity = glm::vec2(0.f, 0.f);
}

Pathfinder::~Pathfinder(void) 
{ 
	nodes.clear(); 
}

void Pathfinder::reset(const std::list<glm::vec2> &pathway)
{
	if (pathway.size() > 1) {
		nodes.clear();
		nodes.assign(pathway.begin(), pathway.end());
		state = PATH_STATE_NEXT;
	} else {
		state = PATH_STATE_FINISHED;
	}
}

void Pathfinder::update(float delta, float speed)
{
	if (state == PATH_STATE_NEXT) {
		std::list<glm::vec2>::iterator itr0 = nodes.begin();
		std::list<glm::vec2>::iterator itr1 = std::next(nodes.begin());
		origin = *itr0;
		destination = *itr1;
		velocity = glm::normalize(destination - origin);
		radius = glm::distance(origin, destination);
		state = PATH_STATE_MOVING;
	} else if (state == PATH_STATE_MOVING) {
		glm::vec2 offset = location + delta * speed * velocity;
		if (point_in_circle(offset, origin, radius)) {
			location = offset;
		} else {
			location = destination;
			nodes.pop_front();
			state = PATH_STATE_NEXT;
		}
	}

	if (nodes.size() < 2) {
		state = PATH_STATE_FINISHED;
	}
}

glm::vec2 Pathfinder::at(void) const { return location; }
	
glm::vec2 Pathfinder::to(void) const { return destination; }

glm::vec2 Pathfinder::velo(void) const { return velocity; }

void Pathfinder::teleport(const glm::vec2 &pos)
{
	nodes.clear();
	state = PATH_STATE_FINISHED;
	origin = pos;
	location = pos;
}

Army::Army(glm::vec2 start, float speedy) : 
	Entity(glm::vec3(start.x, 0.f, start.y), glm::quat(1.f, 0.f, 0.f, 0.f))
{
	speed = speedy;
	pathfinder = new Pathfinder { start };
	movement_mode = MOVEMENT_LAND;
	target_type = TARGET_NONE;
}

Army::~Army(void) 
{ 
	delete pathfinder; 
}

void Army::set_path(const std::list<glm::vec2> &nodes) 
{ 
	pathfinder->reset(nodes); 
}

void Army::update(float delta)
{
	pathfinder->update(delta, speed);

	glm::vec2 location = pathfinder->at();
	position.x = location.x;
	position.z = location.y;

	glm::vec2 dir = glm::normalize(pathfinder->velo());
	float angle = atan2(dir.x, dir.y);
	glm::mat4 R = glm::rotate(glm::mat4(1.f), angle, glm::vec3(0.f, 1.f, 0.f));
	rotation = glm::quat(R);
}

void Army::set_y_offset(float offset) 
{ 
	position.y = offset; 
}

void Army::teleport(const glm::vec2 &pos) 
{
	position.x = pos.x;
	position.z = pos.y;
	pathfinder->teleport(pos);
}
