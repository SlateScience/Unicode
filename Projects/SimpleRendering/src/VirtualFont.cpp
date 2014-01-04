/*
 * THE UNICODE TEST SUITE FOR CINDER: https://github.com/arielm/Unicode
 * COPYRIGHT (C) 2013, ARIEL MALKA ALL RIGHTS RESERVED.
 *
 * THE FOLLOWING SOURCE-CODE IS DISTRIBUTED UNDER THE MODIFIED BSD LICENSE:
 * https://github.com/arielm/Unicode/blob/master/LICENSE.md
 */

#include "VirtualFont.h"
#include "TextLayout.h"

using namespace std;
using namespace ci;

VirtualFont::VirtualFont(float baseSize)
:
baseSize(baseSize)
{
    setSize(12);
    setColor(ColorA(1, 1, 1, 1));
}

bool VirtualFont::add(const string &lang, ActualFont *font)
{
    if (font)
    {
        fontSetMap[lang].insert(font);
        return true;
    }
    
    return false;
}

const FontSet& VirtualFont::getFontSet(const string &lang) const
{
    auto it = fontSetMap.find(lang);
    
    if (it == fontSetMap.end())
    {
        it = fontSetMap.find("");
        
        if (it == fontSetMap.end())
        {
            throw;
        }
    }
    
    return it->second;
}

const ActualFont::Metrics& VirtualFont::getMetrics(const string &lang) const
{
    auto it = fontSetMap.find(lang);
    
    if (it == fontSetMap.end())
    {
        it = fontSetMap.find("");
        
        if (it == fontSetMap.end())
        {
            throw;
        }
    }
    
    return (*it->second.begin())->metrics;
}

TextLayout* VirtualFont::createTextLayout(const TextRun &run)
{
    auto layout = new TextLayout(this, run.lang, run.direction);
    
    map<uint32_t, Cluster> clusterMap;
    auto buffer = hb_buffer_create();
    
    for (auto font : getFontSet(run.lang))
    {
        hb_buffer_clear_contents(buffer);
        
        run.apply(buffer);
        hb_shape(font->hbFont, buffer, NULL, 0);
        
        auto glyphCount = hb_buffer_get_length(buffer);
        auto glyphInfos = hb_buffer_get_glyph_infos(buffer, NULL);
        auto glyphPositions = hb_buffer_get_glyph_positions(buffer, NULL);
        
        bool hasMissingGlyphs = false;
        
        for (int i = 0; i < glyphCount; i++)
        {
            auto codepoint = glyphInfos[i].codepoint;
            auto cluster = glyphInfos[i].cluster;
            
            auto it = clusterMap.find(cluster);
            bool clusterFound = (it != clusterMap.end());
            
            if (codepoint)
            {
                if (clusterFound && (it->second.font != font))
                {
                    continue; // CLUSTER FOUND, WITH ANOTHER FONT (E.G. SPACE)
                }
                else
                {
                    auto offset = Vec2f(glyphPositions[i].x_offset, -glyphPositions[i].y_offset) * font->scale;
                    float advance = glyphPositions[i].x_advance * font->scale.x;
                    
                    if (clusterFound)
                    {
                        it->second.addShape(codepoint, offset, advance);
                    }
                    else
                    {
                        clusterMap.insert(make_pair(cluster, Cluster(font, codepoint, offset, advance)));
                    }
                }
            }
            else if (!clusterFound)
            {
                hasMissingGlyphs = true;
            }
        }
        
        if (!hasMissingGlyphs)
        {
            break; // NO NEED TO PROCEED TO THE NEXT FONT IN THE LIST
        }
    }
    
    if (run.direction == HB_DIRECTION_RTL)
    {
        for (auto it = clusterMap.rbegin(); it != clusterMap.rend(); ++it)
        {
            layout->addCluster(it->second);
        }
    }
    else
    {
        for (auto it = clusterMap.begin(); it != clusterMap.end(); ++it)
        {
            layout->addCluster(it->second);
        }
    }
    
    hb_buffer_destroy(buffer);
    return layout;
}

void VirtualFont::setSize(float newSize)
{
    size = newSize;
    sizeRatio = newSize / baseSize;
}

void VirtualFont::setColor(const ColorA &color)
{
    quad.colors[0] = quad.colors[1] = quad.colors[2] = quad.colors[3] = color;
}

float VirtualFont::getAdvance(const Cluster &cluster) const
{
    return cluster.combinedAdvance * sizeRatio;
}

float VirtualFont::getAdvance(const TextLayout &layout) const
{
    return layout.advance * sizeRatio;
}

void VirtualFont::begin()
{
    glEnable(GL_TEXTURE_2D);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
}

void VirtualFont::end()
{
    glDisable(GL_TEXTURE_2D);
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
}

void VirtualFont::drawCluster(const Cluster &cluster, const Vec2f &position)
{
    glVertexPointer(2, GL_FLOAT, 0, quad.vertices);
    glTexCoordPointer(2, GL_FLOAT, 0, quad.coords);
    glColorPointer(4, GL_FLOAT, 0, quad.colors);
    
    for (auto shape : cluster.shapes)
    {
        auto glyph = cluster.font->getGlyph(shape.codepoint);
        
        if (glyph)
        {
            auto ul = position + (shape.position + glyph->offset) * sizeRatio;
            auto lr = ul + glyph->size * sizeRatio;
            
            quad.vertices[0][0] = ul.x; quad.vertices[0][1] = ul.y;
            quad.vertices[1][0] = lr.x; quad.vertices[1][1] = ul.y;
            quad.vertices[2][0] = lr.x; quad.vertices[2][1] = lr.y;
            quad.vertices[3][0] = ul.x; quad.vertices[3][1] = lr.y;
            
            quad.coords[0][0] = glyph->tx1; quad.coords[0][1] = glyph->ty1;
            quad.coords[1][0] = glyph->tx2; quad.coords[1][1] = glyph->ty1;
            quad.coords[2][0] = glyph->tx2; quad.coords[2][1] = glyph->ty2;
            quad.coords[3][0] = glyph->tx1; quad.coords[3][1] = glyph->ty2;
            
            glyph->texture->bind();
            glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        }
    }
}
