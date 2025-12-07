/*
 * Copyright (c) 2024 Balazs Scheidler <balazs.scheidler@axoflow.com.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */
#include "filterx-allocator.h"
#include "tls-support.h"
#include "compat/pow2.h"

#define FILTERX_AREA_SIZE 65536

typedef struct _FilterXArea
{
  gsize size, used;
  gchar mem[];
} FilterXArea;

gpointer
filterx_area_alloc(FilterXArea *self, gsize new_size)
{
  gsize alloc_size = pow2(round_to_log2(new_size));

  /* no more space here */
  if (self->size < self->used + alloc_size)
    return NULL;

  gpointer res = &self->mem[self->used];
  self->used += alloc_size;
  return res;
}

void
filterx_area_reset(FilterXArea *self)
{
  self->used = 0;
  memset(&self->mem, 0, self->size);
}

FilterXArea *
filterx_area_new(gsize size)
{
  FilterXArea *self = g_malloc0(sizeof(FilterXArea) + size);
  self->size = size;
  return self;
}

void
filterx_area_free(FilterXArea *self)
{
  g_free(self);
}

/* per thread state */

static FilterXArea *
_create_new_area(FilterXAllocator *allocator)
{
  FilterXArea *area = filterx_area_new(FILTERX_AREA_SIZE);
  g_ptr_array_add(allocator->areas, area);
  return area;
}

gpointer
filterx_allocator_malloc(FilterXAllocator *allocator, gsize size)
{
  FilterXArea *area;

  if (allocator->areas->len == 0)
    {
      area = _create_new_area(allocator);
      allocator->active_area = 0;
    }
  else
    {
      area = g_ptr_array_index(allocator->areas, allocator->active_area);
    }
  gpointer res = filterx_area_alloc(area, size);
  if (!res)
    {
      allocator->active_area++;
      if (allocator->active_area == allocator->areas->len)
        {
          area = _create_new_area(allocator);
        }
      else
        {
          area = g_ptr_array_index(allocator->areas, allocator->active_area);
          filterx_area_reset(area);
        }
      res = filterx_area_alloc(area, size);
    }
  g_assert(res != NULL);
  return res;
}


/* save the current allocator position, so we can restore it when we are
 * finished with the current filterx block */
void
filterx_allocator_save_position(FilterXAllocator *allocator, FilterXAllocatorPosition *pos)
{
  if (allocator->areas->len > 0)
    {
      pos->area = allocator->active_area;
      if (allocator->areas->len == pos->area)
        {
          /* we are at the end of our allocated areas, let's save the last
           * real one */
          pos->area--;
        }
      FilterXArea *area = g_ptr_array_index(allocator->areas, pos->area);
      pos->area_used = area->used;
    }
  else
    {
      pos->area = -1;
    }
  pos->position_index = allocator->position_index++;
}

/* restore the allocator position to the previous one.  This can only be
 * restored in the same order */
void
filterx_allocator_restore_position(FilterXAllocator *allocator, FilterXAllocatorPosition *pos)
{
  g_assert(allocator->position_index == pos->position_index + 1);
  allocator->position_index--;

  if (pos->area >= 0)
    {
      allocator->active_area = pos->area;
      FilterXArea *area = g_ptr_array_index(allocator->areas, pos->area);
      area->used = pos->area_used;
    }
}

void
filterx_allocator_empty(FilterXAllocator *allocator)
{
  allocator->active_area = 0;
  if (allocator->areas->len > 0)
    {
      FilterXArea *area;
      area = g_ptr_array_index(allocator->areas, 0);
      filterx_area_reset(area);
    }
}

void
filterx_allocator_init(FilterXAllocator *allocator)
{
  if (!allocator->areas)
    {
      allocator->areas = g_ptr_array_new_full(16, (GDestroyNotify) filterx_area_free);
      allocator->active_area = 0;
    }
  else
    {
      g_assert(allocator->active_area == 0);
    }
}

void
filterx_allocator_clear(FilterXAllocator *allocator)
{
  if (allocator->areas)
    g_ptr_array_free(allocator->areas, TRUE);
  allocator->areas = NULL;
}
