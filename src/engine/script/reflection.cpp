#include "reflection.h"

#include "core/i18n.h"
#include "core/pybindmodule.h"
#include "engine/cameracontroller.h"
#include "engine/engine.h"
#include "engine/objects/modelobject.h"
#include "engine/player.h"
#include "engine/presenter.h"
#include "engine/world/world.h"
#include "loader/file/level/level.h"

#include <boost/range/adaptors.hpp>

namespace engine::script
{
namespace
{
std::filesystem::path getLocalLevelPath(const std::string& basename)
{
  return std::filesystem::path{"data"} / "tr1" / "DATA" / (basename + ".PHD");
}

std::unique_ptr<loader::file::level::Level>
  loadLevel(Engine& engine, const std::string& basename, const std::string& title)
{
  engine.getPresenter().drawLoadingScreen(_("Loading %1%", title));
  auto level = loader::file::level::Level::createLoader(engine.getRootPath() / getLocalLevelPath(basename),
                                                        loader::file::level::Game::Unknown);
  level->loadFileData();
  return level;
}
} // namespace

std::pair<RunResult, std::optional<size_t>> Video::run(Engine& engine, const std::shared_ptr<Player>& /*player*/)
{
  engine.getPresenter().playVideo(engine.getRootPath() / "data" / "tr1" / "FMV" / m_name);
  return {RunResult::NextLevel, std::nullopt};
}

std::pair<RunResult, std::optional<size_t>> Cutscene::run(Engine& engine, const std::shared_ptr<Player>& player)
{
  auto world
    = std::make_unique<world::World>(engine,
                                     loadLevel(engine, m_name, m_name),
                                     std::string{},
                                     0,
                                     m_track,
                                     false,
                                     std::unordered_map<std::string, std::unordered_map<TR1ItemId, std::string>>{},
                                     player);

  world->getCameraController().setEyeRotation(0_deg, m_cameraRot);
  auto pos = world->getCameraController().getTRLocation().position;
  if(m_cameraPosX.has_value())
    pos.X = m_cameraPosX.value();
  if(m_cameraPosZ.has_value())
    pos.Z = m_cameraPosZ.value();

  world->getCameraController().setPosition(pos);

  if(m_flipRooms)
    world->swapAllRooms();

  if(m_weaponSwap)
  {
    const auto& laraPistol = world->findAnimatedModelForType(TR1ItemId::LaraPistolsAnim);
    Expects(laraPistol != nullptr);
    for(const auto& object : world->getObjectManager().getObjects() | boost::adaptors::map_values)
    {
      if(object->m_state.type != TR1ItemId::CutsceneActor1)
        continue;

      auto m = std::dynamic_pointer_cast<objects::ModelObject>(object.get());
      Expects(m != nullptr);
      m->getSkeleton()->setMeshPart(1, laraPistol->bones[1].mesh);
      m->getSkeleton()->setMeshPart(4, laraPistol->bones[4].mesh);
      m->getSkeleton()->rebuildMesh();
    }
  }

  return engine.run(*world, true, false);
}

std::unique_ptr<world::World> Level::loadWorld(Engine& engine, const std::shared_ptr<Player>& player)
{
  engine.getPresenter().debounceInput();

  auto titleIt = m_titles.find(engine.getLanguageWithoutEncoding());
  if(titleIt == m_titles.end())
  {
    BOOST_LOG_TRIVIAL(warning) << "Missing level title translation for language " << engine.getLanguageWithoutEncoding()
                               << ", falling back to language en_GB";
    titleIt = m_titles.find("en_GB");
  }
  if(titleIt == m_titles.end())
    BOOST_LOG_TRIVIAL(error) << "Missing level title";

  const auto title = titleIt == m_titles.end() ? "NO TRANSLATION - " + m_name : titleIt->second;

  player->resetStats();
  for(const auto& [type, qty] : m_inventory)
    player->getInventory().put(type, qty);
  for(const auto& type : m_dropInventory)
    player->getInventory().drop(type);

  if(const auto tbl = core::get<pybind11::dict>(
       core::get<pybind11::dict>(pybind11::globals(), "cheats").value_or(pybind11::dict{}), "inventory"))
  {
    for(const auto& [type, qty] : *tbl)
      player->getInventory().put(type.cast<TR1ItemId>(), qty.cast<size_t>());
  }

  return std::make_unique<world::World>(engine,
                                        loadLevel(engine, m_name, util::unescape(title)),
                                        title,
                                        m_secrets,
                                        m_track,
                                        m_useAlternativeLara,
                                        m_itemTitles,
                                        player);
}

bool Level::isLevel(const std::filesystem::path& path) const
{
  try
  {
    return std::filesystem::equivalent(getLocalLevelPath(m_name), path);
  }
  catch(std::error_code& ec)
  {
    ec.clear();
    return false;
  }
}

std::pair<RunResult, std::optional<size_t>> Level::run(Engine& engine, const std::shared_ptr<Player>& player)
{
  player->requestedWeaponType = m_defaultWeapon;
  player->selectedWeaponType = m_defaultWeapon;
  auto world = loadWorld(engine, player);
  return engine.run(*world, false, m_allowSave);
}

std::pair<RunResult, std::optional<size_t>>
  Level::runFromSave(Engine& engine, const std::optional<size_t>& slot, const std::shared_ptr<Player>& player)
{
  Expects(m_allowSave);
  player->getInventory().clear();
  auto world = loadWorld(engine, player);
  world->load(slot);
  return engine.run(*world, false, m_allowSave);
}

std::pair<RunResult, std::optional<size_t>> TitleMenu::run(Engine& engine, const std::shared_ptr<Player>& player)
{
  player->getInventory().clear();
  auto world = loadWorld(engine, player);
  return engine.runTitleMenu(*world);
}

TitleMenu::TitleMenu(const std::string& name,
                     bool useAlternativeLara,
                     const std::unordered_map<std::string, std::string>& titles,
                     const std::unordered_map<std::string, std::unordered_map<TR1ItemId, std::string>>& itemTitles,
                     const std::unordered_map<TR1ItemId, size_t>& inventory,
                     const std::unordered_set<TR1ItemId>& dropInventory,
                     std::optional<TR1TrackId> track)
    : Level{name, 0, useAlternativeLara, titles, itemTitles, inventory, dropInventory, track, false, WeaponType::None}
{
}
} // namespace engine::script
