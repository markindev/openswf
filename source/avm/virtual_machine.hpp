#pragma once

#include "avm/avm.hpp"
#include "avm/movie_object.hpp"

NS_AVM_BEGIN

class VirtualMachine
{
protected:
    MovieObject*    m_root;
    uint32_t        m_gc_threshold;
    uint32_t        m_objects;
    int32_t         m_version;

public:
    VirtualMachine(MovieNode*, int version = 10);
    ~VirtualMachine();

    void execute(MovieObject*, const uint8_t* bytes, int length);
    void gabarge_collect();

    template<typename T> T* new_object()
    {
        auto nv = new T();
        nv->m_next = m_root->m_next;
        m_root->m_next = nv;
        m_objects ++;
        return nv;
    }

    MovieObject* new_movie_object(MovieNode* node);
    void free_movie_object(MovieObject* movie);

    int32_t get_version() const;
};

// INLINE METHODS

inline int32_t VirtualMachine::get_version() const
{
    return m_version;
}

NS_AVM_END