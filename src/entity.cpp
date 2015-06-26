#include <cstdlib>
#include <cmath>

#include "vmath.h"
#include "mesh.h"
#include "entity.h"
#include "render.h"
#include "camera.h"
#include "world.h"
#include "engine.h"
#include "console.h"
#include "script.h"
#include "gui.h"
#include "anim_state_control.h"
#include "character_controller.h"
#include "obb.h"
#include "gameflow.h"
#include "string.h"
#include "ragdoll.h"

#include <bullet/btBulletCollisionCommon.h>
#include <bullet/btBulletDynamicsCommon.h>
#include <bullet/BulletCollision/CollisionDispatch/btCollisionObject.h>
#include <bullet/BulletCollision/CollisionDispatch/btGhostObject.h>


void Entity::createGhosts()
{
    if(bf.animations.model->mesh_count <= 0)
        return;

    bt.manifoldArray = new btManifoldArray();
    bt.shapes = (btCollisionShape**)malloc(bf.bone_tag_count * sizeof(btCollisionShape*));
    bt.ghostObjects = (btPairCachingGhostObject**)malloc(bf.bone_tag_count * sizeof(btPairCachingGhostObject*));
    bt.last_collisions = (entity_collision_node_p)malloc(bf.bone_tag_count * sizeof(entity_collision_node_t));
    for(uint16_t i=0;i<bf.bone_tag_count;i++)
    {
        btVector3 box;
        box[0] = 0.40 * (bf.bone_tags[i].mesh_base->bb_max[0] - bf.bone_tags[i].mesh_base->bb_min[0]);
        box[1] = 0.40 * (bf.bone_tags[i].mesh_base->bb_max[1] - bf.bone_tags[i].mesh_base->bb_min[1]);
        box[2] = 0.40 * (bf.bone_tags[i].mesh_base->bb_max[2] - bf.bone_tags[i].mesh_base->bb_min[2]);
        bt.shapes[i] = new btBoxShape(box);
        bf.bone_tags[i].mesh_base->R = (box[0] < box[1])?(box[0]):(box[1]);
        bf.bone_tags[i].mesh_base->R = (bf.bone_tags[i].mesh_base->R < box[2])?(bf.bone_tags[i].mesh_base->R):(box[2]);

        bt.ghostObjects[i] = new btPairCachingGhostObject();
        // FIXME bt.ghostObjects[i]->setIgnoreCollisionCheck(bt.bt_body[i], true);

        btTransform gltr = transform * bf.bone_tags[i].full_transform;
        gltr.setOrigin( gltr * bf.bone_tags[i].mesh_base->centre );

        bt.ghostObjects[i]->setWorldTransform(gltr);
        bt.ghostObjects[i]->setCollisionFlags(bt.ghostObjects[i]->getCollisionFlags() | btCollisionObject::CF_CHARACTER_OBJECT);
        bt.ghostObjects[i]->setUserPointer(self);
        bt.ghostObjects[i]->setCollisionShape(bt.shapes[i]);
        bt_engine_dynamicsWorld->addCollisionObject(bt.ghostObjects[i], COLLISION_GROUP_CHARACTERS, COLLISION_GROUP_ALL);

        bt.last_collisions[i].obj_count = 0;
    }
}


void Entity_Enable(std::shared_ptr<Entity> ent)
{
    if(!(ent->state_flags & ENTITY_STATE_ENABLED))
    {
        if(ent->bt.bt_body != NULL)
        {
            for(uint16_t i=0;i<ent->bf.bone_tag_count;i++)
            {
                btRigidBody *b = ent->bt.bt_body[i];
                if((b != NULL) && !b->isInWorld())
                {
                    bt_engine_dynamicsWorld->addRigidBody(b);
                }
            }
        }
        ent->state_flags |= ENTITY_STATE_ENABLED | ENTITY_STATE_ACTIVE | ENTITY_STATE_VISIBLE;
    }
}


void Entity_Disable(std::shared_ptr<Entity> ent)
{
    if(ent->state_flags & ENTITY_STATE_ENABLED)
    {
        if(ent->bt.bt_body != NULL)
        {
            for(uint16_t i=0;i<ent->bf.bone_tag_count;i++)
            {
                btRigidBody *b = ent->bt.bt_body[i];
                if((b != NULL) && b->isInWorld())
                {
                    bt_engine_dynamicsWorld->removeRigidBody(b);
                }
            }
        }
        ent->state_flags = 0x0000;
    }
}

/**
 * This function enables collision for entity_p in all cases exept NULL models.
 * If collision models does not exists, function will create them;
 * @param ent - pointer to the entity.
 */
void Entity_EnableCollision(std::shared_ptr<Entity> ent)
{
    if(ent->bt.bt_body != NULL)
    {
        ent->self->collide_flag = 0x01;
        for(uint16_t i=0;i<ent->bf.bone_tag_count;i++)
        {
            btRigidBody *b = ent->bt.bt_body[i];
            if((b != NULL) && !b->isInWorld())
            {
                bt_engine_dynamicsWorld->addRigidBody(b);
            }
        }
    }
    else
    {
        ent->self->collide_flag = COLLISION_TRIMESH;                            ///@TODO: order collision shape and entity collision type flags! it is a different things!
        BT_GenEntityRigidBody(ent);
    }
}


void Entity_DisableCollision(std::shared_ptr<Entity> ent)
{
    if(ent->bt.bt_body != NULL)
    {
        ent->self->collide_flag = 0x00;
        for(uint16_t i=0;i<ent->bf.bone_tag_count;i++)
        {
            btRigidBody *b = ent->bt.bt_body[i];
            if((b != NULL) && b->isInWorld())
            {
                bt_engine_dynamicsWorld->removeRigidBody(b);
            }
        }
    }
}


void BT_GenEntityRigidBody(std::shared_ptr<Entity> ent)
{
    btVector3 localInertia(0, 0, 0);
    btTransform startTransform;

    if(ent->bf.animations.model == NULL)
    {
        return;
    }

    ent->bt.bt_body = (btRigidBody**)malloc(ent->bf.bone_tag_count * sizeof(btRigidBody*));

    for(uint16_t i=0;i<ent->bf.bone_tag_count;i++)
    {
        base_mesh_p mesh = ent->bf.animations.model->mesh_tree[i].mesh_base;
        btCollisionShape *cshape = BT_CSfromMesh(mesh, true, true, false);
        ent->bt.bt_body[i] = NULL;

        if(cshape)
        {
            cshape->calculateLocalInertia(0.0, localInertia);

            auto tr = ent->transform * ent->bf.bone_tags[i].full_transform;
            startTransform = tr;
            btDefaultMotionState* motionState = new btDefaultMotionState(startTransform);
            ent->bt.bt_body[i] = new btRigidBody(0.0, motionState, cshape, localInertia);
            //ent->bt.bt_body[i]->setCollisionFlags(ent->bt.bt_body[i]->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
            bt_engine_dynamicsWorld->addRigidBody(ent->bt.bt_body[i], COLLISION_GROUP_KINEMATIC, COLLISION_MASK_ALL);
            ent->bt.bt_body[i]->setUserPointer(ent->self);
            ent->bt.bt_body[i]->setUserIndex(i);
        }
    }
}


/**
 * It is from bullet_character_controller
 */
int Ghost_GetPenetrationFixVector(btPairCachingGhostObject *ghost, btManifoldArray *manifoldArray, btVector3* correction)
{
    // Here we must refresh the overlapping paircache as the penetrating movement itself or the
    // previous recovery iteration might have used setWorldTransform and pushed us into an object
    // that is not in the previous cache contents from the last timestep, as will happen if we
    // are pushed into a new AABB overlap. Unhandled this means the next convex sweep gets stuck.
    //
    // Do this by calling the broadphase's setAabb with the moved AABB, this will update the broadphase
    // paircache and the ghostobject's internal paircache at the same time.    /BW

    int ret = 0;
    int num_pairs, manifolds_size;
    btBroadphasePairArray &pairArray = ghost->getOverlappingPairCache()->getOverlappingPairArray();
    btVector3 aabb_min, aabb_max, t;

    ghost->getCollisionShape()->getAabb(ghost->getWorldTransform(), aabb_min, aabb_max);
    bt_engine_dynamicsWorld->getBroadphase()->setAabb(ghost->getBroadphaseHandle(), aabb_min, aabb_max, bt_engine_dynamicsWorld->getDispatcher());
    bt_engine_dynamicsWorld->getDispatcher()->dispatchAllCollisionPairs(ghost->getOverlappingPairCache(), bt_engine_dynamicsWorld->getDispatchInfo(), bt_engine_dynamicsWorld->getDispatcher());

    correction->setZero();
    num_pairs = ghost->getOverlappingPairCache()->getNumOverlappingPairs();
    for(int i=0;i<num_pairs;i++)
    {
        manifoldArray->clear();
        // do not use commented code: it prevents to collision skips.
        //btBroadphasePair &pair = pairArray[i];
        //btBroadphasePair* collisionPair = bt_engine_dynamicsWorld->getPairCache()->findPair(pair.m_pProxy0,pair.m_pProxy1);
        btBroadphasePair *collisionPair = &pairArray[i];

        if(!collisionPair)
        {
            continue;
        }

        if(collisionPair->m_algorithm)
        {
            collisionPair->m_algorithm->getAllContactManifolds(*manifoldArray);
        }

        manifolds_size = manifoldArray->size();
        for(int j=0;j<manifolds_size;j++)
        {
            btPersistentManifold* manifold = (*manifoldArray)[j];
            btScalar directionSign = manifold->getBody0() == ghost ? btScalar(-1.0) : btScalar(1.0);
            for(int k=0;k<manifold->getNumContacts();k++)
            {
                const btManifoldPoint&pt = manifold->getContactPoint(k);
                btScalar dist = pt.getDistance();

                if(dist < 0.0)
                {
                    t = pt.m_normalWorldOnB * dist * directionSign;
                    *correction += t;
                    ret++;
                }
            }
        }
    }

    return ret;
}


void Entity_GhostUpdate(std::shared_ptr<Entity> ent)
{
    if(ent->bt.ghostObjects != NULL)
    {
        if(ent->type_flags & ENTITY_TYPE_DYNAMIC)
        {
            for(uint16_t i=0;i<ent->bf.bone_tag_count;i++)
            {
                auto tr = ent->transform * ent->bf.bone_tags[i].full_transform;
                auto v = ent->bf.animations.model->mesh_tree[i].mesh_base->centre;
                ent->bt.ghostObjects[i]->getWorldTransform() = tr;
                auto pos = tr * v;
                ent->bt.ghostObjects[i]->getWorldTransform().setOrigin(pos);
            }
        }
        else
        {
            for(uint16_t i=0;i<ent->bf.bone_tag_count;i++)
            {
                auto tr = ent->bt.bt_body[i]->getWorldTransform();
                tr.setOrigin(tr * ent->bf.bone_tags[i].mesh_base->centre);
                ent->bt.ghostObjects[i]->getWorldTransform() = tr;
            }
        }
    }
}


void Entity_UpdateCurrentCollisions(std::shared_ptr<Entity> ent)
{
    if(ent->bt.ghostObjects != NULL)
    {
        for(uint16_t i=0;i<ent->bf.bone_tag_count;i++)
        {
            btPairCachingGhostObject *ghost = ent->bt.ghostObjects[i];
            entity_collision_node_p cn = ent->bt.last_collisions + i;

            cn->obj_count = 0;
            auto tr = ent->transform * ent->bf.bone_tags[i].full_transform;
            auto v = ent->bf.animations.model->mesh_tree[i].mesh_base->centre;
            auto orig_tr = ghost->getWorldTransform();
            ghost->getWorldTransform() = tr;
            auto pos = tr * v;
            ghost->getWorldTransform().setOrigin(pos);

            btBroadphasePairArray &pairArray = ghost->getOverlappingPairCache()->getOverlappingPairArray();
            btVector3 aabb_min, aabb_max;

            ghost->getCollisionShape()->getAabb(ghost->getWorldTransform(), aabb_min, aabb_max);
            bt_engine_dynamicsWorld->getBroadphase()->setAabb(ghost->getBroadphaseHandle(), aabb_min, aabb_max, bt_engine_dynamicsWorld->getDispatcher());
            bt_engine_dynamicsWorld->getDispatcher()->dispatchAllCollisionPairs(ghost->getOverlappingPairCache(), bt_engine_dynamicsWorld->getDispatchInfo(), bt_engine_dynamicsWorld->getDispatcher());

            int num_pairs = ghost->getOverlappingPairCache()->getNumOverlappingPairs();
            for(int j=0;j<num_pairs;j++)
            {
                ent->bt.manifoldArray->clear();
                btBroadphasePair *collisionPair = &pairArray[j];

                if(!collisionPair)
                {
                    continue;
                }

                if(collisionPair->m_algorithm)
                {
                    collisionPair->m_algorithm->getAllContactManifolds(*ent->bt.manifoldArray);
                }

                for(int k=0;k<ent->bt.manifoldArray->size();k++)
                {
                    if(cn->obj_count < MAX_OBJECTS_IN_COLLSION_NODE - 1)
                    {
                        btPersistentManifold* manifold = (*ent->bt.manifoldArray)[k];
                        for(int c=0;c<manifold->getNumContacts();c++)               // c++ in C++
                        {
                            //const btManifoldPoint &pt = manifold->getContactPoint(c);
                            if(manifold->getContactPoint(c).getDistance() < 0.0)
                            {
                                cn->obj[cn->obj_count] = (btCollisionObject*)(*ent->bt.manifoldArray)[k]->getBody0();
                                if(ent->self == (engine_container_p)(cn->obj[cn->obj_count]->getUserPointer()))
                                {
                                    cn->obj[cn->obj_count] = (btCollisionObject*)(*ent->bt.manifoldArray)[k]->getBody1();
                                }
                                cn->obj_count++;                                    // do it once in current cycle, so condition (cn->obj_count < MAX_OBJECTS_IN_COLLSION_NODE - 1) located in correct place
                                break;
                            }
                        }
                    }
                }
            }
            ghost->setWorldTransform(orig_tr);
        }
    }
}


///@TODO: make experiment with convexSweepTest with spheres: no more iterative cycles;
int Entity_GetPenetrationFixVector(std::shared_ptr<Entity> ent, btVector3* reaction, bool hasMove)
{
    int ret = 0;

    reaction->setZero();
    if((ent->bt.ghostObjects != NULL) && (ent->bt.no_fix_all == 0))
    {
        auto orig_pos = ent->transform.getOrigin();
        for(uint16_t i=0;i<ent->bf.animations.model->collision_map_size;i++)
        {
            uint16_t m = ent->bf.animations.model->collision_map[i];
            ss_bone_tag_p btag = ent->bf.bone_tags + m;

            if(btag->body_part & ent->bt.no_fix_body_parts)
            {
                continue;
            }

            // antitunneling condition for main body parts, needs only in move case: ((move != NULL) && (btag->body_part & (BODY_PART_BODY_LOW | BODY_PART_BODY_UPPER)))
            btVector3 from;
            if((btag->parent == NULL) || (hasMove && (btag->body_part & (BODY_PART_BODY_LOW | BODY_PART_BODY_UPPER))))
            {
                from = ent->bt.ghostObjects[m]->getWorldTransform().getOrigin();
                from += ent->transform.getOrigin() - orig_pos;
            }
            else
            {
                auto parent_from = btag->parent->full_transform * btag->parent->mesh_base->centre;
                from = ent->transform * parent_from;
            }

            auto tr = ent->transform * btag->full_transform;
            auto to = tr * btag->mesh_base->centre;
            auto curr = from;
            auto move = to - from;
            auto move_len = move.length();
            if((i == 0) && (move_len > 1024.0))                                 ///@FIXME: magick const 1024.0!
            {
                break;
            }
            int iter = (btScalar)(4.0 * move_len / btag->mesh_base->R) + 1;     ///@FIXME (not a critical): magick const 4.0!
            move[0] /= (btScalar)iter;
            move[1] /= (btScalar)iter;
            move[2] /= (btScalar)iter;

            for(int j=0;j<=iter;j++)
            {
                tr.setOrigin(curr);
                auto tr_current = tr;
                ent->bt.ghostObjects[m]->setWorldTransform(tr_current);
                btVector3 tmp;
                if(Ghost_GetPenetrationFixVector(ent->bt.ghostObjects[m], ent->bt.manifoldArray, &tmp))
                {
                    ent->transform.getOrigin() += tmp;
                    curr += tmp;
                    from += tmp;
                    ret++;
                }
                curr += move;
            }
        }
        *reaction = ent->transform.getOrigin() - orig_pos;
        ent->transform.setOrigin(orig_pos);
    }

    return ret;
}


void Entity_FixPenetrations(std::shared_ptr<Entity> ent, btVector3* move)
{
    if(ent->bt.ghostObjects != NULL)
    {
        if((move != NULL) && (ent->character != NULL))
        {
            ent->character->resp.horizontal_collide    = 0x00;
            ent->character->resp.vertical_collide      = 0x00;
        }

        if(ent->type_flags & ENTITY_TYPE_DYNAMIC)
        {
            return;
        }

        if(ent->bt.no_fix_all)
        {
            Entity_GhostUpdate(ent);
            return;
        }

        btVector3 reaction;
        int numPenetrationLoops = Entity_GetPenetrationFixVector(ent, &reaction, move!=nullptr);
        ent->transform.getOrigin() += reaction;

        if(ent->character != NULL)
        {
            Character_UpdateCurrentHeight(ent);
            if((move != NULL) && (numPenetrationLoops > 0))
            {
                btScalar t1 = reaction[0] * reaction[0] + reaction[1] * reaction[1];
                btScalar t2 = move->x() * move->x() + move->y() * move->y();
                if((reaction[2] * reaction[2] < t1) && (move->z() * move->z() < t2))    // we have horizontal move and horizontal correction
                {
                    t2 *= t1;
                    t1 = (reaction[0] * move->x() + reaction[1] * move->y()) / sqrtf(t2);
                    if(t1 < ent->character->critical_wall_component)
                    {
                        ent->character->resp.horizontal_collide |= 0x01;
                    }
                }
                else if((reaction[2] * reaction[2] > t1) && (move->z() * move->z() > t2))
                {
                    if((reaction[2] > 0.0) && (move->z() < 0.0))
                    {
                        ent->character->resp.vertical_collide |= 0x01;
                    }
                    else if((reaction[2] < 0.0) && (move->z() > 0.0))
                    {
                        ent->character->resp.vertical_collide |= 0x02;
                    }
                }
            }

            if(ent->character->height_info.ceiling_hit && (reaction[2] < -0.1))
            {
                ent->character->resp.vertical_collide |= 0x02;
            }

            if(ent->character->height_info.floor_hit && (reaction[2] > 0.1))
            {
                ent->character->resp.vertical_collide |= 0x01;
            }
        }

        Entity_GhostUpdate(ent);
    }
}

/**
 * we check walls and other collision objects reaction. if reaction more then critacal
 * then cmd->horizontal_collide |= 0x01;
 * @param ent - cheked entity
 * @param cmd - here we fill cmd->horizontal_collide field
 * @param move - absolute 3d move vector
 */
int Entity_CheckNextPenetration(std::shared_ptr<Entity> ent, const btVector3& move)
{
    int ret = 0;
    if(ent->bt.ghostObjects != NULL)
    {
        character_response_p resp = &ent->character->resp;

        Entity_GhostUpdate(ent);
        ent->transform.getOrigin() += move;
        //resp->horizontal_collide = 0x00;
        btVector3 reaction;
        ret = Entity_GetPenetrationFixVector(ent, &reaction, true);
        if((ret > 0) && (ent->character != NULL))
        {
            btScalar t1 = reaction[0] * reaction[0] + reaction[1] * reaction[1];
            btScalar t2 = move[0] * move[0] + move[1] * move[1];
            if((reaction[2] * reaction[2] < t1) && (move[2] * move[2] < t2))
            {
                t2 *= t1;
                t1 = (reaction[0] * move[0] + reaction[1] * move[1]) / sqrtf(t2);
                if(t1 < ent->character->critical_wall_component)
                {
                    resp->horizontal_collide |= 0x01;
                }
            }
        }
        ent->transform.getOrigin() -= move;
        Entity_GhostUpdate(ent);
        Entity_CleanCollisionAllBodyParts(ent);
    }

    return ret;
}


void Entity_CheckCollisionCallbacks(std::shared_ptr<Entity> ent)
{
    if(ent->bt.ghostObjects != NULL)
    {
        btCollisionObject *cobj;
        uint32_t curr_flag;
        Entity_UpdateCurrentCollisions(ent);
        while((cobj = Entity_GetRemoveCollisionBodyParts(ent, 0xFFFFFFFF, &curr_flag)) != NULL)
        {
            // do callbacks here:
            int type = -1;
            engine_container_p cont = (engine_container_p)cobj->getUserPointer();
            if(cont != NULL)
            {
                type = cont->object_type;
            }

            if(type == OBJECT_ENTITY)
            {
                std::shared_ptr<Entity> activator = std::static_pointer_cast<Entity>(cont->object);
                
                if(activator->callback_flags & ENTITY_CALLBACK_COLLISION)
                {
                    // Activator and entity IDs are swapped in case of collision callback.
                    lua_ExecEntity(engine_lua, ENTITY_CALLBACK_COLLISION, activator->id, ent->id);
                    Con_Printf("char_body_flag = 0x%X, collider_bone_index = %d, collider_type = %d", curr_flag, cobj->getUserIndex(), type);
                }
            }
        }
    }
}


bool Entity_WasCollisionBodyParts(std::shared_ptr<Entity> ent, uint32_t parts_flags)
{
    if(ent->bt.last_collisions != NULL)
    {
        for(uint16_t i=0;i<ent->bf.bone_tag_count;i++)
        {
            if((ent->bf.bone_tags[i].body_part & parts_flags) && (ent->bt.last_collisions[i].obj_count > 0))
            {
                return true;
            }
        }
    }

    return false;
}


void Entity_CleanCollisionAllBodyParts(std::shared_ptr<Entity> ent)
{
    if(ent->bt.last_collisions != NULL)
    {
        for(uint16_t i=0;i<ent->bf.bone_tag_count;i++)
        {
            ent->bt.last_collisions[i].obj_count = 0;
        }
    }
}


void Entity_CleanCollisionBodyParts(std::shared_ptr<Entity> ent, uint32_t parts_flags)
{
    if(ent->bt.last_collisions != NULL)
    {
        for(uint16_t i=0;i<ent->bf.bone_tag_count;i++)
        {
            if(ent->bf.bone_tags[i].body_part & parts_flags)
            {
                ent->bt.last_collisions[i].obj_count = 0;
            }
        }
    }
}


btCollisionObject *Entity_GetRemoveCollisionBodyParts(std::shared_ptr<Entity> ent, uint32_t parts_flags, uint32_t *curr_flag)
{
    *curr_flag = 0x00;
    if(ent->bt.last_collisions != NULL)
    {
        for(uint16_t i=0;i<ent->bf.bone_tag_count;i++)
        {
            if(ent->bf.bone_tags[i].body_part & parts_flags)
            {
                entity_collision_node_p cn = ent->bt.last_collisions + i;
                if(cn->obj_count > 0)
                {
                    *curr_flag = ent->bf.bone_tags[i].body_part;
                    return cn->obj[--cn->obj_count];
                }
            }
        }
    }

    return NULL;
}


void Entity_UpdateRoomPos(std::shared_ptr<Entity> ent)
{
    room_sector_p new_sector;

    auto v = (ent->bf.bb_min + ent->bf.bb_max)/2;
    auto pos = ent->transform * v;
    std::shared_ptr<Room> new_room = Room_FindPosCogerrence(pos, ent->self->room);
    if(new_room)
    {
        new_sector = Room_GetSectorXYZ(new_room, pos);
        if(new_room != new_sector->owner_room)
        {
            new_room = new_sector->owner_room;
        }

        if(!ent->character && (ent->self->room != new_room))
        {
            if((ent->self->room != NULL) && !Room_IsOverlapped(ent->self->room, new_room))
            {
                if(ent->self->room)
                {
                    Room_RemoveEntity(ent->self->room, ent);
                }
                if(new_room)
                {
                    Room_AddEntity(new_room, ent);
                }
            }
        }

        ent->self->room = new_room;
        ent->last_sector = ent->current_sector;

        if(ent->current_sector != new_sector)
        {
            ent->trigger_layout &= (uint8_t)(~ENTITY_TLAYOUT_SSTATUS); // Reset sector status.
            ent->current_sector = new_sector;
        }
    }
}


void Entity_UpdateRigidBody(std::shared_ptr<Entity> ent, int force)
{
    if(ent->type_flags & ENTITY_TYPE_DYNAMIC)
    {
        //btVector3 pos = ent->bt.bt_body[0]->getWorldTransform().getOrigin();
        //vec3_copy(ent->transform+12, pos);
        ent->transform = ent->bt.bt_body[0]->getWorldTransform();
        Entity_UpdateRoomPos(ent);
        for(uint16_t i=0;i<ent->bf.bone_tag_count;i++)
        {
            auto tr = ent->bt.bt_body[i]->getWorldTransform();
            ent->bf.bone_tags[i].full_transform = ent->transform.inverse() * tr;
        }

        // that cycle is necessary only for skinning models;
        for(uint16_t i=0;i<ent->bf.bone_tag_count;i++)
        {
            if(ent->bf.bone_tags[i].parent != NULL)
            {
                ent->bf.bone_tags[i].transform = ent->bf.bone_tags[i].parent->full_transform.inverse() * ent->bf.bone_tags[i].full_transform;
            }
            else
            {
                ent->bf.bone_tags[i].transform = ent->bf.bone_tags[i].full_transform;
            }
        }

        if(ent->character && ent->bt.ghostObjects)
        {
            for(uint16_t i=0;i<ent->bf.bone_tag_count;i++)
            {
                auto tr = ent->bt.bt_body[i]->getWorldTransform();
                auto v = tr * ent->bf.bone_tags[i].mesh_base->centre;
                tr.setOrigin(v);
                ent->bt.ghostObjects[i]->getWorldTransform() = tr;
            }
        }

        if(ent->bf.bone_tag_count == 1)
        {
            ent->bf.bb_min = ent->bf.bone_tags[0].mesh_base->bb_min;
            ent->bf.bb_max = ent->bf.bone_tags[0].mesh_base->bb_max;
        }
        else
        {
            ent->bf.bb_min = ent->bf.bone_tags[0].mesh_base->bb_min;
            ent->bf.bb_max = ent->bf.bone_tags[0].mesh_base->bb_max;
            for(uint16_t i=0;i<ent->bf.bone_tag_count;i++)
            {
                auto& pos = ent->bf.bone_tags[i].full_transform.getOrigin();
                auto& bb_min = ent->bf.bone_tags[i].mesh_base->bb_min;
                auto& bb_max = ent->bf.bone_tags[i].mesh_base->bb_max;
                btScalar r = bb_max[0] - bb_min[0];
                btScalar t = bb_max[1] - bb_min[1];
                r = (t > r)?(t):(r);
                t = bb_max[2] - bb_min[2];
                r = (t > r)?(t):(r);
                r *= 0.5;

                if(ent->bf.bb_min[0] > pos[0] - r)
                {
                    ent->bf.bb_min[0] = pos[0] - r;
                }
                if(ent->bf.bb_min[1] > pos[1] - r)
                {
                    ent->bf.bb_min[1] = pos[1] - r;
                }
                if(ent->bf.bb_min[2] > pos[2] - r)
                {
                    ent->bf.bb_min[2] = pos[2] - r;
                }

                if(ent->bf.bb_max[0] < pos[0] + r)
                {
                    ent->bf.bb_max[0] = pos[0] + r;
                }
                if(ent->bf.bb_max[1] < pos[1] + r)
                {
                    ent->bf.bb_max[1] = pos[1] + r;
                }
                if(ent->bf.bb_max[2] < pos[2] + r)
                {
                    ent->bf.bb_max[2] = pos[2] + r;
                }
            }
        }
    }
    else
    {
        if((ent->bf.animations.model == NULL) ||
           (ent->bt.bt_body == NULL) ||
           ((force == 0) && (ent->bf.animations.model->animation_count == 1) && (ent->bf.animations.model->animations->frames_count == 1)))
        {
            return;
        }

        Entity_UpdateRoomPos(ent);
        if(ent->self->collide_flag != 0x00)
        {
            for(uint16_t i=0;i<ent->bf.bone_tag_count;i++)
            {
                if(ent->bt.bt_body[i])
                {
                    ent->bt.bt_body[i]->getWorldTransform() = ent->transform * ent->bf.bone_tags[i].full_transform;
                }
            }
        }
    }
    Entity_RebuildBV(ent);
}


void Entity_UpdateRotation(std::shared_ptr<Entity> entity)
{
    btScalar sin_t2, cos_t2, t;
    auto& up_dir = entity->transform.getBasis()[2];                                   // OZ
    auto& view_dir = entity->transform.getBasis()[1];                                 // OY
    auto& right_dir = entity->transform.getBasis()[0];                                // OX
    int i;

    if(entity->character != NULL)
    {
        Entity_GhostUpdate(entity);
    }
    i = entity->angles[0] / 360.0;
    i = (entity->angles[0] < 0.0)?(i-1):(i);
    entity->angles[0] -= 360.0 * i;

    i = entity->angles[1] / 360.0;
    i = (entity->angles[1] < 0.0)?(i-1):(i);
    entity->angles[1] -= 360.0 * i;

    i = entity->angles[2] / 360.0;
    i = (entity->angles[2] < 0.0)?(i-1):(i);
    entity->angles[2] -= 360.0 * i;

    t = entity->angles[0] * M_PI / 180.0;
    sin_t2 = sin(t);
    cos_t2 = cos(t);

    /*
     * LEFT - RIGHT INIT
     */

    view_dir[0] =-sin_t2;                                                       // OY - view
    view_dir[1] = cos_t2;
    view_dir[2] = 0.0;
    view_dir[3] = 0.0;

    right_dir[0] = cos_t2;                                                      // OX - right
    right_dir[1] = sin_t2;
    right_dir[2] = 0.0;
    right_dir[3] = 0.0;

    up_dir[0] = 0.0;                                                            // OZ - up
    up_dir[1] = 0.0;
    up_dir[2] = 1.0;
    up_dir[3] = 0.0;

    if(entity->angles[1] != 0.0)
    {
        t = entity->angles[1] * M_PI / 360.0;                                   // UP - DOWN
        sin_t2 = sin(t);
        cos_t2 = cos(t);
        btVector3 R;
        R[3] = cos_t2;
        R[0] = right_dir[0] * sin_t2;
        R[1] = right_dir[1] * sin_t2;
        R[2] = right_dir[2] * sin_t2;
        auto Rt = -R;
        up_dir = R * up_dir * Rt;
        view_dir = R * view_dir * Rt;
    }

    if(entity->angles[2] != 0.0)
    {
        t = entity->angles[2] * M_PI / 360.0;                                   // ROLL
        sin_t2 = sin(t);
        cos_t2 = cos(t);
        btVector3 R;
        R[3] = cos_t2;
        R[0] = view_dir[0] * sin_t2;
        R[1] = view_dir[1] * sin_t2;
        R[2] = view_dir[2] * sin_t2;
        auto Rt = -R;

        right_dir = R * right_dir * Rt;
        up_dir = R * up_dir * Rt;
    }

    view_dir[3] = 0.0;
    right_dir[3] = 0.0;
    up_dir[3] = 0.0;

    if(entity->character != NULL)
    {
        Entity_FixPenetrations(entity, NULL);
    }
}


void Entity_UpdateCurrentSpeed(std::shared_ptr<Entity> entity, int zeroVz)
{
    btScalar t  = entity->current_speed * entity->character->speed_mult;
    btScalar vz = (zeroVz)?(0.0):(entity->speed[2]);

    if(entity->dir_flag & ENT_MOVE_FORWARD)
    {
        entity->speed = entity->transform.getBasis()[1] * t;
    }
    else if(entity->dir_flag & ENT_MOVE_BACKWARD)
    {
        entity->speed = entity->transform.getBasis()[1] * -t;
    }
    else if(entity->dir_flag & ENT_MOVE_LEFT)
    {
        entity->speed = entity->transform.getBasis()[0] * -t;
    }
    else if(entity->dir_flag & ENT_MOVE_RIGHT)
    {
        entity->speed = entity->transform.getBasis()[0] * t;
    }
    else
    {
        entity->speed.setZero();
    }

    entity->speed[2] = vz;
}


void Entity_AddOverrideAnim(std::shared_ptr<Entity> ent, int model_id)
{
    skeletal_model_p sm = World_GetModelByID(&engine_world, model_id);

    if((sm != NULL) && (sm->mesh_count == ent->bf.bone_tag_count))
    {
        ss_animation_p ss_anim = (ss_animation_p)malloc(sizeof(ss_animation_t));

        ss_anim->model = sm;
        ss_anim->onFrame = NULL;
        ss_anim->next = ent->bf.animations.next;
        ent->bf.animations.next = ss_anim;

        ss_anim->frame_time = 0.0;
        ss_anim->next_state = 0;
        ss_anim->lerp = 0.0;
        ss_anim->current_animation = 0;
        ss_anim->current_frame = 0;
        ss_anim->next_animation = 0;
        ss_anim->next_frame = 0;
        ss_anim->period = 1.0 / 30.0;;
    }
}


void Entity_UpdateCurrentBoneFrame(struct ss_bone_frame_s *bf, const btTransform* etr)
{
    ss_bone_tag_p btag = bf->bone_tags;
    bone_tag_p src_btag, next_btag;
    skeletal_model_p model = bf->animations.model;
    bone_frame_p curr_bf, next_bf;

    next_bf = model->animations[bf->animations.next_animation].frames + bf->animations.next_frame;
    curr_bf = model->animations[bf->animations.current_animation].frames + bf->animations.current_frame;

    btScalar t = 1.0 - bf->animations.lerp;
    btVector3 tr, cmd_tr;
    if(etr && (curr_bf->command & ANIM_CMD_MOVE))
    {
        tr = *etr * curr_bf->move;
        cmd_tr = tr * bf->animations.lerp;
    }
    else
    {
        tr.setZero();
        cmd_tr.setZero();
    }

    bf->bb_max = curr_bf->bb_max * t + next_bf->bb_max * bf->animations.lerp;
    bf->bb_max += cmd_tr;
    bf->bb_min = curr_bf->bb_min * t + next_bf->bb_min * bf->animations.lerp;
    bf->bb_min += cmd_tr;
    bf->centre = curr_bf->centre * t + next_bf->centre * bf->animations.lerp;
    bf->centre += cmd_tr;

    vec3_interpolate_macro(bf->pos, curr_bf->pos, next_bf->pos, bf->animations.lerp, t);
    bf->pos += cmd_tr;
    next_btag = next_bf->bone_tags;
    src_btag = curr_bf->bone_tags;
    for(uint16_t k=0;k<curr_bf->bone_tag_count;k++,btag++,src_btag++,next_btag++)
    {
        vec3_interpolate_macro(btag->offset, src_btag->offset, next_btag->offset, bf->animations.lerp, t);
        btag->transform.getOrigin() = btag->offset;
        btag->transform.getOrigin()[3] = 1.0;
        if(k == 0)
        {
            btag->transform.getOrigin() += bf->pos;
            btag->qrotate = vec4_slerp(src_btag->qrotate, next_btag->qrotate, bf->animations.lerp);
        }
        else
        {
            bone_tag_p ov_src_btag = src_btag;
            bone_tag_p ov_next_btag = next_btag;
            btScalar ov_lerp = bf->animations.lerp;
            for(ss_animation_p ov_anim=bf->animations.next;ov_anim!=NULL;ov_anim = ov_anim->next)
            {
                if((ov_anim->model != NULL) && (ov_anim->model->mesh_tree[k].replace_anim != 0))
                {
                    bone_frame_p ov_curr_bf = ov_anim->model->animations[ov_anim->current_animation].frames + ov_anim->current_frame;
                    bone_frame_p ov_next_bf = ov_anim->model->animations[ov_anim->next_animation].frames + ov_anim->next_frame;
                    ov_src_btag = ov_curr_bf->bone_tags + k;
                    ov_next_btag = ov_next_bf->bone_tags + k;
                    ov_lerp = ov_anim->lerp;
                    break;
                }
            }
            btag->qrotate = vec4_slerp(ov_src_btag->qrotate, ov_next_btag->qrotate, ov_lerp);
        }
        btag->transform.setRotation(btag->qrotate);
    }

    /*
     * build absolute coordinate matrix system
     */
    btag = bf->bone_tags;
    btag->full_transform = btag->transform;
    btag++;
    for(uint16_t k=1;k<curr_bf->bone_tag_count;k++,btag++)
    {
        btag->full_transform = btag->parent->full_transform * btag->transform;
    }
}


int  Entity_GetSubstanceState(std::shared_ptr<Entity> entity)
{
    if((!entity) || (!entity->character))
    {
        return 0;
    }

    if(entity->self->room->flags & TR_ROOM_FLAG_QUICKSAND)
    {
        if(entity->character->height_info.transition_level > entity->transform.getOrigin()[2] + entity->character->Height)
        {
            return ENTITY_SUBSTANCE_QUICKSAND_CONSUMED;
        }
        else
        {
            return ENTITY_SUBSTANCE_QUICKSAND_SHALLOW;
        }
    }
    else if(!entity->character->height_info.water)
    {
        return ENTITY_SUBSTANCE_NONE;
    }
    else if( entity->character->height_info.water &&
            (entity->character->height_info.transition_level > entity->transform.getOrigin()[2]) &&
            (entity->character->height_info.transition_level < entity->transform.getOrigin()[2] + entity->character->wade_depth) )
    {
        return ENTITY_SUBSTANCE_WATER_SHALLOW;
    }
    else if( entity->character->height_info.water &&
            (entity->character->height_info.transition_level > entity->transform.getOrigin()[2] + entity->character->wade_depth) )
    {
        return ENTITY_SUBSTANCE_WATER_WADE;
    }
    else
    {
        return ENTITY_SUBSTANCE_WATER_SWIM;
    }
}

btScalar Entity_FindDistance(std::shared_ptr<Entity> entity_1, std::shared_ptr<Entity> entity_2)
{
    auto v1 = entity_1->transform.getOrigin();
    auto v2 = entity_2->transform.getOrigin();
    return (v1 - v2).length();
}

void Entity_DoAnimCommands(std::shared_ptr<Entity> entity, struct ss_animation_s *ss_anim, int changing)
{
    if((engine_world.anim_commands_count == 0) || (ss_anim->model == NULL))
    {
        return;  // If no anim commands
    }

    animation_frame_p af  = ss_anim->model->animations + ss_anim->current_animation;
    if(af->num_anim_commands <= 255)
    {
        uint32_t count        = af->num_anim_commands;
        int16_t *pointer      = engine_world.anim_commands + af->anim_command;
        int8_t   random_value = 0;

        for(uint32_t i = 0; i < count; i++, pointer++)
        {
            switch(*pointer)
            {
                case TR_ANIMCOMMAND_SETPOSITION:
                    // This command executes ONLY at the end of animation.
                    pointer += 3; // Parse through 3 operands.
                    break;

                case TR_ANIMCOMMAND_JUMPDISTANCE:
                    // This command executes ONLY at the end of animation.
                    pointer += 2; // Parse through 2 operands.
                    break;

                case TR_ANIMCOMMAND_EMPTYHANDS:
                    ///@FIXME: Behaviour is yet to be discovered.
                    break;

                case TR_ANIMCOMMAND_KILL:
                    // This command executes ONLY at the end of animation.
                    if(ss_anim->current_frame == af->frames_count - 1)
                    {
                        if(entity->character)
                        {
                            entity->character->resp.kill = 1;
                        }
                    }

                    break;

                case TR_ANIMCOMMAND_PLAYSOUND:
                    int16_t sound_index;

                    if(ss_anim->current_frame == *++pointer)
                    {
                        sound_index = *++pointer & 0x3FFF;

                        // Quick workaround for TR3 quicksand.
                        if((Entity_GetSubstanceState(entity) == ENTITY_SUBSTANCE_QUICKSAND_CONSUMED) ||
                           (Entity_GetSubstanceState(entity) == ENTITY_SUBSTANCE_QUICKSAND_SHALLOW)   )
                        {
                            sound_index = 18;
                        }

                        if(*pointer & TR_ANIMCOMMAND_CONDITION_WATER)
                        {
                            if(Entity_GetSubstanceState(entity) == ENTITY_SUBSTANCE_WATER_SHALLOW)
                                Audio_Send(sound_index, TR_AUDIO_EMITTER_ENTITY, entity->id);
                        }
                        else if(*pointer & TR_ANIMCOMMAND_CONDITION_LAND)
                        {
                            if(Entity_GetSubstanceState(entity) != ENTITY_SUBSTANCE_WATER_SHALLOW)
                                Audio_Send(sound_index, TR_AUDIO_EMITTER_ENTITY, entity->id);
                        }
                        else
                        {
                            Audio_Send(sound_index, TR_AUDIO_EMITTER_ENTITY, entity->id);
                        }

                    }
                    else
                    {
                        pointer++;
                    }
                    break;

                case TR_ANIMCOMMAND_PLAYEFFECT:
                    // Effects (flipeffects) are various non-typical actions which vary
                    // across different TR game engine versions. There are common ones,
                    // however, and currently only these are supported.
                    if(ss_anim->current_frame == *++pointer)
                    {
                        switch(*++pointer & 0x3FFF)
                        {
                            case TR_EFFECT_SHAKESCREEN:
                                if(engine_world.Character)
                                {
                                    btScalar dist = Entity_FindDistance(engine_world.Character, entity);
                                    dist = (dist > TR_CAM_MAX_SHAKE_DISTANCE)?(0):((TR_CAM_MAX_SHAKE_DISTANCE - dist) / 1024.0);
                                    if(dist > 0)
                                        Cam_Shake(renderer.cam, (dist * TR_CAM_DEFAULT_SHAKE_POWER), 0.5);
                                }
                                break;

                            case TR_EFFECT_CHANGEDIRECTION:
                                break;

                            case TR_EFFECT_HIDEOBJECT:
                                entity->state_flags &= ~ENTITY_STATE_VISIBLE;
                                break;

                            case TR_EFFECT_SHOWOBJECT:
                                entity->state_flags |= ENTITY_STATE_VISIBLE;
                                break;

                            case TR_EFFECT_PLAYSTEPSOUND:
                                // Please note that we bypass land/water mask, as TR3-5 tends to ignore
                                // this flag and play step sound in any case on land, ignoring it
                                // completely in water rooms.
                                if(!Entity_GetSubstanceState(entity))
                                {
                                    // TR3-5 footstep map.
                                    // We define it here as a magic numbers array, because TR3-5 versions
                                    // fortunately have no differences in footstep sounds order.
                                    // Also note that some footstep types mutually share same sound IDs
                                    // across different TR versions.
                                    switch(entity->current_sector->material)
                                    {
                                        case SECTOR_MATERIAL_MUD:
                                            Audio_Send(288, TR_AUDIO_EMITTER_ENTITY, entity->id);
                                            break;

                                        case SECTOR_MATERIAL_SNOW:  // TR3 & TR5 only
                                            if(engine_world.version != TR_IV)
                                            {
                                                Audio_Send(293, TR_AUDIO_EMITTER_ENTITY, entity->id);
                                            }
                                            break;

                                        case SECTOR_MATERIAL_SAND:  // Same as grass
                                            Audio_Send(291, TR_AUDIO_EMITTER_ENTITY, entity->id);
                                            break;

                                        case SECTOR_MATERIAL_GRAVEL:
                                            Audio_Send(290, TR_AUDIO_EMITTER_ENTITY, entity->id);
                                            break;

                                        case SECTOR_MATERIAL_ICE:   // TR3 & TR5 only
                                            if(engine_world.version != TR_IV)
                                            {
                                                Audio_Send(289, TR_AUDIO_EMITTER_ENTITY, entity->id);
                                            }
                                            break;

                                        case SECTOR_MATERIAL_WATER: // BYPASS!
                                            // Audio_Send(17, TR_AUDIO_EMITTER_ENTITY, entity->id);
                                            break;

                                        case SECTOR_MATERIAL_STONE: // DEFAULT SOUND, BYPASS!
                                            // Audio_Send(-1, TR_AUDIO_EMITTER_ENTITY, entity->id);
                                            break;

                                        case SECTOR_MATERIAL_WOOD:
                                            Audio_Send(292, TR_AUDIO_EMITTER_ENTITY, entity->id);
                                            break;

                                        case SECTOR_MATERIAL_METAL:
                                            Audio_Send(294, TR_AUDIO_EMITTER_ENTITY, entity->id);
                                            break;

                                        case SECTOR_MATERIAL_MARBLE:    // TR4 only
                                            if(engine_world.version == TR_IV)
                                            {
                                                Audio_Send(293, TR_AUDIO_EMITTER_ENTITY, entity->id);
                                            }
                                            break;

                                        case SECTOR_MATERIAL_GRASS:     // Same as sand
                                            Audio_Send(291, TR_AUDIO_EMITTER_ENTITY, entity->id);
                                            break;

                                        case SECTOR_MATERIAL_CONCRETE:  // DEFAULT SOUND, BYPASS!
                                            Audio_Send(-1, TR_AUDIO_EMITTER_ENTITY, entity->id);
                                            break;

                                        case SECTOR_MATERIAL_OLDWOOD:   // Same as wood
                                            Audio_Send(292, TR_AUDIO_EMITTER_ENTITY, entity->id);
                                            break;

                                        case SECTOR_MATERIAL_OLDMETAL:  // Same as metal
                                            Audio_Send(294, TR_AUDIO_EMITTER_ENTITY, entity->id);
                                            break;
                                    }
                                }
                                break;

                            case TR_EFFECT_BUBBLE:
                                ///@FIXME: Spawn bubble particle here, when particle system is developed.
                                random_value = rand() % 100;
                                if(random_value > 60)
                                {
                                    Audio_Send(TR_AUDIO_SOUND_BUBBLE, TR_AUDIO_EMITTER_ENTITY, entity->id);
                                }
                                break;

                            default:
                                ///@FIXME: TODO ALL OTHER EFFECTS!
                                break;
                        }
                    }
                    else
                    {
                        pointer++;
                    }
                    break;
            }
        }
    }
}


room_sector_s* Entity_GetLowestSector(room_sector_s* sector)
{
    room_sector_p lowest_sector = sector;

    for(room_sector_p rs=sector;rs!=NULL;rs=rs->sector_below)
    { lowest_sector = rs; }

    return lowest_sector;
}


room_sector_s* Entity_GetHighestSector(room_sector_s* sector)
{
    room_sector_p highest_sector = sector;

    for(room_sector_p rs=sector;rs!=NULL;rs=rs->sector_above)
    { highest_sector = rs; }

    return highest_sector;
}


void Entity_ProcessSector(std::shared_ptr<Entity> ent)
{
    if(!ent->current_sector) return;

    // Calculate both above and below sectors for further usage.
    // Sector below is generally needed for getting proper trigger index,
    // as many triggers tend to be called from the lowest room in a row
    // (e.g. first trapdoor in The Great Wall, etc.)
    // Sector above primarily needed for paranoid cases of monkeyswing.

    room_sector_p highest_sector = Entity_GetHighestSector(ent->current_sector);
    room_sector_p lowest_sector  = Entity_GetLowestSector(ent->current_sector);

    if(ent->character)
    {
        ent->character->height_info.walls_climb_dir  = 0;
        ent->character->height_info.walls_climb_dir |= lowest_sector->flags & (SECTOR_FLAG_CLIMB_WEST  |
                                                                               SECTOR_FLAG_CLIMB_EAST  |
                                                                               SECTOR_FLAG_CLIMB_NORTH |
                                                                               SECTOR_FLAG_CLIMB_SOUTH );

        ent->character->height_info.walls_climb     = (ent->character->height_info.walls_climb_dir > 0);
        ent->character->height_info.ceiling_climb   = 0x00;

        if((highest_sector->flags & SECTOR_FLAG_CLIMB_CEILING) || (lowest_sector->flags & SECTOR_FLAG_CLIMB_CEILING))
        {
            ent->character->height_info.ceiling_climb = 0x01;
        }

        if(lowest_sector->flags & SECTOR_FLAG_DEATH)
        {
            if((ent->move_type == MOVE_ON_FLOOR)    ||
               (ent->move_type == MOVE_UNDERWATER) ||
               (ent->move_type == MOVE_WADE)        ||
               (ent->move_type == MOVE_ON_WATER)    ||
               (ent->move_type == MOVE_QUICKSAND))
            {
                Character_SetParam(ent, PARAM_HEALTH, 0.0);
                ent->character->resp.kill = 1;
            }
        }
    }

    // If entity either marked as trigger activator (Lara) or heavytrigger activator (other entities),
    // we try to execute a trigger for this sector.

    if((ent->type_flags & ENTITY_TYPE_TRIGGER_ACTIVATOR) || (ent->type_flags & ENTITY_TYPE_HEAVYTRIGGER_ACTIVATOR))
    {
        // Look up trigger function table and run trigger if it exists.

        int top = lua_gettop(engine_lua);
        lua_getglobal(engine_lua, "tlist_RunTrigger");
        if(lua_isfunction(engine_lua, -1))
        {
            lua_pushnumber(engine_lua, lowest_sector->trig_index);
            lua_pushnumber(engine_lua, ((ent->bf.animations.model->id == 0) ? TR_ACTIVATORTYPE_LARA : TR_ACTIVATORTYPE_MISC));
            lua_pushnumber(engine_lua, ent->id);
            lua_CallAndLog(engine_lua, 3, 1, 0);
        }
        lua_settop(engine_lua, top);
    }
}


void Entity_SetAnimation(std::shared_ptr<Entity> entity, int animation, int frame, int another_model)
{
    if(!entity || !entity->bf.animations.model || (animation >= entity->bf.animations.model->animation_count))
    {
        return;
    }

    animation = (animation < 0)?(0):(animation);
    entity->bt.no_fix_all = 0x00;

    if(another_model >= 0)
    {
        skeletal_model_p model = World_GetModelByID(&engine_world, another_model);
        if((!model) || (animation >= model->animation_count)) return;
        entity->bf.animations.model = model;
    }

    animation_frame_p anim = &entity->bf.animations.model->animations[animation];

    entity->bf.animations.lerp = 0.0;
    frame %= anim->frames_count;
    frame = (frame >= 0)?(frame):(anim->frames_count - 1 + frame);
    entity->bf.animations.period = 1.0 / 30.0;

    entity->bf.animations.last_state = anim->state_id;
    entity->bf.animations.next_state = anim->state_id;
    entity->current_speed = anim->speed_x;
    entity->bf.animations.current_animation = animation;
    entity->bf.animations.current_frame = frame;
    entity->bf.animations.next_animation = animation;
    entity->bf.animations.next_frame = frame;

    entity->bf.animations.frame_time = (btScalar)frame * entity->bf.animations.period;
    //long int t = (entity->bf.animations.frame_time) / entity->bf.animations.period;
    //btScalar dt = entity->bf.animations.frame_time - (btScalar)t * entity->bf.animations.period;
    entity->bf.animations.frame_time = (btScalar)frame * entity->bf.animations.period;// + dt;

    Entity_UpdateCurrentBoneFrame(&entity->bf, &entity->transform);
    Entity_UpdateRigidBody(entity, 0);
}


struct state_change_s *Anim_FindStateChangeByAnim(struct animation_frame_s *anim, int state_change_anim)
{
    if(state_change_anim >= 0)
    {
        state_change_p ret = anim->state_change;
        for(uint16_t i=0;i<anim->state_change_count;i++,ret++)
        {
            for(uint16_t j=0;j<ret->anim_dispatch_count;j++)
            {
                if(ret->anim_dispatch[j].next_anim == state_change_anim)
                {
                    return ret;
                }
            }
        }
    }

    return NULL;
}


struct state_change_s *Anim_FindStateChangeByID(struct animation_frame_s *anim, uint32_t id)
{
    state_change_p ret = anim->state_change;
    for(uint16_t i=0;i<anim->state_change_count;i++,ret++)
    {
        if(ret->id == id)
        {
            return ret;
        }
    }

    return NULL;
}


int Entity_GetAnimDispatchCase(std::shared_ptr<Entity> entity, uint32_t id)
{
    animation_frame_p anim = entity->bf.animations.model->animations + entity->bf.animations.current_animation;
    state_change_p stc = anim->state_change;

    for(uint16_t i=0;i<anim->state_change_count;i++,stc++)
    {
        if(stc->id == id)
        {
            anim_dispatch_p disp = stc->anim_dispatch;
            for(uint16_t j=0;j<stc->anim_dispatch_count;j++,disp++)
            {
                if((disp->frame_high >= disp->frame_low) && (entity->bf.animations.current_frame >= disp->frame_low) && (entity->bf.animations.current_frame <= disp->frame_high))// ||
                   //(disp->frame_high <  disp->frame_low) && ((entity->bf.current_frame >= disp->frame_low) || (entity->bf.current_frame <= disp->frame_high)))
                {
                    return (int)j;
                }
            }
        }
    }

    return -1;
}

/*
 * Next frame and next anim calculation function.
 */
void Entity_GetNextFrame(struct ss_bone_frame_s *bf, btScalar time, struct state_change_s *stc, int16_t *frame, int16_t *anim, uint16_t anim_flags)
{
    animation_frame_p curr_anim = bf->animations.model->animations + bf->animations.current_animation;

    *frame = (bf->animations.frame_time + time) / bf->animations.period;
    *frame = (*frame >= 0.0)?(*frame):(0.0);                                    // paranoid checking
    *anim  = bf->animations.current_animation;

    /*
     * Flag has a highest priority
     */
    if(anim_flags == ANIM_LOOP_LAST_FRAME)
    {
        if(*frame >= curr_anim->frames_count - 1)
        {
            *frame = curr_anim->frames_count - 1;
            *anim  = bf->animations.current_animation;                          // paranoid dublicate
        }
        return;
    }
    else if(anim_flags == ANIM_LOCK)
    {
        *frame = 0;
        *anim  = bf->animations.current_animation;
        return;
    }

    /*
     * Check next anim if frame >= frames_count
     */
    if(*frame >= curr_anim->frames_count)
    {
        if(curr_anim->next_anim)
        {
            *frame = curr_anim->next_frame;
            *anim  = curr_anim->next_anim->id;
            return;
        }

        *frame %= curr_anim->frames_count;
        *anim   = bf->animations.current_animation;                             // paranoid dublicate
        return;
    }

    /*
     * State change check
     */
    if(stc != NULL)
    {
        anim_dispatch_p disp = stc->anim_dispatch;
        for(uint16_t i=0;i<stc->anim_dispatch_count;i++,disp++)
        {
            if((disp->frame_high >= disp->frame_low) && (*frame >= disp->frame_low) && (*frame <= disp->frame_high))
            {
                *anim  = disp->next_anim;
                *frame = disp->next_frame;
                //*frame = (disp->next_frame + (*frame - disp->frame_low)) % bf->model->animations[disp->next_anim].frames_count;
                return;                                                         // anim was changed
            }
        }
    }
}


void Entity_DoAnimMove(std::shared_ptr<Entity> entity, int16_t *anim, int16_t *frame)
{
    if(entity->bf.animations.model != NULL)
    {
        animation_frame_p curr_af = entity->bf.animations.model->animations + entity->bf.animations.current_animation;
        bone_frame_p curr_bf = curr_af->frames + entity->bf.animations.current_frame;

        if(curr_bf->command & ANIM_CMD_JUMP)
        {
            Character_SetToJump(entity, -curr_bf->v_Vertical, curr_bf->v_Horizontal);
        }
        if(curr_bf->command & ANIM_CMD_CHANGE_DIRECTION)
        {
            //Con_Printf("ROTATED: anim = %d, frame = %d of %d", entity->bf.animations.current_animation, entity->bf.animations.current_frame, entity->bf.animations.model->animations[entity->bf.animations.current_animation].frames_count);
            entity->angles[0] += 180.0;
            if(entity->move_type == MOVE_UNDERWATER)
            {
                entity->angles[1] = -entity->angles[1];                         // for underwater case
            }
            if(entity->dir_flag == ENT_MOVE_BACKWARD)
            {
                entity->dir_flag = ENT_MOVE_FORWARD;
            }
            else if(entity->dir_flag == ENT_MOVE_FORWARD)
            {
                entity->dir_flag = ENT_MOVE_BACKWARD;
            }
            Entity_UpdateRotation(entity);
            Entity_SetAnimation(entity, curr_af->next_anim->id, curr_af->next_frame);
            *anim = entity->bf.animations.current_animation;
            *frame = entity->bf.animations.current_frame;
        }
        if(curr_bf->command & ANIM_CMD_MOVE)
        {
            btVector3 tr = entity->transform.getOrigin() * curr_bf->move;
            entity->transform.getOrigin() += tr;
        }
    }
}

void Character_DoWeaponFrame(std::shared_ptr<Entity> entity, btScalar time);

/**
 * In original engine (+ some information from anim_commands) the anim_commands implement in beginning of frame
 */
///@TODO: rewrite as a cycle through all bf.animations list
int Entity_Frame(std::shared_ptr<Entity> entity, btScalar time)
{
    int16_t frame, anim, ret = 0x00;
    long int t;
    btScalar dt;
    animation_frame_p af;
    state_change_p stc;
    ss_animation_p ss_anim;

    if((entity == NULL) || (entity->type_flags & ENTITY_TYPE_DYNAMIC) || !(entity->state_flags & ENTITY_STATE_ACTIVE)  || !(entity->state_flags & ENTITY_STATE_ENABLED) ||
       (entity->bf.animations.model == NULL) || ((entity->bf.animations.model->animation_count == 1) && (entity->bf.animations.model->animations->frames_count == 1)))
    {
        return 0;
    }

    if(entity->bf.animations.anim_flags & ANIM_LOCK) return 1;                  // penetration fix will be applyed in Character_Move... functions

    ss_anim = &entity->bf.animations;

    Entity_GhostUpdate(entity);

    entity->bf.animations.lerp = 0.0;
    stc = Anim_FindStateChangeByID(ss_anim->model->animations + ss_anim->current_animation, ss_anim->next_state);
    Entity_GetNextFrame(&entity->bf, time, stc, &frame, &anim, ss_anim->anim_flags);
    if(ss_anim->current_animation != anim)
    {
        ss_anim->last_animation = ss_anim->current_animation;

        ret = 0x02;
        Entity_DoAnimCommands(entity, &entity->bf.animations, ret);
        Entity_DoAnimMove(entity, &anim, &frame);

        Entity_SetAnimation(entity, anim, frame);
        stc = Anim_FindStateChangeByID(ss_anim->model->animations + ss_anim->current_animation, ss_anim->next_state);
    }
    else if(ss_anim->current_frame != frame)
    {
        if(ss_anim->current_frame == 0)
        {
            ss_anim->last_animation = ss_anim->current_animation;
        }

        ret = 0x01;
        Entity_DoAnimCommands(entity, &entity->bf.animations, ret);
        Entity_DoAnimMove(entity, &anim, &frame);
    }

    af = entity->bf.animations.model->animations + entity->bf.animations.current_animation;
    entity->bf.animations.frame_time += time;

    t = (entity->bf.animations.frame_time) / entity->bf.animations.period;
    dt = entity->bf.animations.frame_time - (btScalar)t * entity->bf.animations.period;
    entity->bf.animations.frame_time = (btScalar)frame * entity->bf.animations.period + dt;
    entity->bf.animations.lerp = dt / entity->bf.animations.period;
    Entity_GetNextFrame(&entity->bf, entity->bf.animations.period, stc, &entity->bf.animations.next_frame, &entity->bf.animations.next_animation, ss_anim->anim_flags);

    // Update acceleration.
    // With variable framerate, we don't know when we'll reach final
    // frame for sure, so we use native frame number check to increase acceleration.

    if((entity->character) && (ss_anim->current_frame != frame))
    {

        // NB!!! For Lara, we update ONLY X-axis speed/accel.

        if((af->accel_x == 0) || (frame < entity->bf.animations.current_frame))
        {
            entity->current_speed  = af->speed_x;
        }
        else
        {
            entity->current_speed += af->accel_x;
        }
    }

    entity->bf.animations.current_frame = frame;


    Character_DoWeaponFrame(entity, time);

    if(entity->bf.animations.onFrame != NULL)
    {
        entity->bf.animations.onFrame(entity, &entity->bf.animations, ret);
    }

    Entity_UpdateCurrentBoneFrame(&entity->bf, &entity->transform);
    if(entity->character != NULL)
    {
        Entity_FixPenetrations(entity, NULL);
    }

    return ret;
}

/**
 * The function rebuild / renew entity's BV
 */
void Entity_RebuildBV(std::shared_ptr<Entity> ent)
{
    if((ent != NULL) && (ent->bf.animations.model != NULL))
    {
        /*
         * get current BB from animation
         */
        OBB_Rebuild(ent->obb.get(), ent->bf.bb_min, ent->bf.bb_max);
        OBB_Transform(ent->obb.get());
    }
}


void Entity_CheckActivators(std::shared_ptr<Entity> ent)
{
    if((ent != NULL) && (ent->self->room != NULL))
    {
        btVector3 ppos = ent->transform.getOrigin() + ent->transform.getBasis()[1] * ent->bf.bb_max[1];
        engine_container_p cont = ent->self->room->containers;
        for(;cont;cont=cont->next)
        {
            if((cont->object_type == OBJECT_ENTITY) && (cont->object))
            {
                std::shared_ptr<Entity> e = std::static_pointer_cast<Entity>(cont->object);
                btScalar r = e->activation_radius;
                r *= r;
                if((e->type_flags & ENTITY_TYPE_INTERACTIVE) && (e->state_flags & ENTITY_STATE_ENABLED))
                {
                    //Mat4_vec3_mul_macro(pos, e->transform, e->activation_offset);
                    if((e != ent) && (OBB_OBB_Test(e, ent) == 1))//(vec3_dist_sq(ent->transform+12, pos) < r))
                    {
                        lua_ExecEntity(engine_lua, ENTITY_CALLBACK_ACTIVATE, e->id, ent->id);
                    }
                }
                else if((e->type_flags & ENTITY_TYPE_PICKABLE) && (e->state_flags & ENTITY_STATE_ENABLED))
                {
                    const btVector3& v = e->transform.getOrigin();
                    if((e != ent) && ((v[0] - ppos[0]) * (v[0] - ppos[0]) + (v[1] - ppos[1]) * (v[1] - ppos[1]) < r) &&
                                      (v[2] + 32.0 > ent->transform.getOrigin()[2] + ent->bf.bb_min[2]) && (v[2] - 32.0 < ent->transform.getOrigin()[2] + ent->bf.bb_max[2]))
                    {
                        lua_ExecEntity(engine_lua, ENTITY_CALLBACK_ACTIVATE, e->id, ent->id);
                    }
                }
            }
        }
    }
}


void Entity_MoveForward(std::shared_ptr<Entity> ent, btScalar dist)
{
    ent->transform.getOrigin() += ent->transform.getBasis()[1] * dist;
}


void Entity_MoveStrafe(std::shared_ptr<Entity> ent, btScalar dist)
{
    ent->transform.getOrigin() += ent->transform.getBasis()[0] * dist;
}


void Entity_MoveVertical(std::shared_ptr<Entity> ent, btScalar dist)
{
    ent->transform.getOrigin() += ent->transform.getBasis()[2] * dist;
}


/* There are stick code for multianimation (weapon mode) testing
 * Model replacing will be upgraded too, I have to add override
 * flags to model manually in the script*/
void Character_DoWeaponFrame(std::shared_ptr<Entity> entity, btScalar time)
{
    if(entity->character != NULL)
    {
        /* anims (TR_I - TR_V):
         * pistols:
         * 0: idle to fire;
         * 1: draw weapon (short?);
         * 2: draw weapon (full);
         * 3: fire process;
         *
         * shotgun, rifles, crossbow, harpoon, launchers (2 handed weapons):
         * 0: idle to fire;
         * 1: draw weapon;
         * 2: fire process;
         * 3: hide weapon;
         * 4: idle to fire (targeted);
         */
        if((entity->character->cmd.ready_weapon != 0x00) && (entity->character->current_weapon > 0) && (entity->character->weapon_current_state == WEAPON_STATE_HIDE))
        {
            Character_SetWeaponModel(entity, entity->character->current_weapon, 1);
        }

        btScalar dt;
        int t;

        for(ss_animation_p ss_anim=entity->bf.animations.next;ss_anim!=NULL;ss_anim=ss_anim->next)
        {
            if((ss_anim->model != NULL) && (ss_anim->model->animation_count > 4))
            {
                switch(entity->character->weapon_current_state)
                {
                    case WEAPON_STATE_HIDE:
                        if(entity->character->cmd.ready_weapon)   // ready weapon
                        {
                            ss_anim->current_animation = 1;
                            ss_anim->next_animation = 1;
                            ss_anim->current_frame = 0;
                            ss_anim->next_frame = 0;
                            ss_anim->frame_time = 0.0;
                            entity->character->weapon_current_state = WEAPON_STATE_HIDE_TO_READY;
                        }
                        break;

                    case WEAPON_STATE_HIDE_TO_READY:
                        ss_anim->frame_time += time;
                        ss_anim->current_frame = (ss_anim->frame_time) / ss_anim->period;
                        dt = ss_anim->frame_time - (btScalar)ss_anim->current_frame * ss_anim->period;
                        ss_anim->lerp = dt / ss_anim->period;
                        t = ss_anim->model->animations[ss_anim->current_animation].frames_count;

                        if(ss_anim->current_frame < t - 1)
                        {
                            ss_anim->next_frame = (ss_anim->current_frame + 1) % t;
                            ss_anim->next_animation = ss_anim->current_animation;
                        }
                        else if(ss_anim->current_frame < t)
                        {
                            ss_anim->next_frame = 0;
                            ss_anim->next_animation = 0;
                        }
                        else
                        {
                            ss_anim->current_frame = 0;
                            ss_anim->current_animation = 0;
                            ss_anim->next_frame = 0;
                            ss_anim->next_animation = 0;
                            ss_anim->frame_time = 0.0;
                            entity->character->weapon_current_state = WEAPON_STATE_IDLE;
                        }
                        break;

                    case WEAPON_STATE_IDLE:
                        ss_anim->current_frame = 0;
                        ss_anim->current_animation = 0;
                        ss_anim->next_frame = 0;
                        ss_anim->next_animation = 0;
                        ss_anim->frame_time = 0.0;
                        if(entity->character->cmd.ready_weapon)
                        {
                            ss_anim->current_animation = 3;
                            ss_anim->next_animation = 3;
                            ss_anim->current_frame = ss_anim->next_frame = 0;
                            ss_anim->frame_time = 0.0;
                            entity->character->weapon_current_state = WEAPON_STATE_IDLE_TO_HIDE;
                        }
                        else if(entity->character->cmd.action)
                        {
                            entity->character->weapon_current_state = WEAPON_STATE_IDLE_TO_FIRE;
                        }
                        else
                        {
                            // do nothing here, may be;
                        }
                        break;

                    case WEAPON_STATE_FIRE_TO_IDLE:
                        // Yes, same animation, reverse frames order;
                        t = ss_anim->model->animations[ss_anim->current_animation].frames_count;
                        ss_anim->frame_time += time;
                        ss_anim->current_frame = (ss_anim->frame_time) / ss_anim->period;
                        dt = ss_anim->frame_time - (btScalar)ss_anim->current_frame * ss_anim->period;
                        ss_anim->lerp = dt / ss_anim->period;
                        ss_anim->current_frame = t - 1 - ss_anim->current_frame;
                        if(ss_anim->current_frame > 0)
                        {
                            ss_anim->next_frame = ss_anim->current_frame - 1;
                            ss_anim->next_animation = ss_anim->current_animation;
                        }
                        else
                        {
                            ss_anim->next_frame = ss_anim->current_frame = 0;
                            ss_anim->next_animation = ss_anim->current_animation;
                            entity->character->weapon_current_state = WEAPON_STATE_IDLE;
                        }
                        break;

                    case WEAPON_STATE_IDLE_TO_FIRE:
                        ss_anim->frame_time += time;
                        ss_anim->current_frame = (ss_anim->frame_time) / ss_anim->period;
                        dt = ss_anim->frame_time - (btScalar)ss_anim->current_frame * ss_anim->period;
                        ss_anim->lerp = dt / ss_anim->period;
                        t = ss_anim->model->animations[ss_anim->current_animation].frames_count;

                        if(ss_anim->current_frame < t - 1)
                        {
                            ss_anim->next_frame = ss_anim->current_frame + 1;
                            ss_anim->next_animation = ss_anim->current_animation;
                        }
                        else if(ss_anim->current_frame < t)
                        {
                            ss_anim->next_frame = 0;
                            ss_anim->next_animation = 2;
                        }
                        else if(entity->character->cmd.action)
                        {
                            ss_anim->current_frame = 0;
                            ss_anim->next_frame = 1;
                            ss_anim->current_animation = 2;
                            ss_anim->next_animation = ss_anim->current_animation;
                            entity->character->weapon_current_state = WEAPON_STATE_FIRE;
                        }
                        else
                        {
                            ss_anim->frame_time = 0.0;
                            ss_anim->current_frame = ss_anim->model->animations[ss_anim->current_animation].frames_count - 1;
                            entity->character->weapon_current_state = WEAPON_STATE_FIRE_TO_IDLE;
                        }
                        break;

                    case WEAPON_STATE_FIRE:
                        if(entity->character->cmd.action)
                        {
                            // inc time, loop;
                            ss_anim->frame_time += time;
                            ss_anim->current_frame = (ss_anim->frame_time) / ss_anim->period;
                            dt = ss_anim->frame_time - (btScalar)ss_anim->current_frame * ss_anim->period;
                            ss_anim->lerp = dt / ss_anim->period;
                            t = ss_anim->model->animations[ss_anim->current_animation].frames_count;

                            if(ss_anim->current_frame < t - 1)
                            {
                                ss_anim->next_frame = ss_anim->current_frame + 1;
                                ss_anim->next_animation = ss_anim->current_animation;
                            }
                            else if(ss_anim->current_frame < t)
                            {
                                ss_anim->next_frame = 0;
                                ss_anim->next_animation = ss_anim->current_animation;
                            }
                            else
                            {
                                ss_anim->frame_time = dt;
                                ss_anim->current_frame = 0;
                                ss_anim->next_frame = 1;
                            }
                        }
                        else
                        {
                            ss_anim->frame_time = 0.0;
                            ss_anim->current_animation = 0;
                            ss_anim->next_animation = ss_anim->current_animation;
                            ss_anim->current_frame = ss_anim->model->animations[ss_anim->current_animation].frames_count - 1;
                            ss_anim->next_frame = (ss_anim->current_frame > 0)?(ss_anim->current_frame - 1):(0);
                            entity->character->weapon_current_state = WEAPON_STATE_FIRE_TO_IDLE;
                        }
                        break;

                    case WEAPON_STATE_IDLE_TO_HIDE:
                        t = ss_anim->model->animations[ss_anim->current_animation].frames_count;
                        ss_anim->frame_time += time;
                        ss_anim->current_frame = (ss_anim->frame_time) / ss_anim->period;
                        dt = ss_anim->frame_time - (btScalar)ss_anim->current_frame * ss_anim->period;
                        ss_anim->lerp = dt / ss_anim->period;
                        if(ss_anim->current_frame < t - 1)
                        {
                            ss_anim->next_frame = ss_anim->current_frame + 1;
                            ss_anim->next_animation = ss_anim->current_animation;
                        }
                        else
                        {
                            ss_anim->next_frame = ss_anim->current_frame = 0;
                            ss_anim->next_animation = ss_anim->current_animation;
                            entity->character->weapon_current_state = WEAPON_STATE_HIDE;
                            Character_SetWeaponModel(entity, entity->character->current_weapon, 0);
                        }
                        break;
                };
            }
            else if((ss_anim->model != NULL) && (ss_anim->model->animation_count == 4))
            {
                switch(entity->character->weapon_current_state)
                {
                    case WEAPON_STATE_HIDE:
                        if(entity->character->cmd.ready_weapon)   // ready weapon
                        {
                            ss_anim->current_animation = 2;
                            ss_anim->next_animation = 2;
                            ss_anim->current_frame = 0;
                            ss_anim->next_frame = 0;
                            ss_anim->frame_time = 0.0;
                            entity->character->weapon_current_state = WEAPON_STATE_HIDE_TO_READY;
                        }
                        break;

                    case WEAPON_STATE_HIDE_TO_READY:
                        ss_anim->frame_time += time;
                        ss_anim->current_frame = (ss_anim->frame_time) / ss_anim->period;
                        dt = ss_anim->frame_time - (btScalar)ss_anim->current_frame * ss_anim->period;
                        ss_anim->lerp = dt / ss_anim->period;
                        t = ss_anim->model->animations[ss_anim->current_animation].frames_count;

                        if(ss_anim->current_frame < t - 1)
                        {
                            ss_anim->next_frame = (ss_anim->current_frame + 1) % t;
                            ss_anim->next_animation = ss_anim->current_animation;
                        }
                        else if(ss_anim->current_frame < t)
                        {
                            ss_anim->next_frame = 0;
                            ss_anim->next_animation = 0;
                        }
                        else
                        {
                            ss_anim->current_frame = 0;
                            ss_anim->current_animation = 0;
                            ss_anim->next_frame = 0;
                            ss_anim->next_animation = 0;
                            ss_anim->frame_time = 0.0;
                            entity->character->weapon_current_state = WEAPON_STATE_IDLE;
                        }
                        break;

                    case WEAPON_STATE_IDLE:
                        ss_anim->current_frame = 0;
                        ss_anim->current_animation = 0;
                        ss_anim->next_frame = 0;
                        ss_anim->next_animation = 0;
                        ss_anim->frame_time = 0.0;
                        if(entity->character->cmd.ready_weapon)
                        {
                            ss_anim->current_animation = 2;
                            ss_anim->next_animation = 2;
                            ss_anim->current_frame = ss_anim->next_frame = ss_anim->model->animations[ss_anim->current_animation].frames_count - 1;
                            ss_anim->frame_time = 0.0;
                            entity->character->weapon_current_state = WEAPON_STATE_IDLE_TO_HIDE;
                        }
                        else if(entity->character->cmd.action)
                        {
                            entity->character->weapon_current_state = WEAPON_STATE_IDLE_TO_FIRE;
                        }
                        else
                        {
                            // do nothing here, may be;
                        }
                        break;

                    case WEAPON_STATE_FIRE_TO_IDLE:
                        // Yes, same animation, reverse frames order;
                        t = ss_anim->model->animations[ss_anim->current_animation].frames_count;
                        ss_anim->frame_time += time;
                        ss_anim->current_frame = (ss_anim->frame_time) / ss_anim->period;
                        dt = ss_anim->frame_time - (btScalar)ss_anim->current_frame * ss_anim->period;
                        ss_anim->lerp = dt / ss_anim->period;
                        ss_anim->current_frame = t - 1 - ss_anim->current_frame;
                        if(ss_anim->current_frame > 0)
                        {
                            ss_anim->next_frame = ss_anim->current_frame - 1;
                            ss_anim->next_animation = ss_anim->current_animation;
                        }
                        else
                        {
                            ss_anim->next_frame = ss_anim->current_frame = 0;
                            ss_anim->next_animation = ss_anim->current_animation;
                            entity->character->weapon_current_state = WEAPON_STATE_IDLE;
                        }
                        break;

                    case WEAPON_STATE_IDLE_TO_FIRE:
                        ss_anim->frame_time += time;
                        ss_anim->current_frame = (ss_anim->frame_time) / ss_anim->period;
                        dt = ss_anim->frame_time - (btScalar)ss_anim->current_frame * ss_anim->period;
                        ss_anim->lerp = dt / ss_anim->period;
                        t = ss_anim->model->animations[ss_anim->current_animation].frames_count;

                        if(ss_anim->current_frame < t - 1)
                        {
                            ss_anim->next_frame = ss_anim->current_frame + 1;
                            ss_anim->next_animation = ss_anim->current_animation;
                        }
                        else if(ss_anim->current_frame < t)
                        {
                            ss_anim->next_frame = 0;
                            ss_anim->next_animation = 3;
                        }
                        else if(entity->character->cmd.action)
                        {
                            ss_anim->current_frame = 0;
                            ss_anim->next_frame = 1;
                            ss_anim->current_animation = 3;
                            ss_anim->next_animation = ss_anim->current_animation;
                            entity->character->weapon_current_state = WEAPON_STATE_FIRE;
                        }
                        else
                        {
                            ss_anim->frame_time = 0.0;
                            ss_anim->current_frame = ss_anim->model->animations[ss_anim->current_animation].frames_count - 1;
                            entity->character->weapon_current_state = WEAPON_STATE_FIRE_TO_IDLE;
                        }
                        break;

                    case WEAPON_STATE_FIRE:
                        if(entity->character->cmd.action)
                        {
                            // inc time, loop;
                            ss_anim->frame_time += time;
                            ss_anim->current_frame = (ss_anim->frame_time) / ss_anim->period;
                            dt = ss_anim->frame_time - (btScalar)ss_anim->current_frame * ss_anim->period;
                            ss_anim->lerp = dt / ss_anim->period;
                            t = ss_anim->model->animations[ss_anim->current_animation].frames_count;

                            if(ss_anim->current_frame < t - 1)
                            {
                                ss_anim->next_frame = ss_anim->current_frame + 1;
                                ss_anim->next_animation = ss_anim->current_animation;
                            }
                            else if(ss_anim->current_frame < t)
                            {
                                ss_anim->next_frame = 0;
                                ss_anim->next_animation = ss_anim->current_animation;
                            }
                            else
                            {
                                ss_anim->frame_time = dt;
                                ss_anim->current_frame = 0;
                                ss_anim->next_frame = 1;
                            }
                        }
                        else
                        {
                            ss_anim->frame_time = 0.0;
                            ss_anim->current_animation = 0;
                            ss_anim->next_animation = ss_anim->current_animation;
                            ss_anim->current_frame = ss_anim->model->animations[ss_anim->current_animation].frames_count - 1;
                            ss_anim->next_frame = (ss_anim->current_frame > 0)?(ss_anim->current_frame - 1):(0);
                            entity->character->weapon_current_state = WEAPON_STATE_FIRE_TO_IDLE;
                        }
                        break;

                    case WEAPON_STATE_IDLE_TO_HIDE:
                        // Yes, same animation, reverse frames order;
                        t = ss_anim->model->animations[ss_anim->current_animation].frames_count;
                        ss_anim->frame_time += time;
                        ss_anim->current_frame = (ss_anim->frame_time) / ss_anim->period;
                        dt = ss_anim->frame_time - (btScalar)ss_anim->current_frame * ss_anim->period;
                        ss_anim->lerp = dt / ss_anim->period;
                        ss_anim->current_frame = t - 1 - ss_anim->current_frame;
                        if(ss_anim->current_frame > 0)
                        {
                            ss_anim->next_frame = ss_anim->current_frame - 1;
                            ss_anim->next_animation = ss_anim->current_animation;
                        }
                        else
                        {
                            ss_anim->next_frame = ss_anim->current_frame = 0;
                            ss_anim->next_animation = ss_anim->current_animation;
                            entity->character->weapon_current_state = WEAPON_STATE_HIDE;
                            Character_SetWeaponModel(entity, entity->character->current_weapon, 0);
                        }
                        break;
                };
            }

            Entity_DoAnimCommands(entity, ss_anim, 0);
        }
    }
}


Entity::Entity()
    : id(0)
    , move_type(MOVE_ON_FLOOR)
    , state_flags( ENTITY_STATE_ENABLED | ENTITY_STATE_ACTIVE | ENTITY_STATE_VISIBLE )
    , type_flags( ENTITY_TYPE_GENERIC )
    , callback_flags( 0 ) // no callbacks by default
    , OCB( 0 )
    , trigger_layout( 0x00 )
    , timer( 0.0 )
    , self( new engine_container_t )
    , obb( new obb_s() )
    , character( NULL )
    , current_sector( NULL )
    , activation_offset{ 0.0, 256.0, 0.0 }
{
    transform.setIdentity();
    self->next = NULL;
    self->object = shared_from_this();
    self->object_type = OBJECT_ENTITY;
    self->room = NULL;
    self->collide_flag = 0;
    obb->transform = &transform;
    bt.bt_body = NULL;
    bt.bt_joints = NULL;
    bt.bt_joint_count = 0;
    bt.no_fix_all = 0x00;
    bt.no_fix_body_parts = 0x00000000;
    bt.manifoldArray = NULL;
    bt.shapes = NULL;
    bt.ghostObjects = NULL;
    bt.last_collisions = NULL;

    bf.animations.model = NULL;
    bf.animations.onFrame = NULL;
    bf.animations.frame_time = 0.0;
    bf.animations.last_state = 0;
    bf.animations.next_state = 0;
    bf.animations.lerp = 0.0;
    bf.animations.current_animation = 0;
    bf.animations.current_frame = 0;
    bf.animations.next_animation = 0;
    bf.animations.next_frame = 0;
    bf.animations.next = NULL;
    bf.bone_tag_count = 0;
    bf.bone_tags = 0;
    bf.bb_max.setZero();
    bf.bb_min.setZero();
    bf.centre.setZero();
    bf.pos.setZero();
    speed.setZero();
}

Entity::~Entity() {
    if((self->room != NULL) && (this != engine_world.Character.get()))
    {
        Room_RemoveEntity(self->room, std::static_pointer_cast<Entity>(shared_from_this()));
    }

    if(bt.last_collisions)
    {
        free(bt.last_collisions);
        bt.last_collisions = NULL;
    }

    if(bt.bt_joint_count)
    {
        Ragdoll_Delete(std::static_pointer_cast<Entity>(shared_from_this()));
    }

    if(bt.ghostObjects)
    {
        for(int i=0;i<bf.bone_tag_count;i++)
        {
            bt.ghostObjects[i]->setUserPointer(NULL);
            bt_engine_dynamicsWorld->removeCollisionObject(bt.ghostObjects[i]);
            delete bt.ghostObjects[i];
            bt.ghostObjects[i] = NULL;
        }
        free(bt.ghostObjects);
        bt.ghostObjects = NULL;
    }

    if(bt.shapes)
    {
        for(uint16_t i=0;i<bf.bone_tag_count;i++)
        {
            delete bt.shapes[i];
        }
        free(bt.shapes);
        bt.shapes = NULL;
    }

    if(bt.manifoldArray)
    {
        bt.manifoldArray->clear();
        delete bt.manifoldArray;
        bt.manifoldArray = NULL;
    }

    if(character)
    {
        Character_Clean(std::static_pointer_cast<Entity>(shared_from_this()));
    }

    if(bt.bt_body)
    {
        for(int i=0;i<bf.bone_tag_count;i++)
        {
            btRigidBody *body = bt.bt_body[i];
            if(body)
            {
                body->setUserPointer(NULL);
                if(body->getMotionState())
                {
                    delete body->getMotionState();
                    body->setMotionState(NULL);
                }
                if(body->getCollisionShape())
                {
                    delete body->getCollisionShape();
                    body->setCollisionShape(NULL);
                }

                bt_engine_dynamicsWorld->removeRigidBody(body);
                delete body;
                bt.bt_body[i] = NULL;
            }
        }
        free(bt.bt_body);
        bt.bt_body = NULL;
    }

    delete self;
    self = NULL;

    if(bf.bone_tag_count)
    {
        free(bf.bone_tags);
        bf.bone_tags = NULL;
        bf.bone_tag_count = 0;
    }

    for(ss_animation_p ss_anim=bf.animations.next;ss_anim!=NULL;)
    {
        ss_animation_p ss_anim_next = ss_anim->next;
        ss_anim->next = NULL;
        free(ss_anim);
        ss_anim = ss_anim_next;
    }
    bf.animations.next = NULL;
}
