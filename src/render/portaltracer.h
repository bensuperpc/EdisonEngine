#pragma once

#include "loader/datatypes.h"

namespace render
{
    struct PortalTracer
    {
        gameplay::BoundingBox boundingBox{-1, -1, 0, 1, 1, 0};
        const loader::Portal* lastPortal = nullptr;

        bool checkVisibility(const loader::Portal* portal, const gameplay::Camera& camera)
        {
            glm::vec3 camPos{ camera.getViewMatrix()[3] };
            if( glm::dot(portal->normal.toRenderSystem(), portal->vertices[0].toRenderSystem() - camPos) >= 0 )
            {
                return false; // wrong orientation (normals must face the camera)
            }

            int numBehind = 0, numTooFar = 0;
            std::pair<glm::vec3, bool> screen[4];

            gameplay::BoundingBox portalBB{0, 0, 0, 0, 0, 0};
            portalBB.min = { 1,1,0 };
            portalBB.max = { -1,-1,0 };

            for(int i = 0; i < 4; ++i)
            {
                screen[i] = projectOnScreen(portal->vertices[i].toRenderSystem(), camera, numBehind, numTooFar);
                if(!screen[i].second)
                    continue;

                portalBB.min.x = std::min(portalBB.min.x, screen[i].first.x);
                portalBB.min.y = std::min(portalBB.min.y, screen[i].first.y);
                portalBB.max.x = std::max(portalBB.max.x, screen[i].first.x);
                portalBB.max.y = std::max(portalBB.max.y, screen[i].first.y);
            }

            if(numBehind == 4 || numTooFar == 4)
                return false;

            if(numBehind == 0)
            {
                boundingBox.min.x = std::max(portalBB.min.x, boundingBox.min.x);
                boundingBox.min.y = std::max(portalBB.min.y, boundingBox.min.y);
                boundingBox.max.x = std::min(portalBB.max.x, boundingBox.max.x);
                boundingBox.max.y = std::min(portalBB.max.y, boundingBox.max.y);

                lastPortal = portal;

                return boundingBox.min.x < boundingBox.max.x && boundingBox.min.y < boundingBox.max.y;
            }

            BOOST_ASSERT(numBehind >= 1 && numBehind <= 3);

            // consider everything is visible if the camera is in the midst of a portal

            lastPortal = portal;

            return boundingBox.min.x < boundingBox.max.x && boundingBox.min.y < boundingBox.max.y;
        }

        uint16_t getLastDestinationRoom() const
        {
            return getLastPortal()->adjoining_room;
        }

        const loader::Portal* getLastPortal() const
        {
            Expects(lastPortal != nullptr);
            return lastPortal;
        }

    private:
        static std::pair<glm::vec3, bool> projectOnScreen(glm::vec3 vertex,
                                                                     const gameplay::Camera& camera,
                                                                     int& numBehind,
                                                                     int& numTooFar)
        {
            vertex = glm::vec3(camera.getViewMatrix() * glm::vec4(vertex, 1));
            if(vertex.z <= camera.getNearPlane())
                ++numBehind;
            else if(vertex.z > camera.getFarPlane())
                ++numTooFar;

            glm::vec4 tmp{vertex, 1};
            tmp = camera.getProjectionMatrix() * tmp;

            glm::vec3 screen{tmp.x / tmp.w, tmp.y / tmp.w, vertex.z};
            return{ screen, vertex.z > camera.getNearPlane() };
        }
    };
}
