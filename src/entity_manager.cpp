#include <string>
#include <map>
#include <JsonBox.h>

#include "entity_manager.hpp"
#include "entity.hpp"
#include "tileset.hpp"
#include "game_map.hpp"
#include "creature.hpp"
#include "character.hpp"

EntityManager::EntityManager() {}
EntityManager::~EntityManager()
{
    for(auto& entity : mData)
    {
        delete entity.second;
    }
}

template<typename T>
void EntityManager::load(const std::string& filename)
{
    JsonBox::Value v;
    v.loadFromFile(filename);

    JsonBox::Object o = v.getObject();

    for(auto entity : o)
    {
        std::string key = entity.first;
        mData[key] = dynamic_cast<Entity*>(new T(key, entity.second, this));
    }
}

template<typename T>
T* EntityManager::getEntity(const std::string& id) const
{
    return dynamic_cast<T*>(mData.at(id));
}

template<> std::string entityToString<GameMap>() { return "gamemap"; }
template<> std::string entityToString<Tileset>() { return "tileset"; }
template<> std::string entityToString<Creature>() { return "creature"; }
template<> std::string entityToString<Character>() { return "character"; }

template void EntityManager::load<GameMap>(const std::string&);
template void EntityManager::load<Tileset>(const std::string&);
template void EntityManager::load<Creature>(const std::string&);
template void EntityManager::load<Character>(const std::string&);

template GameMap* EntityManager::getEntity<GameMap>(const std::string&) const;
template Tileset* EntityManager::getEntity<Tileset>(const std::string&) const;
template Creature* EntityManager::getEntity<Creature>(const std::string&) const;
template Character* EntityManager::getEntity<Character>(const std::string&) const;
