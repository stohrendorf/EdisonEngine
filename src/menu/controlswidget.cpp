#include "controlswidget.h"

#include "core/i18n.h"
#include "hid/actions.h"
#include "hid/names.h"
#include "ui/widgets/gridbox.h"
#include "ui/widgets/groupbox.h"
#include "ui/widgets/label.h"

#include <optional>
#include <utility>

namespace menu
{
namespace
{
constexpr const size_t Columns = 3;

constexpr const std::array<std::array<std::optional<hid::Action>, 5>, Columns> gameplayActions{{
  {
    hid::Action::Forward,
    hid::Action::Backward,
    hid::Action::Left,
    hid::Action::Right,
    hid::Action::Jump,
  },
  {
    hid::Action::StepLeft,
    hid::Action::StepRight,
    hid::Action::Walk,
    hid::Action::Roll,
    hid::Action::FreeLook,
  },
  {
    hid::Action::Action,
    hid::Action::Holster,
    hid::Action::Menu,
    std::nullopt,
    std::nullopt,
  },
}};

constexpr const std::array<std::array<std::optional<hid::Action>, 4>, Columns> shortcutActions{{
  {
    hid::Action::DrawPistols,
    hid::Action::DrawShotgun,
    hid::Action::DrawUzis,
    hid::Action::DrawMagnums,
  },
  {
    hid::Action::ConsumeSmallMedipack,
    hid::Action::ConsumeLargeMedipack,
    std::nullopt,
    std::nullopt,
  },
  {
    hid::Action::Save,
    hid::Action::Load,
    hid::Action::Screenshot,
    std::nullopt,
  },
}};

static_assert(gameplayActions.size() == shortcutActions.size());
} // namespace

ControlsWidget::ControlsWidget(
  const engine::NamedInputMappingConfig& mappingConfig,
  std::function<std::shared_ptr<Widget>(const engine::InputMappingConfig&, hid::Action)> factory)
    : m_content{std::make_shared<ui::widgets::GridBox>()}
    , m_container{std::make_shared<ui::widgets::GroupBox>(mappingConfig.name, m_content)}
    , m_factory{std::move(factory)}
{
  m_content->setExtents(1, 2);

  auto gridBox = std::make_shared<ui::widgets::GridBox>(glm::ivec2{10, ui::OutlineBorderWidth});
  gridBox->setExtents(2 * gameplayActions.size(), gameplayActions[0].size());
  gridBox->setSelected({1, 0});
  m_controlGroups.emplace_back(gridBox);

  auto groupBox = std::make_shared<ui::widgets::GroupBox>(
    /* translators: TR charmap encoding */ _("Gameplay"), gridBox);
  m_content->set(0, 0, groupBox);

  gridBox = std::make_shared<ui::widgets::GridBox>(glm::ivec2{10, ui::OutlineBorderWidth});
  gridBox->setExtents(2 * shortcutActions.size(), shortcutActions[0].size());
  m_controlGroups.emplace_back(gridBox);

  groupBox = std::make_shared<ui::widgets::GroupBox>(/* translators: TR charmap encoding */ _("Shortcuts"), gridBox);
  m_content->set(0, 1, groupBox);

  updateBindings(mappingConfig);
}

void ControlsWidget::fitToContent()
{
  for(size_t x = 0; x < std::get<0>(m_content->getExtents()); ++x)
    for(size_t y = 0; y < std::get<1>(m_content->getExtents()); ++y)
      m_content->getWidget(x, y)->fitToContent();

  for(const auto& gridBox : m_controlGroups)
  {
    gridBox->fitToContent();
    gridBox->setColumnSize(1, gridBox->getColumnSizes()[1] + 2 * ui::FontHeight);
    gridBox->setColumnSize(3, gridBox->getColumnSizes()[3] + 2 * ui::FontHeight);
  }

  for(size_t x = 0; x < Columns * 2; ++x)
  {
    int maxWidth = 0;
    for(const auto& gridBox : m_controlGroups)
    {
      maxWidth = std::max(gridBox->getColumnSizes().at(x), maxWidth);
    }
    for(const auto& gridBox : m_controlGroups)
    {
      gridBox->setColumnSize(x, maxWidth);
      if(x % 2 == 0)
      {
        for(size_t y = 0; y < std::get<1>(gridBox->getExtents()); ++y)
        {
          if(const auto widget = gridBox->getWidget(x, y))
            widget->setSize({maxWidth, ui::FontHeight});
        }
      }
    }
  }

  m_content->fitToContent();
  m_container->fitToContent();
}

glm::ivec2 ControlsWidget::getPosition() const
{
  return m_container->getPosition();
}

glm::ivec2 ControlsWidget::getSize() const
{
  return m_container->getSize();
}

void ControlsWidget::setPosition(const glm::ivec2& position)
{
  m_container->setPosition(position);
}

void ControlsWidget::setSize(const glm::ivec2& size)
{
  m_container->setSize(size);
}

void ControlsWidget::update(bool hasFocus)
{
  m_container->update(hasFocus);
}

void ControlsWidget::draw(ui::Ui& ui, const engine::Presenter& presenter) const
{
  m_container->draw(ui, presenter);
}

const std::shared_ptr<ui::widgets::GridBox>& ControlsWidget::getCurrentGridBox() const
{
  return m_controlGroups.at(std::get<1>(m_content->getSelected()));
}

void ControlsWidget::nextRow()
{
  do
  {
    const auto& currentGridBox = getCurrentGridBox();
    if(!currentGridBox->nextRow())
    {
      if(!m_content->nextRow())
      {
        // wrap around
        m_content->setSelected({0, 0});
      }

      getCurrentGridBox()->setSelected({std::get<0>(currentGridBox->getSelected()), 0});
    }
  } while(getCurrentGridBox()->getSelectedWidget() == nullptr);
}

void ControlsWidget::prevRow()
{
  do
  {
    const auto& currentGridBox = getCurrentGridBox();
    if(!currentGridBox->prevRow())
    {
      if(!m_content->prevRow())
      {
        // wrap around
        m_content->setSelected({0, m_controlGroups.size() - 1});
      }

      getCurrentGridBox()->setSelected(
        {std::get<0>(currentGridBox->getSelected()), std::get<1>(getCurrentGridBox()->getExtents()) - 1});
    }
  } while(getCurrentGridBox()->getSelectedWidget() == nullptr);
}

void ControlsWidget::nextColumn()
{
  do
  {
    const auto& currentGridBox = getCurrentGridBox();
    if(!currentGridBox->nextColumn() || !currentGridBox->nextColumn())
    {
      getCurrentGridBox()->setSelected({1, std::get<1>(currentGridBox->getSelected())});
    }
  } while(getCurrentGridBox()->getSelectedWidget() == nullptr);
}

void ControlsWidget::prevColumn()
{
  do
  {
    const auto& currentGridBox = getCurrentGridBox();
    if(!currentGridBox->prevColumn() || !currentGridBox->prevColumn())
    {
      getCurrentGridBox()->setSelected(
        {std::get<0>(currentGridBox->getExtents()) - 1, std::get<1>(currentGridBox->getSelected())});
    }
  } while(getCurrentGridBox()->getSelectedWidget() == nullptr);
}

void ControlsWidget::updateBindings(const engine::NamedInputMappingConfig& mappingConfig)
{
  m_container->setTitle(mappingConfig.name);

  auto set = [this, &mappingConfig](ui::widgets::GridBox& gridBox, size_t x0, size_t y, hid::Action action)
  {
    auto label = std::make_shared<ui::widgets::Label>(hid::getName(action), ui::widgets::Label::Alignment::Right);
    label->fitToContent();
    gridBox.set(x0, y, label);

    auto widget = m_factory(mappingConfig.mappings, action);
    widget->fitToContent();
    gridBox.set(x0 + 1, y, widget);
  };

  for(size_t x = 0; x < gameplayActions.size(); ++x)
  {
    const auto& column = gameplayActions[x];
    for(size_t y = 0; y < column.size(); ++y)
    {
      const auto& action = column[y];
      if(action.has_value())
        set(*m_controlGroups[0], x * 2, y, action.value());
    }
  }

  for(size_t x = 0; x < shortcutActions.size(); ++x)
  {
    const auto& column = shortcutActions[x];
    for(size_t y = 0; y < column.size(); ++y)
    {
      const auto& action = column[y];
      if(action.has_value())
        set(*m_controlGroups[1], x * 2, y, action.value());
    }
  }

  fitToContent();
}

hid::Action ControlsWidget::getCurrentAction() const
{
  const auto getAction = [this](size_t n)
  {
    auto [x, y] = m_controlGroups.at(n)->getSelected();
    switch(n)
    {
    case 0: return gameplayActions.at(x / 2).at(y).value();
    case 1: return shortcutActions.at(x / 2).at(y).value();
    default: BOOST_THROW_EXCEPTION(std::runtime_error("Invalid control group"));
    }
  };

  return getAction(std::get<1>(m_content->getSelected()));
}
} // namespace menu
