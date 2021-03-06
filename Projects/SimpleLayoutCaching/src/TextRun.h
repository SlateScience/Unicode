/*
 * THE UNICODE TEST SUITE FOR CINDER: https://github.com/arielm/Unicode
 * COPYRIGHT (C) 2013, ARIEL MALKA ALL RIGHTS RESERVED.
 *
 * THE FOLLOWING SOURCE-CODE IS DISTRIBUTED UNDER THE MODIFIED BSD LICENSE:
 * https://github.com/arielm/Unicode/blob/master/LICENSE.md
 */

#pragma once

#include "hb.h"

#include <string>

class TextRun
{
public:
    std::string text;
    hb_script_t script;
    std::string lang;
    hb_direction_t direction;
    
    TextRun(const std::string &text, hb_script_t script, const std::string &lang, hb_direction_t direction)
    :
    text(text),
    script(script),
    lang(lang),
    direction(direction)
    {}
    
    bool operator<(const TextRun &rhs) const
    {
        return tie(script, direction, lang, text) < tie(rhs.script, rhs.direction, rhs.lang, rhs.text);
    }
    
    void apply(hb_buffer_t *buffer) const
    {
        hb_buffer_set_script(buffer, script);
        hb_buffer_set_direction(buffer, direction);
        
        if (!lang.empty())
        {
            hb_buffer_set_language(buffer, hb_language_from_string(lang.data(), -1));
        }
        
        auto textSize = text.size();
        hb_buffer_add_utf8(buffer, text.data(), textSize, 0, textSize);
    }
};
