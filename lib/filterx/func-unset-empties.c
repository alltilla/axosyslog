/*
 * Copyright (c) 2024 Attila Szakacs
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

#include "filterx/func-unset-empties.h"
#include "filterx/object-extractor.h"
#include "filterx/object-string.h"
#include "filterx/object-primitive.h"
#include "filterx/object-null.h"
#include "filterx/object-dict-interface.h"
#include "filterx/object-list-interface.h"
#include "filterx/filterx-eval.h"
#include "filterx/filterx-globals.h"

#define FILTERX_FUNC_UNSET_EMPTIES_USAGE "Usage: unset_empties(object, recursive=true)"

typedef struct FilterXFunctionUnsetEmpties_
{
  FilterXFunction super;
  FilterXExpr *object_expr;
  gboolean recursive;
} FilterXFunctionUnsetEmpties;

static gboolean _process_dict(FilterXFunctionUnsetEmpties *self, FilterXObject *obj);
static gboolean _process_list(FilterXFunctionUnsetEmpties *self, FilterXObject *obj);

static gboolean
_should_unset(FilterXFunctionUnsetEmpties *self, FilterXObject *obj)
{
  gsize str_len;
  const gchar *str;
  if (filterx_object_extract_string(obj, &str, &str_len))
    {
      return str_len == 0 ||
             strcasecmp(str, "n/a") == 0 ||
             strcmp(str, "-") == 0;
    }

  if (filterx_object_is_type(obj, &FILTERX_TYPE_NAME(null)))
    {
      return TRUE;
    }

  if (filterx_object_is_type(obj, &FILTERX_TYPE_NAME(dict)) ||
      filterx_object_is_type(obj, &FILTERX_TYPE_NAME(list)))
    {
      guint64 len;
      filterx_object_len(obj, &len);
      return len == 0;
    }

  return FALSE;
}

/* Also unsets inner dicts' and lists' values is recursive is set. */
static gboolean
_add_key_to_unset_list_if_needed(FilterXObject *key, FilterXObject *value, gpointer user_data)
{
  FilterXFunctionUnsetEmpties *self = ((gpointer *) user_data)[0];
  GList **keys_to_unset = ((gpointer *) user_data)[1];

  if (self->recursive)
    {
      if (filterx_object_is_type(value, &FILTERX_TYPE_NAME(dict)) && !_process_dict(self, value))
        return FALSE;
      if (filterx_object_is_type(value, &FILTERX_TYPE_NAME(list)) && !_process_list(self, value))
        return FALSE;
    }

  if (!_should_unset(self, value))
    return TRUE;

  *keys_to_unset = g_list_append(*keys_to_unset, filterx_object_ref(key));
  return TRUE;
}

static gboolean
_process_dict(FilterXFunctionUnsetEmpties *self, FilterXObject *obj)
{
  GList *keys_to_unset = NULL;
  gpointer user_data[] = { self, &keys_to_unset };
  gboolean success = filterx_dict_iter(obj, _add_key_to_unset_list_if_needed, user_data);

  for (GList *elem = keys_to_unset; elem && success; elem = elem->next)
    {
      FilterXObject *key = (FilterXObject *) elem->data;
      if (!filterx_object_unset_key(obj, key))
        success = FALSE;
    }

  g_list_free_full(keys_to_unset, (GDestroyNotify) filterx_object_unref);
  return success;
}

/* Takes reference of obj. */
static FilterXObject *
_eval_on_dict(FilterXFunctionUnsetEmpties *self, FilterXObject *obj)
{
  gboolean success = _process_dict(self, obj);
  filterx_object_unref(obj);
  return success ? filterx_boolean_new(TRUE) : NULL;
}

static gboolean
_process_list(FilterXFunctionUnsetEmpties *self, FilterXObject *obj)
{
  guint64 len;
  filterx_object_len(obj, &len);
  if (len == 0)
    return TRUE;

  for (gint64 i = ((gint64) len) - 1; i >= 0; i--)
    {
      FilterXObject *elem = filterx_list_get_subscript(obj, i);

      if (self->recursive)
        {
          if (filterx_object_is_type(elem, &FILTERX_TYPE_NAME(dict)) && !_process_dict(self, elem))
            {
              filterx_object_unref(elem);
              return FALSE;
            }
          if (filterx_object_is_type(elem, &FILTERX_TYPE_NAME(list)) && !_process_list(self, elem))
            {
              filterx_object_unref(elem);
              return FALSE;
            }
        }

      if (_should_unset(self, elem))
        {
          if (!filterx_list_unset_index(obj, i))
            {
              filterx_object_unref(elem);
              return FALSE;
            }
        }

      filterx_object_unref(elem);
    }

  return TRUE;
}

/* Takes reference of obj. */
static FilterXObject *
_eval_on_list(FilterXFunctionUnsetEmpties *self, FilterXObject *obj)
{
  gboolean success = _process_list(self, obj);
  filterx_object_unref(obj);
  return success ? filterx_boolean_new(TRUE) : NULL;
}

static FilterXObject *
_eval(FilterXExpr *s)
{
  FilterXFunctionUnsetEmpties *self = (FilterXFunctionUnsetEmpties *) s;

  FilterXObject *obj = filterx_expr_eval(self->object_expr);
  if (!obj)
    {
      filterx_eval_push_error("Failed to evaluate first argument. " FILTERX_FUNC_UNSET_EMPTIES_USAGE, s, NULL);
      return NULL;
    }

  if (filterx_object_is_type(obj, &FILTERX_TYPE_NAME(dict)))
    return _eval_on_dict(self, obj);

  if (filterx_object_is_type(obj, &FILTERX_TYPE_NAME(list)))
    return _eval_on_list(self, obj);

  filterx_eval_push_error("Object must be dict or list. " FILTERX_FUNC_UNSET_EMPTIES_USAGE, s, obj);
  filterx_object_unref(obj);
  return NULL;
}

static void
_free(FilterXExpr *s)
{
  FilterXFunctionUnsetEmpties *self = (FilterXFunctionUnsetEmpties *) s;
  filterx_expr_unref(self->object_expr);
  filterx_function_free_method(&self->super);
}

static FilterXExpr *
_extract_object_expr(FilterXFunctionArgs *args, GError **error)
{
  FilterXExpr *object_expr = filterx_function_args_get_expr(args, 0);
  if (!object_expr)
    {
      g_set_error(error, FILTERX_FUNCTION_ERROR, FILTERX_FUNCTION_ERROR_CTOR_FAIL,
                  "argument must be set: object. " FILTERX_FUNC_UNSET_EMPTIES_USAGE);
      return NULL;
    }

  return object_expr;
}

static gboolean
_extract_optional_args(FilterXFunctionUnsetEmpties *self, FilterXFunctionArgs *args, GError **error)
{
  gboolean exists, eval_error;
  gboolean value = filterx_function_args_get_named_literal_boolean(args, "recursive", &exists, &eval_error);
  if (!exists)
    return TRUE;

  if (eval_error)
    {
      g_set_error(error, FILTERX_FUNCTION_ERROR, FILTERX_FUNCTION_ERROR_CTOR_FAIL,
                  "recursive argument must be boolean literal. " FILTERX_FUNC_UNSET_EMPTIES_USAGE);
      return FALSE;
    }

  self->recursive = value;
  return TRUE;
}

static gboolean
_extract_args(FilterXFunctionUnsetEmpties *self, FilterXFunctionArgs *args, GError **error)
{
  if (filterx_function_args_len(args) != 1)
    {
      g_set_error(error, FILTERX_FUNCTION_ERROR, FILTERX_FUNCTION_ERROR_CTOR_FAIL,
                  "invalid number of arguments. " FILTERX_FUNC_UNSET_EMPTIES_USAGE);
      return FALSE;
    }

  self->object_expr = _extract_object_expr(args, error);
  if (!self->object_expr)
    return FALSE;

  if (!_extract_optional_args(self, args, error))
    return FALSE;

  return TRUE;
}

FilterXExpr *
filterx_function_unset_empties_new(FilterXFunctionArgs *args, GError **error)
{
  FilterXFunctionUnsetEmpties *self = g_new0(FilterXFunctionUnsetEmpties, 1);
  filterx_function_init_instance(&self->super, "unset_empties");
  self->super.super.eval = _eval;
  self->super.super.free_fn = _free;

  self->recursive = TRUE;

  if (!_extract_args(self, args, error))
    goto error;

  filterx_function_args_free(args);
  return &self->super.super;

error:
  filterx_function_args_free(args);
  filterx_expr_unref(&self->super.super);
  return NULL;
}
