/*
 * Copyright (C) 2018 Glimp IP Ltd
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define strdup(X) _strdup(X)
#endif

#include <vector>
#include <mutex>
#include <condition_variable>

#include "glimpse_log.h"
#include "glimpse_mem_pool.h"


struct gm_mem_pool {
    struct gm_logger *log;

    char *name;

    std::mutex lock;
    std::condition_variable available_cond;
    unsigned max_size;
    std::vector<void *> available;
    std::vector<void *> busy;

    void *(*alloc_mem)(struct gm_mem_pool *pool, void *user_data);
    void (*free_mem)(struct gm_mem_pool *pool, void *mem, void *user_data);
    void *user_data;
};

struct gm_mem_pool *
mem_pool_alloc(struct gm_logger *log,
               const char *name,
               unsigned max_size,
               void *(*alloc_mem)(struct gm_mem_pool *pool, void *user_data),
               void (*free_mem)(struct gm_mem_pool *pool, void *mem,
                                void *user_data),
               void *user_data)
{
    struct gm_mem_pool *pool = new gm_mem_pool();

    pool->log = log;
    pool->max_size = max_size;
    pool->name = strdup(name);
    pool->alloc_mem = alloc_mem;
    pool->free_mem = free_mem;
    pool->user_data = user_data;

    return pool;
}

void
mem_pool_free(struct gm_mem_pool *pool)
{
    mem_pool_free_resources(pool);
    free(pool->name);
    delete pool;
}

static void __attribute__((unused))
debug_print_busy_and_available_lists(struct gm_mem_pool *pool)
{
    gm_debug(pool->log, "pool %p (%s) lists:",
             pool, pool->name);

    unsigned size = pool->busy.size();
    for (unsigned i = 0; i < size; i++) {
        gm_debug(pool->log, "busy> %p", pool->busy[i]);
    }
    size = pool->available.size();
    for (unsigned i = 0; i < size; i++) {
        gm_debug(pool->log, "available> %p", pool->available[i]);
    }
}

void *
mem_pool_acquire_resource(struct gm_mem_pool *pool)
{
    void *resource;

    std::unique_lock<std::mutex> scoped_cond_lock(pool->lock);

    //gm_error(pool->log, "mem_pool_acquire_resource: lists before");
    //debug_print_busy_and_available_lists(pool);

    /* Sanity check with arbitrary upper limit for the number of allocations */
    /* XXX Had to remove this assertion for recording mode, where we keep
     *     frame recordings around for an indefinite amount of time. In the
     *     situation we see memory unexpectedly growing out of control, we
     *     likely want to re-enable this.
     */
#if 0
    gm_assert(pool->log,
              (pool->busy.size() + pool->available.size()) < 100,
              "'%s' memory pool growing out of control (%u allocations)",
              pool->name,
              (pool->busy.size() + pool->available.size()));
#endif

    if (pool->available.size()) {
        resource = pool->available.back();
        pool->available.pop_back();
    } else if (pool->busy.size() + pool->available.size() > pool->max_size) {

        gm_debug(pool->log,
                 "Throttling \"%s\" pool acquisition, waiting for old %s object to be released\n",
                 pool->name, pool->name);

        while (!pool->available.size())
            pool->available_cond.wait(scoped_cond_lock);

        resource = pool->available.back();
        pool->available.pop_back();
    } else {
        resource = pool->alloc_mem(pool, pool->user_data);
    }

    pool->busy.push_back(resource);

    //gm_debug(pool->log, "mem_pool_acquire_resource %p: lists after", resource);
    //debug_print_busy_and_available_lists(pool);

    return resource;
}

void
mem_pool_recycle_resource(struct gm_mem_pool *pool, void *resource)
{
    std::lock_guard<std::mutex> scope_lock(pool->lock);

    //gm_error(pool->log, "mem_pool_recycle_resource %p: lists before", resource);
    //debug_print_busy_and_available_lists(pool);

    unsigned size = pool->busy.size();
    for (unsigned i = 0; i < size; i++) {
        if (pool->busy[i] == resource) {
            pool->busy[i] = pool->busy.back();
            pool->busy.pop_back();
            break;
        }
    }

    gm_assert(pool->log,
              pool->busy.size() == (size - 1),
              "Didn't find recycled resource %p in %s pool's busy list",
              resource,
              pool->name);

    pool->available.push_back(resource);

    //gm_debug(pool->log, "mem_pool_recycle_resource %p: lists after", resource);
    //debug_print_busy_and_available_lists(pool);

    pool->available_cond.notify_all();
}

void
mem_pool_free_resources(struct gm_mem_pool *pool)
{
    gm_assert(pool->log,
              pool->busy.size() == 0,
              "Shouldn't be freeing a pool (%s) with resources still in use",
              pool->name);

    while (pool->available.size()) {
        void *resource = pool->available.back();
        pool->available.pop_back();
        pool->free_mem(pool, resource, pool->user_data);
    }
}

const char *
mem_pool_get_name(struct gm_mem_pool *pool)
{
    return pool->name;
}

void
mem_pool_foreach(struct gm_mem_pool *pool,
                 void (*callback)(struct gm_mem_pool *pool,
                                  void *resource,
                                  void *user_data),
                 void *user_data)
{
    std::lock_guard<std::mutex> scope_lock(pool->lock);

    //debug_print_busy_and_available_lists(pool);

    unsigned size = pool->busy.size();
    for (unsigned i = 0; i < size; i++) {
        callback(pool, pool->busy[i], user_data);
    }
}
