/*
 * THE UNICODE TEST SUITE FOR CINDER: https://github.com/arielm/Unicode
 * COPYRIGHT (C) 2013, ARIEL MALKA ALL RIGHTS RESERVED.
 *
 * THE FOLLOWING SOURCE-CODE IS DISTRIBUTED UNDER THE MODIFIED BSD LICENSE:
 * https://github.com/arielm/Unicode/blob/master/LICENSE.md
 */

/*
 * DONE:
 *
 * 1) MERGING OF SCRIPT/LANGUAGE AND BIDI ITEMS:
 *    SEEMS TO WORK AS INTENDED!
 *
 * 2) BUG-FIX:
 *    USING std::set FOR FontSet WAS NOT PRESERVING THE (CRUCIAL) ORDER OF INSERTION
 */

/*
 * TODO:
 *
 * 1) TEST ON iOS AND ANDROID
 *
 * 2) ADD SCALE-FACTOR FOR ACTUAL-FONTS IN XML DEFINITION:
 *    - NECESSARY IN CASE WE NEED TO MATCH SIZES BETWEEN (SMALLER) "Geeza Pro" AND "Arial"
 *
 * 3) ADAPT TEXT-LAYOUT-CACHE SYSTEM TO TextGroup
 */

#include "cinder/app/AppNative.h"
#include "cinder/Xml.h"
#include "cinder/Utilities.h"

#include "TextItemizer.h"
#include "FontManager.h"

#include <boost/algorithm/string.hpp>

using namespace std;
using namespace ci;
using namespace app;

const float FONT_SIZE = 27;
const float LINE_TOP = 66;
const float LINE_SPACING = 66;

class Application : public AppNative
{
    FontManager fontManager;
    TextItemizer itemizer;
    VirtualFont *font;
    vector<unique_ptr<TextLayout>> lineLayouts;
    
public:
    void prepareSettings(Settings *settings);
    void setup();
    
    void draw();
    void drawLineLayout(TextLayout &layout, float y, float left, float right);
    void drawHLine(float y);
    
    void addLineLayout(const string &text, const string &langHint = "", hb_direction_t overallDirection = HB_DIRECTION_LTR);
    string trimText(const string &text) const;
};

void Application::prepareSettings(Settings *settings)
{
    settings->setWindowSize(1024, 736);
    settings->enableHighDensityDisplay();
    settings->disableFrameRate();
}

void Application::setup()
{
    font = fontManager.getVirtualFont("res://SansSerif-osx.xml", FONT_SIZE);
    
    // ---

    /*
     * TEXT EXAMPLES FROM THE ScriptDetector PROJECT
     */

    XmlTree doc(loadResource("Text.xml"));
    auto rootElement = doc.getChild("Text");
    
    for (auto &lineElement : rootElement.getChildren())
    {
        auto text = trimText(lineElement->getValue());
        auto language = lineElement->getAttributeValue<string>("lang", "");
        hb_direction_t direction = (lineElement->getAttributeValue<string>("dir", "") == "rtl") ? HB_DIRECTION_RTL : HB_DIRECTION_LTR;
        
        addLineLayout(text, language, direction);
    }

    /*
     * TEXT EXAMPLES FROM THE SimpleBIDI PROJECT
     */
    addLineLayout("The title is مفتاح معايير الويب in Arabic.");
    addLineLayout("The title is \"مفتاح معايير الويب!\u200f\" in Arabic.");
    addLineLayout("The names of these states in Arabic are مصر,‎ البحرين and الكويت respectively.");
    addLineLayout("W3C‏ (World Wide Web Consortium) מעביר את שירותי הארחה באירופה ל - ERCIM.", "", HB_DIRECTION_RTL);
    addLineLayout("The title says \"W3C פעילות הבינאום,\u200f\" in Hebrew.");
    addLineLayout("one two ثلاثة four خمسة");
    addLineLayout("one two ثلاثة 1234 خمسة");
    
    // ---
    
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);
    
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
}

void Application::draw()
{
    gl::clear(Color(1, 1, 0.95f), false);
    
    Vec2i windowSize = toPixels(getWindowSize());
    gl::setMatricesWindow(windowSize, true);
    
    // ---
    
    float y = LINE_TOP;
    float left = 24;
    float right = windowSize.x - 24;
    
    font->setSize(FONT_SIZE);
    
    for (auto &layout : lineLayouts)
    {
        drawLineLayout(*layout, y, left, right);
        y += LINE_SPACING;
    }
}

void Application::drawLineLayout(TextLayout &layout, float y, float left, float right)
{
    float x = (layout.overallDirection == HB_DIRECTION_LTR) ? left : (right - font->getAdvance(layout));
    Vec2f position(x, y);
    
    font->begin();
    
    for (auto cluster : layout.clusters)
    {
        font->drawCluster(cluster, position);
        position.x += font->getAdvance(cluster);
    }
    
    font->end();
    
    // ---
    
    glColor4f(0.5f, 0, 0, 0.075f);
    drawHLine(y);
}

void Application::drawHLine(float y)
{
    gl::drawLine(Vec2f(-9999, y), Vec2f(+9999, y));
}

void Application::addLineLayout(const string &text, const string &langHint, hb_direction_t overallDirection)
{
    lineLayouts.emplace_back(unique_ptr<TextLayout>(font->createTextLayout(itemizer.process(text, langHint, overallDirection))));
}

string Application::trimText(const string &text) const
{
    auto rawLines = split(text, '\n');
    
    for (auto line : rawLines)
    {
        auto trimmed = boost::algorithm::trim_copy(line);
        
        if (!trimmed.empty())
        {
            return trimmed;
        }
    }
    
    return "";
}

CINDER_APP_NATIVE(Application, RendererGl(RendererGl::AA_NONE))
