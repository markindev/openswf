#pragma once

#include <cstdint>
#include <algorithm>
#include <vector>
#include <unordered_map>

#include "openswf_stream.hpp"

// ## TAG Categories
// there two categories of tags in a SWF file are as follows:
// 1. definition tags define the content of the SWF file—the shapes, text,
// bitmaps, sounds, and so on. each definition tag assigns a unique ID
// called a character ID to the content it defines.
// 2. control tags create and manipulate rendered instances of characters in
// the dictionary, and control the flow of the file.
//
// ## TAG Ordering
// Generally speaking, tags in a SWF can occur in any order. However, you
// must observe the following rules:
// 1. The FileAttributes tag must be the first tag in the SWF file for SWF 8 and later.
// 2. A tag should only depend on tags that come before it. A tag should never
// depend on a tag that comes later in the file.
// 3. A definition tag that defines a character must occur before any control
// tag that refers to that character.
// 4. Streaming sound tags must be in order. Out-of-order streaming sound tags
// result in the sound being played out of order.
// 5. The End tag is always the last tag in the SWF file.

namespace openswf
{
    class IStyleCommand;

    namespace record // should we hide this details from interface?
    {
        struct Header
        {
            bool        compressed;
            uint8_t     version;
            uint32_t    size;
            Rect        frame_size;   // frame size in twips
            float       frame_rate;   // frame delay in 8.8 fixed number of frames per second
            uint16_t    frame_count;  // total number of frames in file

            static Header read(Stream& stream);
        };

        struct TagHeader
        {
            TagCode     code; // tag code
            uint32_t    size; // offset in bytes from end of header to next tag
            uint32_t    end_pos;

            TagHeader() : code(TagCode::END), size(0) {}
            static TagHeader read(Stream& stream);
        };

        // TAG = 0
        // the End tag indicates the end of file
        struct End
        {
            static End read(Stream& stream);
        };

        // TAG = 1
        // the ShowFrame tag instructs us to display the contents of the display list. 
        // the file is paused for the duration of a single frame.
        struct ShowFrame
        {
            static ShowFrame read(Stream& stream);
        };

        // TAG = 2
        // The DefineShape tag defines a shape for later use by control tags such as PlaceObject.

        // TAG = 22
        // DefineShape2 extends the capabilities of DefineShape with the ability to support
        // more than 255 styles in the style list and multiple style lists in a single shape.

        // TAG = 32
        // DefineShape3 extends the capabilities of DefineShape2 by extending all
        // of the RGB color fields to support RGBA with opacity information.
        // struct FillGradient {};
        // struct FillBitmap {};

        struct LineStyle
        {
            typedef std::vector<LineStyle> Array;

            uint16_t    width;
            Color       color;
        };

        struct ShapeEdge
        {
            typedef std::vector<ShapeEdge> Array;

            ShapeEdge(const Point2f& anchor)
                : control(anchor), anchor(anchor) {}

            ShapeEdge(int32_t cx, int32_t cy, int32_t ax, int32_t ay)
                : control(Point2f(cx, cy)), anchor(Point2f(ax, ay)){}

            Point2f control, anchor;
        };

        struct ShapePath
        {
            typedef std::vector<ShapePath> Array;

            uint32_t            left_fill;
            uint32_t            right_fill;
            uint32_t            line;

            Point2f             start;
            ShapeEdge::Array    edges;

            ShapePath() : left_fill(0), right_fill(0), line(0) {}
            void reset()
            {
                left_fill = right_fill = 0;
                line = 0;
            }

            void restart(const Point2f& cursor)
            {
                start.x = cursor.x;
                start.y = cursor.y;
                edges.clear();
            }
        };

        struct DefineShape
        {
            uint16_t            character_id;
            Rect                bounds;         // bounds of shape

            // * the style arrays begin at index 1
            std::vector<IStyleCommand*> fill_styles;
            LineStyle::Array    line_styles;
            ShapePath::Array    paths;

            static DefineShape read(Stream& stream, TagCode type);
        };

        // TAG = 4
        // the PlaceObject tag adds a character to the display list.

        // TAG = 26
        // the PlaceObject2 tag can both add a character to the display list,
        // and modify the attributes of a character that is already on the display list.
        struct PlaceObject
        {
            uint16_t        character_id;   // ID of character to place
            uint16_t        depth;          // depth of character
            Matrix          matrix;         // transform matrix data
            ColorTransform  cxform;         // color transform data

            uint16_t        ratio;          // morph ratio
            std::string     name;           // name of sprite
            uint16_t        clip_depth;     // specifies the top-most depth that will be masked 

            PlaceObject() : character_id(0), depth(0), ratio(0), clip_depth(0) {}
            static PlaceObject read(Stream& stream, const TagHeader& header);

            void parse_tag_4(Stream& stream, const TagHeader& header);
            void parse_tag_26(Stream& stream);
        };

        // TAG = 5
        // the RemoveObject tag removes the specified character (at the 
        // specified depth) from the display list.

        // Tag = 28
        // The RemoveObject2 tag removes the character at the specified depth from the display list.
        struct RemoveObject
        {
            uint16_t    character_id;
            uint16_t    depth;

            static RemoveObject read(Stream& stream, TagCode type);
        };

        // TAG = 9
        // the SetBackgroundColor tag sets the background color of the display.
        struct SetBackgroundColor
        {
            Color   color;

            static SetBackgroundColor read(Stream& stream);
        };

        // TAG = 39
        // The DefineSprite tag defines a sprite character.
        // It consists of a character ID and a frame count, followed by a series of control tags.
        // The sprite is terminated with an End tag.
        // The following tags are valid within a DefineSprite tag:
        // 1. ShowFrame 2. PlaceObject 3. PlaceObject2 4. RemoveObject 5. RemoveObject2
        // 6. StartSound 7. FrameLabel 8. SoundStreamHead 9. SoundStreamHead2 10. SoundStreamBlock
        // 11. Actions 12. End

        struct DefineSpriteHeader
        {
            uint16_t    character_id;
            uint16_t    frame_count;

            static DefineSpriteHeader read(Stream& stream);
        };

        // TAG = 43
        // the FRAME_LABEL tag gives the specified name to the current frame
        struct FrameLabel
        {
            std::string name;
            uint8_t     named_anchor;   // swf 6 later

            static FrameLabel read(Stream& stream);
        };

        // TAG = 69
        // the FileAttributes tag defines characteristics of the SWF file.
        // this tag is required for swf 8 and later and must be the first
        // in the swf file.
        struct FileAttributes
        {
            uint32_t attributes; // see FileAttributeMask for details

            static FileAttributes read(Stream& stream);
        };

        // TAG = 86
        // the DefineSceneAndFrameLabelData tag contains scene and frame label data for a MovieClip. 
        // scenes are supported for the main timeline only, for all other movie clips 
        // a single scene is exported.
        struct DefineSceneAndFrameLabelData
        {
            uint32_t                    scene_count;        // number of scenes
            std::vector<uint32_t>       scene_offsets;      //
            std::vector<std::string>    scene_names;        //
            
            uint32_t                    frame_label_count;
            std::vector<uint32_t>       frame_numbers;      // 
            std::vector<std::string>    frame_labels;       // 
            
            static DefineSceneAndFrameLabelData read(Stream& stream);
        };
    }
}