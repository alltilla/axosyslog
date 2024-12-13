/*
 * Copyright (c) 2023 Balazs Scheidler <balazs.scheidler@axoflow.com>
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
#include "filterx/expr-setattr.h"
#include "filterx/object-primitive.h"
#include "filterx/object-string.h"
#include "filterx/filterx-eval.h"
#include "filterx/filterx-ref.h"
#include "scratch-buffers.h"

typedef struct _FilterXSetAttr
{
  FilterXExpr super;
  FilterXExpr *object;
  FilterXObject *attr;
  FilterXExpr *new_value;
} FilterXSetAttr;

static FilterXObject *
_eval(FilterXExpr *s)
{
  FilterXSetAttr *self = (FilterXSetAttr *) s;
  FilterXObject *result = NULL;

  FilterXObject *object = filterx_expr_eval_typed(self->object);
  if (!object)
    return NULL;

  if (object->readonly)
    {
      filterx_eval_push_error("Attribute set failed, object is readonly", s, self->attr);
      goto exit;
    }

  FilterXObject *new_value = filterx_expr_eval(self->new_value);
  if (!new_value)
    goto exit;

  /* TODO: create ref unconditionally after implementing hierarchical CoW for JSON types
   * (or after creating our own dict/list repr) */
  if (!new_value->weak_referenced)
    {
      new_value = filterx_ref_new(new_value);
    }

  FilterXObject *cloned = filterx_object_clone(new_value);
  filterx_object_unref(new_value);

  if (!filterx_object_setattr(object, self->attr, &cloned))
    {
      filterx_eval_push_error("Attribute set failed", s, self->attr);
      filterx_object_unref(cloned);
    }
  else
    {
      result = cloned;
    }

exit:
  filterx_object_unref(object);
  return result;
}

static gboolean
_init(FilterXExpr *s, GlobalConfig *cfg)
{
  FilterXSetAttr *self = (FilterXSetAttr *) s;

  if (!filterx_expr_init(self->object, cfg))
    return FALSE;

  if (!filterx_expr_init(self->new_value, cfg))
    {
      filterx_expr_deinit(self->object, cfg);
      return FALSE;
    }

  return filterx_expr_init_method(s, cfg);
}

static void
_deinit(FilterXExpr *s, GlobalConfig *cfg)
{
  FilterXSetAttr *self = (FilterXSetAttr *) s;

  filterx_expr_deinit(self->object, cfg);
  filterx_expr_deinit(self->new_value, cfg);
  filterx_expr_deinit_method(s, cfg);
}

static void
_free(FilterXExpr *s)
{
  FilterXSetAttr *self = (FilterXSetAttr *) s;

  filterx_object_unref(self->attr);
  filterx_expr_unref(self->object);
  filterx_expr_unref(self->new_value);
  filterx_expr_free_method(s);
}

/* Takes reference of object and new_value */
FilterXExpr *
filterx_setattr_new(FilterXExpr *object, FilterXString *attr_name, FilterXExpr *new_value)
{
  FilterXSetAttr *self = g_new0(FilterXSetAttr, 1);

  filterx_expr_init_instance(&self->super);
  self->super.eval = _eval;
  self->super.init = _init;
  self->super.deinit = _deinit;
  self->super.free_fn = _free;
  self->object = object;

  self->attr = (FilterXObject *) attr_name;

  self->new_value = new_value;
  self->super.ignore_falsy_result = TRUE;
  return &self->super;
}
