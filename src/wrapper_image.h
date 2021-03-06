// Copyright (C) 2006-2014  Davis E. King (davis@dlib.net)
// Copyright (C) 2017       Impossible Labs
//
// Boost Software License - Version 1.0 - August 17th, 2003
//
// Permission is hereby granted, free of charge, to any person or organization
// obtaining a copy of the software and accompanying documentation covered by
// this license (the "Software") to use, reproduce, display, distribute,
// execute, and transmit the Software, and to prepare derivative works of the
// Software, and to permit third-parties to whom the Software is furnished to
// do so, all subject to the following:
//
// The copyright notices in the Software and this entire statement, including
// the above license grant, this restriction and the following disclaimer,
// must be included in all copies of the Software, in whole or in part, and
// all derivative works of the Software, unless such copies or derivative
// works are solely in the form of machine-executable object code generated by
// a source language processor.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
// SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
// FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.

#pragma once

#include <dlib/assert.h>
#include <dlib/image_processing/generic_image.h>

namespace glimpse
{
    template <
        typename T,
        typename mem_manager = dlib::default_memory_manager
        >
    class wrapped_image
    {
    public:
        void set_size (long rows, long cols);

        long nc() const { return nc_; }
        long nr() const { return nr_; }
        long width_step() const { return stride_; }
        const T *data() const { return data_; }

        void wrap(long width,
                  long height,
                  long stride,
                  const T *data)
        {
            assert(stride >= width * (long)sizeof(T));

            nc_ = width;
            stride_ = stride;
            nr_ = height;
            data_ = data;
        }

    private:
        const T* data_;
        long nc_;
        long nr_;
        long stride_;
    };

    template <
        typename T,
        typename mem_manager
        >
    void glimpse::wrapped_image<T,mem_manager>::set_size(long rows,
                                                         long cols)
    {
        // make sure requires clause is not broken
        DLIB_ASSERT((cols >= 0 && rows >= 0) ,
               "\tvoid array2d::set_size(long rows, long cols)"
               << "\n\tThe array2d can't have negative rows or columns."
               << "\n\tthis: " << this
               << "\n\tcols: " << cols 
               << "\n\trows: " << rows 
        );
    }
}


/* Allow dlib to use our wrapper as an image by implementing dlib's
 * generic image trait...
 */
namespace dlib
{
    template <typename T, typename mm>
    struct image_traits<glimpse::wrapped_image<T,mm> >
    {
        typedef T pixel_type;
    };
    template <typename T, typename mm>
    struct image_traits<const glimpse::wrapped_image<T,mm> >
    {
        typedef T pixel_type;
    };

    template <typename T, typename mm>
    inline long num_rows( const glimpse::wrapped_image<T,mm>& img) { return img.nr(); }
    template <typename T, typename mm>
    inline long num_columns( const glimpse::wrapped_image<T,mm>& img) { return img.nc(); }

    template <typename T, typename mm>
    inline void set_image_size(
        glimpse::wrapped_image<T,mm>& img,
        long rows,
        long cols 
    ) {
        /* We don't own the buffer storage so we can't resize it.
         *
         * Also don't bother considering the case where the image becomes
         * smaller - simpler to just disallow resizing entirely.
         */
        assert(!"reached");
        //img.set_size(rows,cols);
    }

    template <typename T, typename mm>
    inline void* image_data(glimpse::wrapped_image<T,mm>& img)
    {
        return (void *)(img.data());
    }

    template <typename T, typename mm>
    inline const void *image_data(const glimpse::wrapped_image<T,mm>& img)
    {
        return static_cast<const void *>(img.data());
    }

    template <typename T, typename mm>
    inline long width_step(const glimpse::wrapped_image<T,mm>& img) 
    { 
        return img.width_step(); 
    }

}


