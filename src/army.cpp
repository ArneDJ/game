#include <list>
#include <memory>

#include <glm/glm.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "geometry/geom.h"
#include "util/entity.h"
#include "army.h"

Pathfinder::Pathfinder(const glm::vec2 &start) 
{
	location = start;
	destination = start;
	origin = start;
	m_state = pathfind_state::FINISHED;
	radius = 0.f;
	velocity = glm::vec2(0.f, 0.f);
}

void Pathfinder::reset(const std::list<glm::vec2> &pathway)
{
	if (pathway.size() > 1) {
		nodes.clear();
		nodes.assign(pathway.begin(), pathway.end());
		m_state = pathfind_state::NEXT;
	} else {
		m_state = pathfind_state::FINISHED;
	}
}

void Pathfinder::update(float delta, float speed)
{
	if (m_state == pathfind_state::NEXT) {
		std::list<glm::vec2>::iterator itr0 = nodes.begin();
		std::list<glm::vec2>::iterator itr1 = std::next(nodes.begin());
		origin = *itr0;
		destination = *itr1;
		velocity = glm::normalize(destination - origin);
		radius = glm::distance(origin, destination);
		m_state = pathfind_state::MOVING;
	} else if (m_state == pathfind_state::MOVING) {
		glm::vec2 offset = location + delta * speed * velocity;
		if (geom::point_in_circle(offset, origin, radius)) {
			location = offset;
		} else {
			location = destination;
			nodes.pop_front(); m_state = pathfind_state::NEXT;
		}
	}

	if (nodes.size() < 2) {
		m_state = pathfind_state::FINISHED;
	}
}

glm::vec2 Pathfinder::at() const { return location; }
	
glm::vec2 Pathfinder::to() const { return destination; }

glm::vec2 Pathfinder::velo() const { return velocity; }
	
enum pathfind_state Pathfinder::state() const { return m_state; }

void Pathfinder::teleport(const glm::vec2 &pos)
{
	nodes.clear();
	m_state = pathfind_state::FINISHED;
	origin = pos;
	location = pos;
}

ArmyNode::ArmyNode(glm::vec2 start, float speedy)
{
	position = { start.x, 0.f, start.y };
	glm::mat4 R = glm::rotate(glm::mat4(1.f), 1.57f, glm::vec3(0.f, 1.f, 0.f));
	rotation = glm::quat(R);

	speed = speedy;
	pathfinder = std::make_unique<Pathfinder>(start);
	movement_mode = MOVEMENT_LAND;
	target_type = TARGET_NONE;
}

void ArmyNode::set_path(const std::list<glm::vec2> &nodes) 
{ 
	pathfinder->reset(nodes); 
}

void ArmyNode::update(float delta)
{
	pathfinder->update(delta, speed);

	glm::vec2 location = pathfinder->at();
	position.x = location.x;
	position.z = location.y;

	// update rotation
	if (pathfinder->state() == pathfind_state::MOVING) {
		glm::vec2 dir = glm::normalize(pathfinder->velo());
		float angle = atan2(dir.x, dir.y);
		glm::mat4 R = glm::rotate(glm::mat4(1.f), angle, glm::vec3(0.f, 1.f, 0.f));
		rotation = glm::quat(R);
	}
}

void ArmyNode::set_y_offset(float offset) 
{ 
	position.y = offset; 
}

void ArmyNode::teleport(const glm::vec2 &pos) 
{
	position.x = pos.x;
	position.z = pos.y;
	pathfinder->teleport(pos);
}
