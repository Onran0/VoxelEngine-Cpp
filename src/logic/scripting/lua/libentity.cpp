#include "libentity.hpp"

#include "../../../objects/Player.hpp"
#include "../../../objects/Entities.hpp"
#include "../../../objects/rigging.hpp"
#include "../../../physics/Hitbox.hpp"
#include "../../../window/Camera.hpp"
#include "../../../content/Content.hpp"
#include "../../../voxels/Chunks.hpp"
#include "../../../engine.hpp"

using namespace scripting;

static int l_exists(lua::State* L) {
    return lua::pushboolean(L, get_entity(L, 1).has_value());
}

static int l_spawn(lua::State* L) {
    auto level = controller->getLevel();
    auto defname = lua::tostring(L, 1);
    auto& def = content->entities.require(defname);
    auto pos = lua::tovec3(L, 2);
    dynamic::Map_sptr args = nullptr;
    if (lua::gettop(L) > 2) {
        auto value = lua::tovalue(L, 3);
        if (auto map = std::get_if<dynamic::Map_sptr>(&value)) {
            args = *map;
        }
    }
    level->entities->spawn(def, pos, args);
    return 1;
}

static int l_despawn(lua::State* L) {
    if (auto entity = get_entity(L, 1)) {
        entity->destroy();
    }
    return 0;
}

static int l_get_skeleton(lua::State* L) {
    if (auto entity = get_entity(L, 1)) {
        return lua::pushstring(L, entity->getSkeleton().config->getName());
    }
    return 0;
}

static int l_set_skeleton(lua::State* L) {
    if (auto entity = get_entity(L, 1)) {
        std::string skeletonName = lua::require_string(L, 2);
        auto rigConfig = content->getSkeleton(skeletonName);
        if (rigConfig == nullptr) {
            throw std::runtime_error("skeleton not found '"+skeletonName+"'");
        }
        entity->setRig(rigConfig);
    }
    return 0;
}

static int l_get_all_in_box(lua::State* L) {
    auto pos = lua::tovec<3>(L, 1);
    auto size = lua::tovec<3>(L, 2);
    auto found = level->entities->getAllInside(AABB(pos, pos + size));
    lua::createtable(L, found.size(), 0);
    for (size_t i = 0; i < found.size(); i++) {
        const auto& entity = found[i];
        lua::pushinteger(L, entity.getUID());
        lua::rawseti(L, i+1);
    }
    return 1;
}

static int l_get_all_in_radius(lua::State* L) {
    auto pos = lua::tovec<3>(L, 1);
    auto radius = lua::tonumber(L, 2);
    auto found = level->entities->getAllInRadius(pos, radius);
    lua::createtable(L, found.size(), 0);
    for (size_t i = 0; i < found.size(); i++) {
        const auto& entity = found[i];
        lua::pushinteger(L, entity.getUID());
        lua::rawseti(L, i+1);
    }
    return 1;  
}

static int l_raycast(lua::State* L) {
    auto start = lua::tovec<3>(L, 1);
    auto dir = lua::tovec<3>(L, 2);
    auto maxDistance = lua::tonumber(L, 3);
    auto ignore = lua::tointeger(L, 4);
    glm::vec3 end;
    glm::ivec3 normal;
    glm::ivec3 iend;

    blockid_t block = BLOCK_VOID;

    if (auto voxel = level->chunks->rayCast(start, dir, maxDistance, end, normal, iend)) {
        maxDistance = glm::distance(start, end);
        block = voxel->id;
    }
    if (auto ray = level->entities->rayCast(start, dir, maxDistance, ignore)) {
        if (lua::gettop(L) >= 5) {
            lua::pushvalue(L, 5);
        } else {
            lua::createtable(L, 0, 6);
        }
        
        lua::pushvec3_arr(L, start + dir * ray->distance);
        lua::setfield(L, "endpoint");

        lua::pushvec3_arr(L, ray->normal);
        lua::setfield(L, "normal");

        lua::pushnumber(L, glm::distance(start, end));
        lua::setfield(L, "length");

        lua::pushvec3_arr(L, iend);
        lua::setfield(L, "iendpoint");

        lua::pushinteger(L, block);
        lua::setfield(L, "block");

        lua::pushinteger(L, ray->entity);
        lua::setfield(L, "entity");
        return 1;
    } else if (block != BLOCK_VOID) {
        if (lua::gettop(L) >= 5) {
            lua::pushvalue(L, 5);
        } else {
            lua::createtable(L, 0, 5);
        }
        lua::pushvec3_arr(L, end);
        lua::setfield(L, "endpoint");

        lua::pushvec3_arr(L, normal);
        lua::setfield(L, "normal");

        lua::pushnumber(L, glm::distance(start, end));
        lua::setfield(L, "length");

        lua::pushvec3_arr(L, iend);
        lua::setfield(L, "iendpoint");

        lua::pushinteger(L, block);
        lua::setfield(L, "block");
        return 1;
    }
    return 0;
}

const luaL_Reg entitylib [] = {
    {"exists", lua::wrap<l_exists>},
    {"spawn", lua::wrap<l_spawn>},
    {"despawn", lua::wrap<l_despawn>},
    {"get_skeleton", lua::wrap<l_get_skeleton>},
    {"set_skeleton", lua::wrap<l_set_skeleton>},
    {"get_all_in_box", lua::wrap<l_get_all_in_box>},
    {"get_all_in_radius", lua::wrap<l_get_all_in_radius>},
    {"raycast", lua::wrap<l_raycast>},
    {NULL, NULL}
};
