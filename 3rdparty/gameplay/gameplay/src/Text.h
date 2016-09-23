#pragma once

#include "Properties.h"
#include "Font.h"
#include "Vector4.h"
#include "Drawable.h"


namespace gameplay
{
    /**
     * Defines a text block of characters to be drawn.
     *
     * Text can be attached to a node.
     */
    class Text : public Drawable
    {
        friend class Node;

    public:

        /**
         * Sets the text to be drawn.
         *
         * @param str The text string to be drawn.
         */
        void setText(const char* str);

        /**
         * Get the string that will be drawn from this Text object.
         *
         * @return The text string to be drawn.
         */
        const char* getText() const;

        /**
         * Gets the size of the text to be drawn.
         *
         * @return The size of the text to be drawn.
         */
        unsigned int getSize() const;

        /**
         * Set the width to draw the text within.
         *
         * @param width The width to draw the text.
         */
        void setWidth(float width);

        /**
         * Gets the width of the text.
         *
         * @return The width of the text.
         */
        float getWidth() const;

        /**
         * Set the height of text to be drawn within.
         *
         * @param height The height to draw the text.
         */
        void setHeight(float height);

        /**
         * Gets the width of the text.
         *
         * @return The overall width of the text.
         */
        float getHeight() const;

        /**
         * Sets if the the text is wrapped by the text width.
         *
         * @param wrap true if the the text is wrapped by the text width.
         */
        void setWrap(bool wrap);

        /**
         * Gets if the the text is wrapped by the text width.
         *
         * Default is true.
         *
         * @return true if the the text is wrapped by the text width.
         */
        bool getWrap() const;

        /**
         * Sets the justification to align the text within the text bounds.
         *
         * @param align The text justification alignment.
         */
        void setJustify(Font::Justify align);

        /**
         * Gets the justification to align the text within the text bounds.
         *
         * @return The text justification alignment.
         */
        Font::Justify getJustify() const;

        /**
         * Sets the local clipping region for this text.
         *
         * This is used for clipping unwanted regions of text.
         *
         * @param clip The clipping region for this text.
         */
        void setClip(const Rectangle& clip);

        /**
         * Gets the local clipping region for this text.
         *
         * This is used for clipping unwanted regions of text.
         *
         * Default is Rectangle(0, 0, 0, 0) which means no clipping region is applied.
         *
         * @return clip The clipping region for this text.
         */
        const Rectangle& getClip() const;

        /**
         * Sets the opacity for the sprite.
         *
         * The range is from full transparent to opaque [0.0,1.0].
         *
         * @param opacity The opacity for the sprite.
         */
        void setOpacity(float opacity);

        /**
         * Gets the opacity for the sprite.
         *
         * The range is from full transparent to opaque [0.0,1.0].
         *
         * @return The opacity for the sprite.
         */
        float getOpacity() const;

        /**
         * Sets the color (RGBA) for the sprite.
         *
         * @param color The color(RGBA) for the sprite.
         */
        void setColor(const Vector4& color);

        /**
         * Gets the color (RGBA) for the sprite.
         *
         * @return The color(RGBA) for the sprite.
         */
        const Vector4& getColor() const;

        /**
         * @see Drawable::draw
         */
        size_t draw(bool wireframe = false) override;

    protected:

        /**
         * Constructor
         */
        Text();

        /**
         * Destructor
         */
        ~Text();

        /**
         * operator=
         */
        Text& operator=(const Text& text) = delete;

    private:

        std::shared_ptr<Font> _font;
        std::string _text;
        unsigned int _size;
        float _width;
        float _height;
        bool _wrap;
        Font::Justify _align;
        Rectangle _clip;
        float _opacity;
        Vector4 _color;
    };
}