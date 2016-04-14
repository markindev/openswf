#include <memory>
#include <algorithm>

#include "debug.hpp"
#include "record.hpp"
#include "stream.hpp"
#include "player.hpp"
#include "shader.hpp"
#include "shape.hpp"
#include "bitmap.hpp"

extern "C" {
    #include "tesselator.h"
}

using namespace openswf::record;

namespace openswf
{
    // SHAPE FILL
    const uint32_t  MAX_POLYGON_SIZE    = 6;

    ShapeFillPtr ShapeFill::create(
        uint16_t cid,
        BitmapDataPtr bitmap,
        const Color& additive_start, const Color& additive_end,
        const Matrix& matrix_start, const Matrix& matrix_end)
    {
        auto fill = new (std::nothrow) ShapeFill();
        if( fill == nullptr ) return nullptr;

        fill->m_texture_cid = cid;
        fill->m_bitmap = std::move(bitmap);
        fill->m_texture = 0;

        fill->m_additive_start = additive_start;
        fill->m_additive_end = additive_end;

        fill->m_texcoord_start = matrix_start;
        fill->m_texcoord_end = matrix_end;

        return ShapeFillPtr(fill);
    }

    ShapeFillPtr ShapeFill::create(const Color& additive)
    {
        return create(0, nullptr, additive, additive, Matrix::identity, Matrix::identity);
    }

    ShapeFillPtr ShapeFill::create(const Color& start, const Color& end)
    {
        return create(0, nullptr, start, end, Matrix::identity, Matrix::identity);
    }

    ShapeFillPtr ShapeFill::create(BitmapDataPtr bitmap, const Matrix& transform)
    {
        return create(0, std::move(bitmap), Color::black, Color::black, transform, transform);
    }

    ShapeFillPtr ShapeFill::create(BitmapDataPtr bitmap, const Matrix& start, const Matrix& end)
    {
        return create(0, std::move(bitmap), Color::black, Color::black, start, end);
    }

    ShapeFillPtr ShapeFill::create(uint16_t cid, const Matrix& transform)
    {
        return create(cid, nullptr, Color::black, Color::black, transform, transform);
    }

    ShapeFillPtr ShapeFill::create(uint16_t cid, const Matrix& start, const Matrix& end)
    {
        return create(cid, nullptr, Color::black, Color::black, start, end);
    }

    Rid ShapeFill::get_bitmap(Player* env)
    {
        if( m_texture == 0 )
        {
            auto texture = env->get_character<Bitmap>(m_texture_cid);
            if( texture )
            {
                m_texture = texture->get_texture_rid();
            }
            else if( m_bitmap != nullptr )
            {
                m_texture = Render::get_instance().create_texture(
                    m_bitmap->get_ptr(), m_bitmap->get_width(), m_bitmap->get_height(), m_bitmap->get_format(), 1);
            }
        }

        return m_texture;
    }

    Color ShapeFill::get_additive_color(int ratio) const
    {
        return m_additive_start;
    }

    Point2f ShapeFill::get_texcoord(const Point2f& position, int ratio) const
    {
        static const Rect coordinates = Rect(-16384, 16384, -16384, 16384).to_pixel();

        Point2f ll = this->m_texcoord_start * Point2f(coordinates.xmin, coordinates.ymin);
        Point2f ru = this->m_texcoord_start * Point2f(coordinates.xmax, coordinates.ymax);

        return Point2f( (position.x-ll.x) / (ru.x - ll.x), (position.y-ll.y) / (ru.y - ll.y) );
    }

    // SHAPE RECORD

    ShapeRecordPtr ShapeRecord::create(const Rect& rect,
        std::vector<Point2f>&& vertices, std::vector<uint16_t>&& contour_indices)
    {
        auto record = new (std::nothrow) ShapeRecord;
        if( record == nullptr ) return nullptr;

        record->bounds = rect;
        record->vertices = std::move(vertices);
        record->contour_indices = std::move(contour_indices);
        return ShapeRecordPtr(record);
    }

    bool ShapeRecord::tesselate(
        const std::vector<Point2f>& vertices, const std::vector<uint16_t>& contour_indices,
        const std::vector<ShapeFillPtr>& fill_styles,
        std::vector<VertexPack>& out_vertices, std::vector<uint16_t>& out_vertices_size,
        std::vector<uint16_t>& out_indices, std::vector<uint16_t>& out_indices_size)
    {
        for( auto i=0; i<contour_indices.size(); i++ )
        {
            auto tess = tessNewTess(nullptr);
            if( !tess ) return false;
            
            auto end_pos = contour_indices[i];
            auto start_pos = i == 0 ? 0 : contour_indices[i-1];
            tessAddContour(tess, 2, vertices.data()+start_pos, sizeof(Point2f), end_pos-start_pos);
            
            if( !tessTesselate(tess, TESS_WINDING_NONZERO, TESS_POLYGONS, MAX_POLYGON_SIZE, 2, 0) )
            {
                tessDeleteTess(tess);
                return false;
            }

            const TESSreal* tess_vertices = tessGetVertices(tess);
            const TESSindex vcount = tessGetVertexCount(tess);
            const TESSindex nelems = tessGetElementCount(tess);
            const TESSindex* elems = tessGetElements(tess);

            auto vert_base_size = out_vertices.size();
            out_vertices.reserve(vert_base_size+vcount);
            for( int j=0; j<vcount; j++ )
            {
                auto position = Point2f(tess_vertices[j*2], tess_vertices[j*2+1]).to_pixel();
                auto texcoord = fill_styles[i]->get_texcoord(position);
                out_vertices.push_back( {position.x, position.y, texcoord.x, texcoord.y} );
            }

            auto ind_base_size = out_indices.size();
            out_indices.reserve(ind_base_size+nelems*(MAX_POLYGON_SIZE-2)*3);
            for( int j=0; j<nelems; j++ )
            {
                const int* p = &elems[j*MAX_POLYGON_SIZE];
                assert(p[0] != TESS_UNDEF && p[1] != TESS_UNDEF && p[2] != TESS_UNDEF);

                // triangle fans
                for( int k=2; k<MAX_POLYGON_SIZE && p[k] != TESS_UNDEF; k++ )
                {
                    out_indices.push_back(p[0]);
                    out_indices.push_back(p[k-1]);
                    out_indices.push_back(p[k]);
                }
            }

            tessDeleteTess(tess);
            out_indices_size.push_back( out_indices.size() );
            out_vertices_size.push_back( out_vertices.size() );
        }

        assert( out_indices_size.back() == out_indices.size() );
        assert( out_indices_size.size() == out_vertices_size.size() );
        return true;
    }

    /// SHAPE PARSING
    Shape* Shape::create(uint16_t cid, 
        std::vector<ShapeFillPtr>&& fill_styles,
        std::vector<LinePtr>&& line_styles,
        ShapeRecordPtr record)
    {
        auto shape = new (std::nothrow) Shape();
        if( shape && shape->initialize(cid, std::move(fill_styles), std::move(line_styles), std::move(record)) )
            return shape;

        LWARNING("failed to initialize shape!");
        if( shape ) delete shape;
        return nullptr;
    }

    bool Shape::initialize(uint16_t cid,
        std::vector<ShapeFillPtr>&& fill_styles,
        std::vector<LinePtr>&& line_styles,
        ShapeRecordPtr record)
    {
        this->character_id  = cid;
        this->bounds        = record->bounds;
        this->fill_styles   = std::move(fill_styles);
        this->line_styles   = std::move(line_styles);

        return ShapeRecord::tesselate(
            record->vertices, record->contour_indices, this->fill_styles,
            this->vertices, this->vertices_size, this->indices, this->indices_size);
    }

    INode* Shape::create_instance(Player* env)
    {
        return new ShapeNode(env, this);
    }

    uint16_t Shape::get_character_id() const
    {
        return this->character_id;
    }

    /// SHAPE NODE
    ShapeNode::ShapeNode(Player* env, Shape* shape)
    : INode(env, shape), m_shape(shape)
    {}

    void ShapeNode::update(float dt)
    {}

    void ShapeNode::render(const Matrix& matrix, const ColorTransform& cxform)
    {
        auto& shader = Shader::get_instance();
        shader.set_program(PROGRAM_DEFAULT);

        for( auto i=0; i<m_shape->vertices_size.size(); i++ )
        {
            auto vbase = i == 0 ? 0 : m_shape->vertices_size[i-1];
            auto vcount = m_shape->vertices_size[i] - vbase;

            auto ibase = i == 0 ? 0 : m_shape->indices_size[i-1];
            auto icount = m_shape->indices_size[i] - ibase;

            auto color = m_shape->fill_styles[i]->get_additive_color();
            for( auto j=vbase; j<vbase+vcount; j++ )
                m_shape->vertices[j].additive = color;

            shader.set_texture(0, m_shape->fill_styles[i]->get_bitmap(m_environment));
            shader.draw(
                vcount, m_shape->vertices.data()+vbase,
                icount, m_shape->indices.data()+ibase,
                matrix*m_matrix, cxform*m_cxform);
        }
    }
}