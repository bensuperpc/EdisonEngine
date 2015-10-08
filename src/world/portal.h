#pragma once

#include "util/vmath.h"

#include <memory>
#include <vector>

namespace world
{
struct Room;

/*
 * пока геометрия текущего портала и портала назначения совпадают.
 * далее будет проведена привязка камеры к взаимоориентации порталов
 */
struct Portal
{
    std::vector<glm::vec3> vertices;                                                           // Оригинальные вершины портала
    util::Plane normal;                                                           // уравнение плоскости оригинальных вершин (оно же нормаль)
    glm::vec3 centre = { 0,0,0 };                                                         // центр портала
    std::shared_ptr<Room> dest_room = nullptr;                                                   // куда ведет портал
    std::shared_ptr<Room> current_room;                                                // комната, где нааходится портал

    explicit Portal(size_t vcount = 0)
        : vertices(vcount)
    {
    }

    ~Portal() = default;

    void move(const glm::vec3 &mv);
    bool rayIntersect(const glm::vec3 &dir, const glm::vec3 &point);              // check the intersection of the beam and portal

    void updateNormal();
};

} // namespace world
