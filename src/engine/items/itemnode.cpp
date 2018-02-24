#include "itemnode.h"

#include "engine/laranode.h"
#include "level/level.h"

#include <boost/range/adaptor/indexed.hpp>


namespace engine
{
    namespace items
    {
        namespace
        {
            struct SpriteVertex
            {
                glm::vec3 pos;

                glm::vec2 uv;

                glm::vec3 color{1.0f};
            };


            std::shared_ptr<gameplay::Mesh> createSpriteMesh(const loader::Sprite& sprite,
                                                             const std::shared_ptr<gameplay::Material>& material)
            {
                const SpriteVertex vertices[]{
                    {{sprite.left_side, sprite.top_side, 0}, {sprite.t0.x, sprite.t1.y}},
                    {{sprite.right_side, sprite.top_side, 0}, {sprite.t1.x, sprite.t1.y}},
                    {{sprite.right_side, sprite.bottom_side, 0}, {sprite.t1.x, sprite.t0.y}},
                    {{sprite.left_side, sprite.bottom_side, 0}, {sprite.t0.x, sprite.t0.y}}
                };

                gameplay::gl::StructuredVertexBuffer::AttributeMapping attribs{
                    {VERTEX_ATTRIBUTE_POSITION_NAME, gameplay::gl::VertexAttribute{&SpriteVertex::pos}},
                    {VERTEX_ATTRIBUTE_TEXCOORD_PREFIX_NAME, gameplay::gl::VertexAttribute{&SpriteVertex::uv}},
                    {VERTEX_ATTRIBUTE_COLOR_NAME, gameplay::gl::VertexAttribute{&SpriteVertex::color}}
                };

                auto mesh = std::make_shared<gameplay::Mesh>(attribs, false);
                mesh->getBuffer(0)->assign<SpriteVertex>(vertices, 4);

                static const uint16_t indices[6] =
                {
                    0, 1, 2,
                    0, 2, 3
                };

                gameplay::gl::VertexArrayBuilder builder;

                auto indexBuffer = std::make_shared<gameplay::gl::IndexBuffer>();
                indexBuffer->setData(indices, 6, false);
                builder.attach(indexBuffer);
                builder.attach(mesh->getBuffers());

                auto part = std::make_shared<gameplay::MeshPart>(
                    builder.build(material->getShaderProgram()->getHandle()));
                mesh->addPart(part);
                part->setMaterial(material);

                return mesh;
            }
        }


        void ItemNode::applyTransform()
        {
            glm::vec3 tr;

            if( const auto parent = m_state.position.room )
            {
                tr = m_state.position.position.toRenderSystem() - parent->position.toRenderSystem();
            }
            else
            {
                tr = m_state.position.position.toRenderSystem();
            }

            getNode()->setLocalMatrix(translate(glm::mat4{1.0f}, tr) * m_state.rotation.toMatrix());

            updateSounds();
        }

        ItemNode::ItemNode(const gsl::not_null<level::Level*>& level,
                           const gsl::not_null<const loader::Room*>& room,
                           const loader::Item& item,
                           const bool hasProcessAnimCommandsOverride,
                           const Characteristics characteristics)
            : m_level{level}
            , m_hasProcessAnimCommandsOverride{hasProcessAnimCommandsOverride}
            , m_characteristics{characteristics}
        {
            BOOST_ASSERT( room->isInnerPositionXZ( item.position ) );

            m_state.object_number = item.type;
            m_state.position.position = item.position;
            m_state.position.room = room;
            m_state.rotation.Y = core::Angle{item.rotation};
            m_state.shade = item.darkness;
            m_state.activationState = floordata::ActivationState(item.activationState);
            m_state.timer = floordata::ActivationState::extractTimeout(item.activationState);

            if( m_state.activationState.isOneshot() )
            {
                m_state.activationState.setOneshot(false);
                m_state.triggerState = TriggerState::Locked;
            }

            if( m_state.activationState.isFullyActivated() )
            {
                m_state.activationState.fullyDeactivate();
                m_state.activationState.setInverted(true);
                activate();
                m_state.triggerState = TriggerState::Active;
            }
        }

        void ItemNode::setCurrentRoom(const loader::Room* newRoom)
        {
            if( newRoom == m_state.position.room )
            {
                return;
            }

            if( newRoom == nullptr )
            {
                BOOST_LOG_TRIVIAL( fatal ) << "No room to switch to.";
                return;
            }
            BOOST_LOG_TRIVIAL( debug ) << "Room switch of " << getNode()->getId() << " to " << newRoom->node->getId();

            newRoom->node->addChild(getNode());

            m_state.position.room = newRoom;
        }

        ModelItemNode::ModelItemNode(const gsl::not_null<level::Level*>& level,
                                     const std::string& name,
                                     const gsl::not_null<const loader::Room*>& room,
                                     const loader::Item& item,
                                     const bool hasProcessAnimCommandsOverride,
                                     const Characteristics characteristics,
                                     const loader::SkeletalModelType& animatedModel)
            : ItemNode{
                level,
                room,
                item,
                hasProcessAnimCommandsOverride,
                characteristics
            }
            , m_skeleton{std::make_shared<SkeletalModelNode>(name, level, animatedModel)}
        {
            m_skeleton->setAnimIdGlobal(m_state,
                                        animatedModel.anim_index,
                                        getLevel().m_animations.at(animatedModel.anim_index).firstFrame);
        }

        void ModelItemNode::update()
        {
            const auto endOfAnim = m_skeleton->advanceFrame(m_state);

            m_state.is_hit = false;

            if( endOfAnim )
            {
                const loader::Animation& animation = m_skeleton->getCurrentAnimData(m_state);
                BOOST_ASSERT(
                    animation.animCommandCount == 0 || animation.animCommandIndex < getLevel().m_animCommands.size() );
                const auto* cmd = animation.animCommandCount == 0
                                      ? nullptr
                                      : &getLevel().m_animCommands[animation
                                          .animCommandIndex];
                for( uint16_t i = 0; i < animation.animCommandCount; ++i )
                {
                    BOOST_ASSERT( cmd < &getLevel().m_animCommands.back() );
                    const auto opcode = static_cast<AnimCommandOpcode>(*cmd);
                    ++cmd;
                    switch( opcode )
                    {
                        case AnimCommandOpcode::SetPosition:
                            moveLocal(
                                cmd[0],
                                cmd[1],
                                cmd[2]
                            );
                            cmd += 3;
                            break;
                        case AnimCommandOpcode::StartFalling:
                        {
                            m_state.fallspeed = cmd[0];
                        }
                            m_state.speed = cmd[1];
                            m_state.falling = true;
                            cmd += 2;
                            break;
                        case AnimCommandOpcode::PlaySound:
                            cmd += 2;
                            break;
                        case AnimCommandOpcode::PlayEffect:
                            cmd += 2;
                            break;
                        case AnimCommandOpcode::Kill:
                            m_state.triggerState = TriggerState::Deactivated;
                            break;
                        default:
                            break;
                    }
                }

                const loader::Animation& currentAnim = m_skeleton->getCurrentAnimData(m_state);
                m_skeleton->setAnimIdGlobal(m_state, currentAnim.nextAnimation, currentAnim.nextFrame);
                m_state.goal_anim_state = m_state.current_anim_state;
            }

            const loader::Animation& animation = m_skeleton->getCurrentAnimData(m_state);
            BOOST_ASSERT( animation.animCommandCount == 0 || animation.animCommandIndex < getLevel().m_animCommands.size
                () );
            const auto* cmd = animation.animCommandCount == 0
                                  ? nullptr
                                  : &getLevel().m_animCommands[animation
                                      .animCommandIndex];
            for( uint16_t i = 0; i < animation.animCommandCount; ++i )
            {
                BOOST_ASSERT( cmd < &getLevel().m_animCommands.back() );
                const auto opcode = static_cast<AnimCommandOpcode>(*cmd);
                ++cmd;
                switch( opcode )
                {
                    case AnimCommandOpcode::SetPosition:
                        cmd += 3;
                        break;
                    case AnimCommandOpcode::StartFalling:
                        cmd += 2;
                        break;
                    case AnimCommandOpcode::PlaySound:
                        if( m_state.frame_number == cmd[0] )
                        {
                            playSoundEffect(cmd[1]);
                        }
                        cmd += 2;
                        break;
                    case AnimCommandOpcode::PlayEffect:
                        if( m_state.frame_number == cmd[0] )
                        {
                            BOOST_LOG_TRIVIAL( debug ) << "Anim effect: " << int(cmd[1]);
                            if( cmd[1] == 0 )
                            {
                                m_state.rotation.Y += 180_deg;
                            }
                            else if( cmd[1] == 12 )
                            {
                                getLevel().m_lara->setHandStatus(HandStatus::None);
                            }
                            //! @todo Execute anim effect cmd[1]
                        }
                        cmd += 2;
                        break;
                    default:
                        break;
                }
            }

            applyMovement(false);
        }

        void ItemNode::activate()
        {
            if( !m_hasProcessAnimCommandsOverride )
            {
                m_state.triggerState = TriggerState::Inactive;
                return;
            }

            if( m_isActive )
            {
                //BOOST_LOG_TRIVIAL(warning) << "Item controller " << getId() << " already active";
            }
            else
            {
                BOOST_LOG_TRIVIAL( trace ) << "Activating item controller";
            }

            m_isActive = true;
        }

        void ItemNode::deactivate()
        {
            if( !m_isActive )
            {
                //BOOST_LOG_TRIVIAL(warning) << "Item controller " << getId() << " already inactive";
            }
            else
            {
                BOOST_LOG_TRIVIAL( trace ) << "Deactivating item controller " << getNode()->getId();
            }

            m_isActive = false;
        }

        std::shared_ptr<audio::SourceHandle> ItemNode::playSoundEffect(const int id)
        {
            auto handle = getLevel().playSound(id, getNode()->getTranslationWorld());
            if( handle != nullptr )
            {
                m_sounds.emplace(handle);
            }
            return handle;
        }

        bool ItemNode::triggerKey()
        {
            if( getLevel().m_lara->getHandStatus() != HandStatus::None )
            {
                return false;
            }

            if( m_state.triggerState != TriggerState::Active )
            {
                return false;
            }

            m_state.triggerState = TriggerState::Deactivated;
            return true;
        }

        void ItemNode::updateSounds()
        {
            decltype( m_sounds ) cleaned;
            std::copy_if(m_sounds.begin(), m_sounds.end(), inserter(cleaned, cleaned.end()),
                         [](const std::weak_ptr<audio::SourceHandle>& h)
                         {
                             return h.expired();
                         });

            m_sounds = std::move(cleaned);

            for( const std::weak_ptr<audio::SourceHandle>& handle : m_sounds )
            {
                std::shared_ptr<audio::SourceHandle> lockedHandle = handle.lock();
                lockedHandle->setPosition(getNode()->getTranslationWorld());
            }
        }

        bool InteractionLimits::canInteract(const ItemNode& item, const LaraNode& lara) const
        {
            const auto angle = lara.m_state.rotation - item.m_state.rotation;
            if( angle.X < minAngle.X || angle.X > maxAngle.X
                || angle.Y < minAngle.Y || angle.Y > maxAngle.Y
                || angle.Z < minAngle.Z || angle.Z > maxAngle.Z )
            {
                return false;
            }

            const auto dist =
                glm::vec4{(lara.m_state.position.position - item.m_state.position.position).toRenderSystem(), 1.0f}
                * item.m_state.rotation.toMatrix();
            const glm::vec3 tdist{dist};

            return distance.contains(core::TRCoordinates{tdist});
        }

        void ModelItemNode::applyMovement(const bool forLara)
        {
            if( m_state.falling )
            {
                if( m_state.fallspeed >= 128 )
                {
                    m_state.fallspeed += 1;
                }
                else
                {
                    m_state.fallspeed += 6;
                }

                if( forLara )
                {
                    // we only add accelleration here
                    m_state.speed = m_state.speed + m_skeleton->calculateFloorSpeed(m_state, 0)
                                    - m_skeleton->calculateFloorSpeed(m_state, -1);
                }
            }
            else
            {
                m_state.speed = m_skeleton->calculateFloorSpeed(m_state);
            }

            move(
                getMovementAngle().sin() * m_state.speed,
                m_state.falling ? m_state.fallspeed : 0,
                getMovementAngle().cos() * m_state.speed
            );

            applyTransform();

            m_skeleton->updatePose(m_state);
            updateLighting();
        }

        loader::BoundingBox ModelItemNode::getBoundingBox() const
        {
            return m_skeleton->getBoundingBox(m_state);
        }

        SpriteItemNode::SpriteItemNode(const gsl::not_null<level::Level*>& level,
                                       const std::string& name,
                                       const gsl::not_null<const loader::Room*>& room,
                                       const loader::Item& item,
                                       const bool hasProcessAnimCommandsOverride,
                                       const Characteristics characteristics,
                                       const loader::Sprite& sprite,
                                       const std::shared_ptr<gameplay::Material>& material,
                                       const std::vector<std::shared_ptr<gameplay::gl::Texture>>& textures)
            : ItemNode{
                level,
                room,
                item,
                hasProcessAnimCommandsOverride,
                characteristics
            }
        {
            const auto spriteMesh = createSpriteMesh(sprite, material);
            auto model = std::make_shared<gameplay::Model>();
            model->addMesh(spriteMesh);

            m_node = std::make_shared<gameplay::Node>(name);
            m_node->setDrawable(model);
            m_node->addMaterialParameterSetter("u_diffuseTexture",
                                               [texture = textures[sprite.texture]](const gameplay::Node& /*node*/,
                                                                                    gameplay::gl::Program::ActiveUniform
                                                                                    & uniform)
                                               {
                                                   uniform.set(*texture);
                                               });
            m_node->addMaterialParameterSetter("u_baseLight", [darkness = item.darkness](const gameplay::Node& /*node*/,
                                                                                         gameplay::gl::Program::
                                                                                         ActiveUniform& uniform)
            {
                uniform.set((8192 - darkness) / 32.0f);
            });
        }

        bool ModelItemNode::isNear(const ModelItemNode& other, const int radius) const
        {
            const auto aFrame = getSkeleton()->getInterpolationInfo(m_state).getNearestFrame();
            const auto bFrame = other.getSkeleton()->getInterpolationInfo(m_state).getNearestFrame();
            if( other.m_state.position.position.Y + bFrame->bbox.minY >= aFrame->bbox.maxY + m_state.position.position.Y
                || m_state.position.position.Y + aFrame->bbox.minY >= other.m_state.position.position.Y + bFrame
                                                                                                          ->bbox.maxY )
            {
                return false;
            }

            const auto c = m_state.rotation.Y.cos();
            const auto s = m_state.rotation.Y.sin();
            const auto dx = other.m_state.position.position.X - m_state.position.position.X;
            const auto dz = other.m_state.position.position.Z - m_state.position.position.Z;
            const auto x = c * dx - s * dz;
            const auto z = s * dx + c * dz;
            return x >= aFrame->bbox.minX - radius
                   && x <= aFrame->bbox.maxX + radius
                   && z >= aFrame->bbox.minZ - radius
                   && z <= aFrame->bbox.maxZ + radius;
        }

        void ModelItemNode::enemyPush(LaraNode& other, CollisionInfo& collisionInfo, bool enableSpaz,
                                      bool withXZCollRadius)
        {
            const auto dx = other.m_state.position.position.X - m_state.position.position.X;
            const auto dz = other.m_state.position.position.Z - m_state.position.position.Z;
            const auto c = m_state.rotation.Y.cos();
            const auto s = m_state.rotation.Y.sin();
            auto posX = c * dx - s * dz;
            auto posZ = s * dx + c * dz;
            const auto itemKeyFrame = m_skeleton->getInterpolationInfo(m_state).getNearestFrame();
            auto itemBBoxMinX = itemKeyFrame->bbox.minX;
            auto itemBBoxMaxX = itemKeyFrame->bbox.maxX;
            auto itemBBoxMaxZ = itemKeyFrame->bbox.maxZ;
            auto itemBBoxMinZ = itemKeyFrame->bbox.minZ;
            if( withXZCollRadius )
            {
                const auto r = collisionInfo.collisionRadius;
                itemBBoxMinX -= r;
                itemBBoxMaxX += r;
                itemBBoxMinZ -= r;
                itemBBoxMaxZ += r;
            }
            if( posX >= itemBBoxMinX && posX <= itemBBoxMaxX && posZ >= itemBBoxMinZ && posZ <= itemBBoxMaxZ )
            {
                const auto dxMin = posX - itemBBoxMinX;
                const auto dxMax = itemBBoxMaxX - posX;
                const auto dzMin = posZ - itemBBoxMinZ;
                const auto dzMax = itemBBoxMaxZ - posZ;
                if( posX - itemBBoxMinX <= itemBBoxMaxX - posX && dxMin <= dzMax && dxMin <= dzMin )
                {
                    posX -= dxMin;
                }
                else if( dxMin >= dxMax && dxMax <= dzMax && dxMax <= dzMin )
                {
                    posX = itemBBoxMaxX;
                }
                else if( dxMin < dzMax || dzMax > dxMax || dzMax > dzMin )
                {
                    posZ -= dzMin;
                }
                else
                {
                    posZ = itemBBoxMaxZ;
                }
                other.m_state.position.position.X = ((c * posX + s * posZ)) + m_state.position.position.X;
                other.m_state.position.position.Z = ((c * posZ - s * posX)) + m_state.position.position.Z;
                if( enableSpaz )
                {
                    const auto midX = (itemKeyFrame->bbox.minX + itemKeyFrame->bbox.maxX) / 2;
                    const auto midZ = (itemKeyFrame->bbox.minZ + itemKeyFrame->bbox.maxZ) / 2;
                    const auto a = core::Angle::fromAtan(dx - ((midX * c + midZ * s)), dz - ((midZ * c - midX * s)))
                                   - 180_deg;
                    getLevel().m_lara->hit_direction = axisFromAngle(other.m_state.rotation.Y - a, 45_deg).get();
                    if( getLevel().m_lara->hit_frame == 0 )
                    {
                        other.playSoundEffect(27);
                    }
                    if( ++getLevel().m_lara->hit_frame > 34 )
                    {
                        getLevel().m_lara->hit_frame = 34;
                    }
                }
                collisionInfo.badPositiveDistance = 32512;
                collisionInfo.badNegativeDistance = -384;
                collisionInfo.badCeilingDistance = 0;
                const auto facingAngle = collisionInfo.facingAngle;
                collisionInfo.facingAngle = core::Angle::fromAtan(
                    other.m_state.position.position.X - collisionInfo.oldPosition.X,
                    other.m_state.position.position.Z - collisionInfo.oldPosition.Z);
                collisionInfo.initHeightInfo(other.m_state.position.position, getLevel(), 762);
                collisionInfo.facingAngle = facingAngle;
                if( collisionInfo.collisionType != CollisionInfo::AxisColl_None )
                {
                    other.m_state.position.position.X = collisionInfo.oldPosition.X;
                    other.m_state.position.position.Z = collisionInfo.oldPosition.Z;
                }
                else
                {
                    collisionInfo.oldPosition = other.m_state.position.position;
                    other.updateFloorHeight(-10);
                }
            }
        }

        bool ModelItemNode::testBoneCollision(const ModelItemNode& other)
        {
            m_state.touch_bits = 0;
            const auto itemCyls = m_skeleton->getBoneCollisionCylinders(
                m_state,
                *m_skeleton->getInterpolationInfo(m_state).getNearestFrame(),
                nullptr);
            const auto laraCyls = other.m_skeleton->getBoneCollisionCylinders(
                other.m_state,
                *other.m_skeleton->getInterpolationInfo(other.m_state).getNearestFrame(),
                nullptr);
            for( const auto& itemCyl : itemCyls | boost::adaptors::indexed(0) )
            {
                if( itemCyl.value().radius <= 0 )
                    continue;

                for( const auto& laraCyl : laraCyls )
                {
                    if( laraCyl.radius <= 0 )
                        continue;
                    if( laraCyl.position.distanceTo(itemCyl.value().position)
                        >= util::square(itemCyl.value().radius + laraCyl.radius) )
                        continue;

                    m_state.touch_bits |= 1 << itemCyl.index();
                    break;
                }
            }

            return m_state.touch_bits != 0;
        }

        bool ItemState::stalkBox(const level::Level& lvl, const loader::Box* box) const
        {
            Expects( box != nullptr );

            const auto laraToBoxDistX = (box->xmin + box->xmax) / 2 - lvl.m_lara->m_state.position.position.X;
            const auto laraToBoxDistZ = (box->zmin + box->zmax) / 2 - lvl.m_lara->m_state.position.position.Z;

            if( laraToBoxDistX > 3 * loader::SectorSize || laraToBoxDistX < -3 * loader::SectorSize
                || laraToBoxDistZ > 3 * loader::SectorSize ||
                laraToBoxDistZ < -3 * loader::SectorSize )
            {
                return false;
            }

            const auto laraAxisBack = *axisFromAngle(lvl.m_lara->m_state.rotation.Y + 180_deg, 45_deg);
            core::Axis laraToBoxAxis;
            if( laraToBoxDistZ > 0 )
            {
                if( laraToBoxDistX > 0 )
                {
                    laraToBoxAxis = core::Axis::PosX;
                }
                else
                {
                    laraToBoxAxis = core::Axis::NegZ;
                }
            }
            else if( laraToBoxDistX > 0 )
            {
                // Z <= 0, X > 0
                laraToBoxAxis = core::Axis::NegX;
            }
            else
            {
                // Z <= 0, X <= 0
                laraToBoxAxis = core::Axis::PosZ;
            }

            if( laraAxisBack == laraToBoxAxis )
            {
                return false;
            }

            core::Axis itemToLaraAxis;
            if( position.position.Z <= lvl.m_lara->m_state.position.position.Z )
            {
                if( position.position.X <= lvl.m_lara->m_state.position.position.X )
                {
                    itemToLaraAxis = core::Axis::PosZ;
                }
                else
                {
                    itemToLaraAxis = core::Axis::NegX;
                }
            }
            else
            {
                if( position.position.X > lvl.m_lara->m_state.position.position.X )
                {
                    itemToLaraAxis = core::Axis::PosX;
                }
                else
                {
                    itemToLaraAxis = core::Axis::NegZ;
                }
            }

            if( laraAxisBack != itemToLaraAxis )
            {
                return true;
            }

            switch( laraAxisBack )
            {
                case core::Axis::PosZ:
                    return laraToBoxAxis == core::Axis::NegZ;
                case core::Axis::PosX:
                    return laraToBoxAxis == core::Axis::NegX;
                case core::Axis::NegZ:
                    return laraToBoxAxis == core::Axis::PosZ;
                case core::Axis::NegX:
                    return laraToBoxAxis == core::Axis::PosX;
            }

            BOOST_THROW_EXCEPTION( std::runtime_error( "Unreachable code reached" ) );
        }

        bool ItemState::isInsideZoneButNotInBox(const level::Level& lvl, const int16_t zoneId,
                                                const loader::Box* box) const
        {
            Expects( creatureInfo != nullptr );
            Expects( box != nullptr );

            const auto zoneRef = loader::Box::getZoneRef(lvl.roomsAreSwapped, creatureInfo->lot.fly,
                                                         creatureInfo->lot.step);

            if( zoneId != box->*zoneRef )
            {
                return false;
            }

            if( creatureInfo->lot.block_mask & box->overlap_index )
            {
                return false;
            }

            if( position.position.Z > box->zmin && position.position.Z < box->zmax )
            {
                if( position.position.X > box->xmin && position.position.X < box->xmax )
                {
                    return false;
                }
            }

            return true;
        }

        bool ItemState::inSameQuadrantAsBoxRelativeToLara(const level::Level& lvl, const loader::Box* box) const
        {
            Expects( box != nullptr );
            const auto laraToBoxX = (box->xmin + box->xmax) / 2 - lvl.m_lara->m_state.position.position.X;
            const auto laraToBoxZ = (box->zmin + box->zmax) / 2 - lvl.m_lara->m_state.position.position.Z;
            if( laraToBoxX <= -5 * loader::SectorSize
                || laraToBoxX >= 5 * loader::SectorSize
                || laraToBoxZ <= -5 * loader::SectorSize
                || laraToBoxZ >= 5 * loader::SectorSize )
            {
                const auto laraToNpcX = position.position.X - lvl.m_lara->m_state.position.position.X;
                const auto laraToNpcZ = position.position.Z - lvl.m_lara->m_state.position.position.Z;
                return ((laraToNpcZ > 0) == (laraToBoxZ > 0))
                       || ((laraToNpcX > 0) == (laraToBoxX > 0));
            }

            return false;
        }

        void ItemState::initCreatureInfo(const level::Level& lvl)
        {
            if( creatureInfo != nullptr )
                return;

            creatureInfo = std::make_shared<ai::CreatureInfo>(lvl, this);
            collectZoneBoxes(lvl);
        }

        void ItemState::collectZoneBoxes(const level::Level& lvl)
        {
            const auto zoneRef1 = loader::Box::getZoneRef(false, creatureInfo->lot.fly, creatureInfo->lot.step);
            const auto zoneRef2 = loader::Box::getZoneRef(true, creatureInfo->lot.fly, creatureInfo->lot.step);

            box_number = position.room->getInnerSectorByAbsolutePosition(position.position)->box;
            const auto zoneData1 = box_number->*zoneRef1;
            const auto zoneData2 = box_number->*zoneRef2;
            creatureInfo->lot.boxes.clear();
            for( const auto& box : lvl.m_boxes )
            {
                if( box.*zoneRef1 == zoneData1 || box.*zoneRef2 == zoneData2 )
                {
                    creatureInfo->lot.boxes.emplace_back(&box);
                }
            }
        }

        sol::usertype<ItemState>& ItemState::userType()
        {
            static auto type = sol::usertype<ItemState>(
                sol::meta_function::construct, sol::no_constructor,
                "position", [](ItemState& self) { return self.position.position; },
                "rotation", [](ItemState& self) { return self.rotation; },
                "current_anim_state", sol::readonly(&ItemState::current_anim_state),
                "goal_anim_state", &ItemState::goal_anim_state,
                "required_anim_state", &ItemState::required_anim_state,
                "activation_state", &ItemState::triggerState,
                "patch_heights", [](ItemState& self, int delta) { /* TODO */ },
                "health", &ItemState::health,
                "do_enemy_push", []() { /* TODO */ },
                "set_y_angle", [](ItemState& self, int16_t angle) { self.rotation.Y = core::Angle(angle); },
                "set_collidable", [](ItemState& self, bool flag) { self.collidable = flag; },
                "frame_number", &ItemState::frame_number,
                "enable_ai", &ItemState::initCreatureInfo,
                "creature_info", sol::readonly(&ItemState::creatureInfo)
            );
            return type;
        }

        ItemState::~ItemState() = default;
    }
}
