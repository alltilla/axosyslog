/*
 * Copyright (c) 2023 shifter
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

#ifndef FILTERX_EXPR_FUNCTION_H_INCLUDED
#define FILTERX_EXPR_FUNCTION_H_INCLUDED

#include "filterx/filterx-expr.h"
#include "filterx-object.h"

typedef FilterXObject *(*FilterXSimpleFunctionProto)(GPtrArray *);

typedef struct _FilterXFunction
{
  FilterXExpr super;
  gchar *function_name;
} FilterXFunction;

typedef struct _FilterXFunctionArgs FilterXFunctionArgs;

typedef FilterXFunction *(*FilterXFunctionCtor)(const gchar *, FilterXFunctionArgs *, GError **);

#define FILTERX_FUNCTION_ERROR filterx_function_error_quark()
GQuark filterx_function_error_quark(void);

enum FilterXFunctionError
{
  FILTERX_FUNCTION_ERROR_FUNCTION_NOT_FOUND,
  FILTERX_FUNCTION_ERROR_CTOR_FAIL,
};

void filterx_function_init_instance(FilterXFunction *s, const gchar *function_name);
void filterx_function_free_method(FilterXFunction *s);

FilterXFunctionArgs *filterx_function_args_new(GList *positional_exprs);
guint64 filterx_function_args_len(FilterXFunctionArgs *self);
FilterXExpr *filterx_function_args_get_expr(FilterXFunctionArgs *self, guint64 index);
FilterXObject *filterx_function_args_get_object(FilterXFunctionArgs *self, guint64 index);
const gchar *filterx_function_args_get_literal_string(FilterXFunctionArgs *self, guint64 index, gsize *len);
gboolean filterx_function_args_is_literal_null(FilterXFunctionArgs *self, guint64 index);
void filterx_function_args_free(FilterXFunctionArgs *self);

FilterXExpr *filterx_function_lookup(GlobalConfig *cfg, const gchar *function_name, GList *positional_args,
                                     GError **error);

#endif
