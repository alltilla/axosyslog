/*
 * Copyright (c) 2024 Axoflow
 * Copyright (c) 2024 Attila Szakacs <attila.szakacs@axoflow.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

#include "metrics-probe-filterx.h"

#define FILTERX_FUNC_METRICS_PROBE_USAGE "metrics_probe(\"key\", increment=inc_int, labels={\"key\": \"value\"})"

typedef struct FilterXFunctionMetricsProbe_
{
  FilterXFunction super;
  gchar *key;
  FilterXExpr *increment_expr;
  FilterXExpr *labels_expr;
} FilterXFunctionMetricsProbe;

static FilterXObject *
_eval(FilterXExpr *s)
{
  /* TODO: implement */
  return NULL;
}

static void
_free(FilterXExpr *s)
{
  FilterXFunctionMetricsProbe *self = (FilterXFunctionMetricsProbe *) s;

  g_free(self->key);
  filterx_expr_unref(self->increment_expr);
  filterx_expr_unref(self->labels_expr);

  filterx_function_free_method(&self->super);
}

static gboolean
_extract_args(FilterXFunctionMetricsProbe *self, FilterXFunctionArgs *args, GError **error)
{
  if (filterx_function_args_len(args) != 1)
    {
      g_set_error(error, FILTERX_FUNCTION_ERROR, FILTERX_FUNCTION_ERROR_CTOR_FAIL,
                  "invalid number of arguments. " FILTERX_FUNC_METRICS_PROBE_USAGE);
      return FALSE;
    }

  self->key = g_strdup(filterx_function_args_get_literal_string(args, 0, NULL));
  if (!self->key)
    {
      g_set_error(error, FILTERX_FUNCTION_ERROR, FILTERX_FUNCTION_ERROR_CTOR_FAIL,
                  "key must be literal string. " FILTERX_FUNC_METRICS_PROBE_USAGE);
      return FALSE;
    }

  self->increment_expr = filterx_function_args_get_named_expr(args, "increment");
  self->labels_expr = filterx_function_args_get_named_expr(args, "labels");

  return TRUE;
}

FilterXFunction *
filterx_function_metrics_probe_new(const gchar *function_name, FilterXFunctionArgs *args, GError **error)
{
  FilterXFunctionMetricsProbe *self = g_new0(FilterXFunctionMetricsProbe, 1);
  filterx_function_init_instance(&self->super, function_name);

  self->super.super.eval = _eval;
  self->super.super.free_fn = _free;

  if (!_extract_args(self, args, error) ||
      !filterx_function_args_check(args, error))
    goto error;

  filterx_function_args_free(args);
  return &self->super;

error:
  filterx_function_args_free(args);
  filterx_expr_unref(&self->super.super);
  return NULL;
}

FILTERX_FUNCTION(metrics_probe, filterx_function_metrics_probe_new);
