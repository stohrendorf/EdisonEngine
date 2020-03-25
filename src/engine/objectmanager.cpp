#include "objectmanager.h"

#include "loader/file/item.h"
#include "objects/laraobject.h"
#include "objects/objectfactory.h"
#include "particle.h"
#include "serialization/map.h"
#include "serialization/not_null.h"

namespace engine
{
std::shared_ptr<objects::LaraObject> ObjectManager::createObjects(Engine& engine,
                                                                  std::vector<loader::file::Item>& items)
{
  Expects(m_objectCounter == ObjectId(-1));
  m_objectCounter = gsl::narrow<ObjectId>(items.size());

  std::shared_ptr<objects::LaraObject> lara = nullptr;
  ObjectId id = -1;
  for(auto& item : items)
  {
    ++id;

    auto object = objects::createObject(engine, item);
    if(item.type == TR1ItemId::Lara)
    {
      lara = std::dynamic_pointer_cast<objects::LaraObject>(object);
      Expects(lara != nullptr);
    }

    if(object != nullptr)
    {
      m_objects.emplace(std::make_pair(id, object));
    }
  }

  return lara;
}

void ObjectManager::applyScheduledDeletions()
{
  if(m_scheduledDeletions.empty())
    return;

  for(const auto& del : m_scheduledDeletions)
  {
    auto it = std::find_if(m_dynamicObjects.begin(),
                           m_dynamicObjects.end(),
                           [del](const std::shared_ptr<objects::Object>& i) { return i.get() == del; });
    if(it != m_dynamicObjects.end())
    {
      m_dynamicObjects.erase(it);
      continue;
    }

    auto it2 = std::find_if(m_objects.begin(),
                            m_objects.end(),
                            [del](const std::pair<uint16_t, gsl::not_null<std::shared_ptr<objects::Object>>>& i) {
                              return i.second.get().get() == del;
                            });
    if(it2 != m_objects.end())
    {
      m_objects.erase(it2);
      continue;
    }
  }

  m_scheduledDeletions.clear();
}

void ObjectManager::registerObject(const gsl::not_null<std::shared_ptr<objects::Object>>& object)
{
  if(m_objectCounter == std::numeric_limits<uint16_t>::max())
    BOOST_THROW_EXCEPTION(std::runtime_error("Artificial object counter exceeded"));

  m_objects.emplace(m_objectCounter++, object);
}

std::shared_ptr<objects::Object> ObjectManager::find(const objects::Object* object) const
{
  if(object == nullptr)
    return nullptr;

  auto it = std::find_if(m_objects.begin(),
                         m_objects.end(),
                         [object](const std::pair<uint16_t, gsl::not_null<std::shared_ptr<objects::Object>>>& x) {
                           return x.second.get().get() == object;
                         });

  if(it == m_objects.end())
    return nullptr;

  return it->second;
}

std::shared_ptr<objects::Object> ObjectManager::getObject(ObjectId id) const
{
  const auto it = m_objects.find(id);
  if(it == m_objects.end())
    return nullptr;

  return it->second.get();
}

void ObjectManager::update(Engine& engine)
{
  for(const auto& object : m_objects | boost::adaptors::map_values)
  {
    if(object.get() == engine.getLaraPtr()) // Lara is special and needs to be updated last
      continue;

    if(object->m_isActive)
      object->update();

    object->getNode()->setVisible(object->m_state.triggerState != objects::TriggerState::Invisible);
  }

  for(const auto& object : m_dynamicObjects)
  {
    if(object->m_isActive)
      object->update();

    object->getNode()->setVisible(object->m_state.triggerState != objects::TriggerState::Invisible);
  }

  auto currentParticles = std::move(m_particles);
  for(const auto& particle : currentParticles)
  {
    if(particle->update(engine))
    {
      setParent(particle, particle->pos.room->node);
      particle->updateLight();
      m_particles.emplace_back(particle);
    }
    else
    {
      setParent(particle, nullptr);
    }
  }
}

void ObjectManager::serialize(const serialization::Serializer& ser)
{
  ser(S_NV("objectCounter", m_objectCounter), S_NV("objects", m_objects));
}

void ObjectManager::eraseParticle(const std::shared_ptr<Particle>& particle)
{
  if(particle == nullptr)
    return;

  const auto it
    = std::find_if(m_particles.begin(), m_particles.end(), [particle](const auto& p) { return particle == p.get(); });
  if(it != m_particles.end())
    m_particles.erase(it);

  setParent(particle, nullptr);
}
} // namespace engine
