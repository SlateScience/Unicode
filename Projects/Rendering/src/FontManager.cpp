/*
 * THE UNICODE TEST SUITE FOR CINDER: https://github.com/arielm/Unicode
 * COPYRIGHT (C) 2013, ARIEL MALKA ALL RIGHTS RESERVED.
 *
 * THE FOLLOWING SOURCE-CODE IS DISTRIBUTED UNDER THE MODIFIED BSD LICENSE:
 * https://github.com/arielm/Unicode/blob/master/LICENSE.md
 */

#include "FontManager.h"

#include "chronotext/utils/Utils.h"

using namespace std;
using namespace ci;
using namespace chr;

enum
{
    PLATFORM_OSX,
    PLATFORM_WINDOW,
    PLATFORM_IOS,
    PLATFORM_ANDROID
};

const string PLATFORM_NAMES[4] = {"osx", "windows", "ios", "android"};

/*
 * FIXME: SHOULD BE DEFINED AT THE XML-LEVEL
 */
const string DEFAULT_FONT_NAME = "sans-serif";
const VirtualFont::Style DEFAULT_FONT_STYLE = VirtualFont::STYLE_REGULAR;

FontManager::FontManager()
:
ftHelper(make_shared<FreetypeHelper>()),
itemizer(langHelper)
{
#if defined(CINDER_MAC)
    platform = PLATFORM_OSX;
#elif defined(CINDER_MSW)
    platform = PLATFORM_WINDOW;
#elif defined(CINDER_COCOA_TOUCH)
    platform = PLATFORM_IOS;
#elif defined(CINDER_ANDROID)
    platform = PLATFORM_ANDROID;
#else
    assert(false);
#endif
}

void FontManager::loadGlobalMap(InputSourceRef source)
{
    XmlTree doc(source->loadDataSource()); // EARLY-THROW IF THE DOCUMENT IS MALFORMED
    
    /*
     * THE FOLLOWING IS NOT SUPPOSED TO THROW...
     * IN THE WORST-CASE: THE MAP WILL BE EMPTY OR PARTIAL
     */
    if (doc.hasChild("GlobalMap"))
    {
        for (auto fontElement : doc.getChild("GlobalMap"))
        {
            auto name = fontElement.getAttributeValue<string>("name");
            
            if (!name.empty())
            {
                auto style = VirtualFont::styleStringToEnum(fontElement.getAttributeValue<string>("style", "regular"));
                auto baseSize = fontElement.getAttributeValue<float>("base-size", 0);
                
                for (auto &refElement : fontElement.getChildren())
                {
                    auto os = refElement->getAttributeValue<string>("os", "");
                    
                    if (os == PLATFORM_NAMES[platform])
                    {
                        auto uri = refElement->getAttributeValue<string>("uri", "");
                        
                        if (!uri.empty())
                        {
                            globalMap[make_pair(name, style)] = make_pair(uri, baseSize);
                            break;
                        }
                    }
                }
            }
        }
    }
}

VirtualFont& FontManager::getFont(const std::string &name, VirtualFont::Style style, float baseSize)
{
    auto key = make_tuple(name, style, baseSize);
    auto it1 = shortcuts.find(key);
    
    if (it1 != shortcuts.end())
    {
        return *it1->second;
    }
    
    // ---
    
    auto it2 = globalMap.find(make_pair(name, style));
    
    if (it2 != globalMap.end())
    {
        bool useMipmap = false;
        
        if (baseSize == 0)
        {
            baseSize = it2->second.second;
            useMipmap = true;
        }
        
        if (baseSize == 0)
        {
            throw invalid_argument("UNDEFINED FONT-SIZE");
        }
        
        auto uri = it2->second.first;
        auto &font = getFont(InputSource::get(uri), baseSize, useMipmap); // CAN THROW
        
        /*
         * ALLOWING CACHING UPON FURTHER ACCESS
         */
        shortcuts[key] = &font;
        
        return font;
    }
    
    /*
     * BASIC SYSTEM FOR HANDLING UNDEFINED FONT NAMES AND STYLES
     */
    if (name != DEFAULT_FONT_NAME)
    {
        auto &font = getFont(DEFAULT_FONT_NAME, get<1>(key), get<2>(key));
        shortcuts[key] = &font;
        return font;
    }
    else if (style != DEFAULT_FONT_STYLE)
    {
        auto &font = getFont(DEFAULT_FONT_NAME, DEFAULT_FONT_STYLE, get<2>(key));
        shortcuts[key] = &font;
        return font;
    }
    
    /*
     * SHOULD NOT OCCUR, UNLESS NO "DEFAULT FONT" IS DEFINED
     */
    throw invalid_argument(string("UNDEFINED FONT: ") + name + " " + VirtualFont::styleEnumToString(style));
}

VirtualFont& FontManager::getFont(InputSourceRef source, float baseSize, bool useMipmap)
{
    VirtualFont::Key key(source->getURI(), baseSize, useMipmap);
    auto it = virtualFonts.find(key);
    
    if (it != virtualFonts.end())
    {
        return *it->second.get();
    }
    else
    {
        XmlTree doc(source->loadDataSource()); // EARLY-THROW IF THE DOCUMENT IS MALFORMED
        
        /*
         * THE FOLLOWING IS NOT SUPPOSED TO THROW...
         * IN THE WORST-CASE: THE FONT WILL BE "EMPTY" OR "PARTIAL"
         *
         * TODO: THROW WHEN DOCUMENT IS INVALID
         */
        if (doc.hasChild("VirtualFont"))
        {
            auto font = new VirtualFont(layoutCache, itemizer, baseSize);
            virtualFonts[key] = unique_ptr<VirtualFont>(font);

            for (auto fontElement : doc.getChild("VirtualFont"))
            {
                auto langList = splitLanguages(fontElement.getAttributeValue<string>("lang", ""));
                
                for (auto lang : langList)
                {
                    for (auto &variantElement : fontElement.getChildren())
                    {
                        if (variantElement->getTag() == "Group")
                        {
                            for (auto &refElement : variantElement->getChildren())
                            {
                                auto descriptor = parseDescriptor(*refElement);
                                
                                if (!descriptor.empty() && font->add(lang, getActualFont(descriptor, baseSize, useMipmap)))
                                {
                                    break;
                                }
                            }
                        }
                        else
                        {
                            auto descriptor = parseDescriptor(*variantElement);
                            
                            if (!descriptor.empty())
                            {
                                font->add(lang, getActualFont(descriptor, baseSize, useMipmap));
                            }
                        }
                    }
                }
            }
            
            return *font;
        }
        
        throw invalid_argument("INVALID FONT-DEFINITION: " + source->getURI());
    }
}

void FontManager::reload()
{
    for (auto &it : actualFonts)
    {
        it.second->reload();
    }
}

void FontManager::unload()
{
    for (auto &it : actualFonts)
    {
        it.second->unload();
    }
}

void FontManager::discardTextures()
{
    for (auto &it : actualFonts)
    {
        it.second->discardTextures();
    }
}

size_t FontManager::getTextureMemoryUsage() const
{
    size_t total = 0;
    
    for (auto &it : actualFonts)
    {
        total += it.second->getTextureMemoryUsage();
    }
    
    return total;
}

ActualFont* FontManager::getActualFont(const ActualFont::Descriptor &descriptor, float baseSize, bool useMipmap)
{
    ActualFont::Key key(descriptor, baseSize, useMipmap);
    auto it = actualFonts.find(key);
    
    if (it != actualFonts.end())
    {
        return it->second.get();
    }
    else
    {
        try
        {
            auto font = new ActualFont(ftHelper, descriptor, baseSize, useMipmap);
            actualFonts[key] = unique_ptr<ActualFont>(font);
            
            return font;
        }
        catch (exception &e)
        {
            LOGD << e.what() << " - " << descriptor.source->getURI() << endl;
        }
        
        return NULL;
    }
}

vector<string> FontManager::splitLanguages(const string &languages)
{
	return split(languages, ":");
}

ActualFont::Descriptor FontManager::parseDescriptor(const XmlTree &element)
{
    auto uri = element.getAttributeValue<string>("uri", "");
    auto faceIndex = element.getAttributeValue<int>("face-index", 0);
    auto scale = element.getAttributeValue<float>("scale", 1);
    
    if (!uri.empty())
    {
        return ActualFont::Descriptor(InputSource::get(uri), faceIndex, scale);
    }
    else
    {
        return ActualFont::Descriptor();
    }
}