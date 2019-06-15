#pragma once

#include "glassert.h"
#include "pixel.h"

#include "gsl-lite.hpp"

#include <vector>

namespace render
{
namespace gl
{
namespace detail
{
template<typename T, size_t N>
struct FastFill
{
    static_assert( sizeof( T ) == N, "Type size mismatch" );

    static inline void fill(const gsl::not_null<T*>& data, size_t n, const T& value)
    {
        std::fill_n( data.get(), n, value );
    }
};


template<typename T>
struct FastFill<T, 1>
{
    static_assert( sizeof( T ) == 1, "Type size mismatch" );

    static inline void fill(const gsl::not_null<T*>& data, size_t n, const T& value)
    {
        std::memset( data.get(), value, n );
    }
};


template<typename T>
struct FastFill<T, 2>
{
    static_assert( sizeof( T ) == 2, "Type size mismatch" );

    static inline void fill(const gsl::not_null<T*>& data, size_t n, const T& value)
    {
        std::wmemset( data.get(), value, n );
    }
};


template<>
struct FastFill<SRGBA8, 4>
{
    static_assert( sizeof( SRGBA8 ) == 4, "Type size mismatch" );

    static inline void fill(const gsl::not_null<SRGBA8*>& data, const size_t n, const SRGBA8& value)
    {
        const auto scalar = value.r;
        if( scalar == value.g && scalar == value.b && scalar == value.a )
            std::memset( data.get(), scalar, n * 4u );
        else
            std::fill_n( data.get(), n, value );
    }
};


template<typename T>
inline void fill(const gsl::not_null<T*>& data, size_t n, const T& value)
{
    FastFill<T, sizeof( T )>::fill( data, n, value );
}
}

template<typename TStorage>
class Image
{
public:
    using StorageType = TStorage;

    explicit Image(const ::gl::GLint width, const ::gl::GLint height, const StorageType* data = nullptr)
            : m_data{}
            , m_width{width}
            , m_height{height}
    {
        Expects( width >= 0 && height >= 0 );

        const auto dataSize = static_cast<size_t>(width * height);
        if( data == nullptr )
            m_data.resize( dataSize );
        else
            m_data.assign( data, data + dataSize );
    }

    Image() : Image{0, 0}
    {}

    Image(const Image&) = delete;

    Image(Image&& rhs) noexcept
            : m_data{std::move( rhs.m_data )}
            , m_width{rhs.m_width}
            , m_height{rhs.m_height}
    {
        rhs.m_width = 0;
        rhs.m_height = 0;
    }

    Image& operator=(const Image&) = delete;

    Image& operator=(Image&& rhs) noexcept
    {
        m_data = std::move( rhs.m_data );
        m_width = rhs.m_width;
        m_height = rhs.m_height;
        rhs.m_width = 0;
        rhs.m_height = 0;
        return *this;
    }

    ~Image() = default;

    const std::vector<StorageType>& getData() const
    {
        return m_data;
    }

    StorageType* getRawData()
    {
        return m_data.data();
    }

    auto begin()
    {
        return m_data.begin();
    }

    void assign(const std::vector<StorageType>& data)
    {
        Expects( m_data.size() == data.size() );
        m_data = data;
    }

    void assign(const StorageType* data, size_t size)
    {
        Expects( m_data.size() == size );
        m_data.assign( data + 0, data + size );
    }

    void assign(std::vector<StorageType>&& data)
    {
        Expects( m_data.size() == data.size() );
        m_data = std::move( data );
    }

    ::gl::GLint getHeight() const
    {
        return m_height;
    }

    ::gl::GLint getWidth() const
    {
        return m_width;
    }

    StorageType& at(const ::gl::GLint x, const ::gl::GLint y)
    {
        Expects( x >= 0 );
        Expects( x < m_width );
        Expects( y >= 0 );
        Expects( y < m_height );

        if( x < 0 || x >= m_width || y < 0 || y >= m_height )
        {
            BOOST_THROW_EXCEPTION( std::out_of_range{"Image coordinates out of range"} );
        }

        return m_data[y * m_width + x];
    }

    void set(const ::gl::GLint x, const ::gl::GLint y, const StorageType& pixel, const bool blend = false)
    {
        if( x < 0 || x >= m_width || y < 0 || y >= m_height )
            return;

        const auto o = gsl::narrow_cast<size_t>( y * m_width + x );

        if( !blend )
        {
            m_data[o] = pixel;
        }
        else
        {
            m_data[o] = mixAlpha( m_data[o], pixel );
        }
    }

    const StorageType& at(const ::gl::GLint x, const ::gl::GLint y) const
    {
        Expects( x >= 0 );
        Expects( x < m_width );
        Expects( y >= 0 );
        Expects( y < m_height );

        if( x < 0 || x >= m_width || y < 0 || y >= m_height )
        {
            BOOST_THROW_EXCEPTION( std::out_of_range{"Image coordinates out of range"} );
        }

        return m_data[y * m_width + x];
    }

    void fill(const StorageType& color)
    {
        if( !m_data.empty() )
            detail::fill( gsl::not_null<StorageType*>{m_data.data()}, m_data.size(), color );
    }

    void line(::gl::GLint x0, ::gl::GLint y0, const ::gl::GLint x1, const ::gl::GLint y1, const StorageType& color, const bool blend = false)
    {
        // shamelessly copied from wikipedia
        const ::gl::GLint dx = abs( x1 - x0 );
        const ::gl::GLint sx = x0 < x1 ? 1 : -1;
        const ::gl::GLint dy = -abs( y1 - y0 );
        const ::gl::GLint sy = y0 < y1 ? 1 : -1;

        ::gl::GLint err = dx + dy;

        while( true )
        {
            set( x0, y0, color, blend );

            if( x0 == x1 && y0 == y1 )
                break;

            const auto e2 = 2 * err;
            if( e2 > dy )
            {
                err += dy;
                x0 += sx;
            }
            if( e2 < dx )
            {
                err += dx;
                y0 += sy;
            }
        }
    }

private:

    std::vector<StorageType> m_data;

    ::gl::GLint m_width;

    ::gl::GLint m_height;
};
}
}