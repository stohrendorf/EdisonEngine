#include "Base.h"
#include "TileSet.h"
#include "Matrix.h"
#include "Scene.h"


namespace gameplay
{
    TileSet::TileSet()
        : Drawable()
        , _tiles(nullptr)
        , _tileWidth(0)
        , _tileHeight(0)
        , _rowCount(0)
        , _columnCount(0)
        , _width(0)
        , _height(0)
        , _batch(nullptr)
        , _opacity(1.0f)
        , _color(Vector4::one())
    {
    }


    TileSet::~TileSet()
    {
        SAFE_DELETE_ARRAY(_tiles);
        SAFE_DELETE(_batch);
    }


    // ReSharper disable once CppMemberFunctionMayBeConst
    void TileSet::setTileSource(unsigned int column, unsigned int row, const Vector2& source)
    {
        BOOST_ASSERT(column < _columnCount);
        BOOST_ASSERT(row < _rowCount);

        _tiles[row * _columnCount + column] = source;
    }


    // ReSharper disable once CppMemberFunctionMayBeConst
    void TileSet::getTileSource(unsigned int column, unsigned int row, Vector2* source)
    {
        BOOST_ASSERT(column < _columnCount);
        BOOST_ASSERT(row < _rowCount);
        BOOST_ASSERT(source);

        source->x = _tiles[row * _columnCount + column].x;
        source->y = _tiles[row * _columnCount + column].y;
    }


    float TileSet::getTileWidth() const
    {
        return _tileWidth;
    }


    float TileSet::getTileHeight() const
    {
        return _tileHeight;
    }


    unsigned int TileSet::getRowCount() const
    {
        return _rowCount;
    }


    unsigned int TileSet::getColumnCount() const
    {
        return _columnCount;
    }


    float TileSet::getWidth() const
    {
        return _width;
    }


    float TileSet::getHeight() const
    {
        return _height;
    }


    void TileSet::setOpacity(float opacity)
    {
        _opacity = opacity;
    }


    float TileSet::getOpacity() const
    {
        return _opacity;
    }


    void TileSet::setColor(const Vector4& color)
    {
        _color = color;
    }


    const Vector4& TileSet::getColor() const
    {
        return _color;
    }


    size_t TileSet::draw(bool /*wireframe*/)
    {
        // Apply scene camera projection and translation offsets
        Vector3 position = Vector3::zero();
        if( _node && _node->getScene() )
        {
            auto activeCamera = _node->getScene()->getActiveCamera();
            if( activeCamera )
            {
                auto cameraNode = _node->getScene()->getActiveCamera()->getNode();
                if( cameraNode )
                {
                    // Scene projection
                    Matrix projectionMatrix;
                    projectionMatrix = _node->getProjectionMatrix();
                    _batch->setProjectionMatrix(projectionMatrix);

                    position.x -= cameraNode->getTranslationWorld().x;
                    position.y -= cameraNode->getTranslationWorld().y;
                }
            }

            // Apply node translation offsets
            Vector3 translation = _node->getTranslationWorld();
            position.x += translation.x;
            position.y += translation.y;
            position.z += translation.z;
        }

        // Draw each cell in the tile set
        position.y += _tileHeight * (_rowCount - 1);
        float xStart = position.x;
        _batch->start();
        for( unsigned int row = 0; row < _rowCount; row++ )
        {
            for( unsigned int col = 0; col < _columnCount; col++ )
            {
                Vector2 scale = Vector2(_tileWidth, _tileHeight);

                // Negative values are skipped to allow blank tiles
                if( _tiles[row * _columnCount + col].x >= 0 &&
                    _tiles[row * _columnCount + col].y >= 0 )
                {
                    Rectangle source = Rectangle(_tiles[row * _columnCount + col].x,
                                                 _tiles[row * _columnCount + col].y, _tileWidth, _tileHeight);
                    _batch->draw(position, source, scale, Vector4(_color.x, _color.y, _color.z, _color.w * _opacity),
                                 Vector2(0.5f, 0.5f), 0);
                }

                position.x += _tileWidth;
            }
            position.x = xStart;
            position.y -= _tileHeight;
        }
        _batch->finish();
        return 1;
    }
}