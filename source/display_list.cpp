#include "player.hpp"
#include "display_list.hpp"

#include <algorithm>

extern "C" {
#include "GLFW/glfw3.h"
}

namespace openswf
{
    ////
    Primitive::Primitive(Shape* shape) : m_shape(shape) {}

    Primitive::~Primitive() {}

    void Primitive::update(float dt) {}

    void Primitive::render(const Matrix& matrix, const ColorTransform& cxform)
    {
        m_shape->render(m_matrix*matrix, m_cxform*cxform);
    }

    ////
    MovieClip::MovieClip(Player* env, Sprite* sprite)
    : m_environment(env), m_sprite(sprite), m_frame_timer(0), m_current_frame(0), m_paused(false)
    {
        assert( sprite->get_frame_rate() < 64 && sprite->get_frame_rate() > 0.1f );
        m_frame_delta = 1.f / sprite->get_frame_rate();
        goto_and_play(1);
    }

    MovieClip::~MovieClip()
    {
        reset();
    }

    // INHERITANTED
    void MovieClip::update(float dt)
    {
        if( !m_paused )
        {
            m_frame_timer += dt;
            if( m_frame_timer > m_frame_delta )
            {
                auto frame = m_current_frame;
                while( m_frame_timer > m_frame_delta )
                {
                    m_frame_timer -= m_frame_delta;

                    if( frame >= m_sprite->get_frame_count() )
                        frame = 0;
                    frame ++;
                }

                goto_frame(frame);
            }
        }

        for( auto& pair : m_children )
            pair.second->update(dt);
    }

    void MovieClip::render(const Matrix& matrix, const ColorTransform& cxform)
    {
        for( auto& pair : m_children )
            pair.second->render( matrix*m_matrix, cxform*m_cxform );
    }

    // PROTECTED METHODS
    void MovieClip::place(uint16_t depth, uint16_t cid, const Matrix& matrix, const ColorTransform& cxform)
    {
        auto iter = m_children.find(depth);
        if( iter != m_children.end() )
            delete iter->second;

        auto ch = m_environment->get_character(cid);
        if( ch != nullptr )
        {
            if( typeid(ch) == typeid(Sprite*) )
                m_children[depth] = new MovieClip(m_environment, static_cast<Sprite*>(ch));
            else
                m_children[depth] = new Primitive(static_cast<Shape*>(ch));
        }
    }

    void MovieClip::modify(uint16_t depth, const Matrix& matrix, const ColorTransform& cxform)
    {
        auto iter = m_children.find(depth);
        if( iter == m_children.end() )
            return;

        iter->second->reset(matrix, cxform);
    }

    void MovieClip::erase(uint16_t depth)
    {
        auto iter = m_children.find(depth);
        if( iter == m_children.end() )
            return;

        delete iter->second;
        m_children.erase(iter);
    }

    void MovieClip::reset()
    {
        for( auto& pair : m_children )
            delete pair.second;
        m_children.clear();
        m_current_frame = 0;
    }

    void MovieClip::goto_and_play(uint32_t frame)
    {
        m_paused = false;
        goto_frame(frame);
    }

    void MovieClip::goto_and_stop(uint32_t frame)
    {
        m_paused = true;
        goto_frame(frame);
    }

    void MovieClip::goto_frame(uint32_t frame)
    {
        m_frame_timer = 0.f;
        
        if( frame < 1 ) frame = 1;
        if( m_current_frame == frame )
            return;

        if( m_current_frame > (int)frame )
            reset();

        while(
            m_current_frame < frame &&
            m_current_frame < m_sprite->get_frame_count() )
        {
            m_sprite->execute(*this, ++m_current_frame);
        }
        
        m_current_frame = frame;
    }
}