/*
 * Copyright (c) 2024 Axoflow
 * Copyright (c) 2024 Attila Szakacs <attila.szakacs@axoflow.com>
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

#include "filterx-metrics.h"
#include "filterx-metrics-labels.h"
#include "expr-literal.h"
#include "object-string.h"
#include "filterx-eval.h"
#include "stats/stats-cluster-single.h"
#include "stats/stats-registry.h"
#include "metrics/metrics-tls-cache.h"
#include "scratch-buffers.h"

struct _FilterXMetrics
{
  struct {
    FilterXExpr *expr;
    gchar *str;
  } key;

  FilterXMetricsLabels *labels;
  gint level;

  StatsCluster *const_cluster;
};

gboolean
filterx_metrics_is_enabled(FilterXMetrics *self)
{
  return stats_check_level(self->level);
}

static const gchar *
_format_sck_name(FilterXMetrics *self)
{
  if (self->key.str)
    return self->key.str;

  FilterXObject *key_obj = filterx_expr_eval_typed(self->key.expr);
  if (!key_obj)
    {
      msg_error("filterx: failed to format metrics key", filterx_format_last_error());
      return NULL;
    }

  gsize len;
  const gchar *name = filterx_string_get_value(key_obj, &len);
  if (len == 0)
    {
      msg_error("filterx: failed to format metrics key: key must be a non-empty string");
      goto exit;
    }

  GString *name_buffer = scratch_buffers_alloc();
  g_string_append(name_buffer, name);
  name = name_buffer->str;

exit:
  filterx_object_unref(key_obj);
  return name;
}

static gboolean
_format_sck(FilterXMetrics *self, StatsClusterKey *sck)
{
  sck->name = _format_sck_name(self);
  if (!sck->name)
    return FALSE;

  // TODO: error
  sck->labels = filterx_metrics_labels_format(self->labels, &sck->labels_len, NULL);
  if (!sck->labels)
    return FALSE;

  return TRUE;
}

StatsCounterItem *
filterx_metrics_get_stats_counter(FilterXMetrics *self)
{
  if (self->const_cluster)
    return stats_cluster_single_get_counter(self->const_cluster);

  StatsCounterItem *counter = NULL;

  ScratchBuffersMarker marker;
  scratch_buffers_mark(&marker);

  StatsClusterKey sck;
  if (!_format_sck(self, &sck))
    goto exit;

  counter = metrics_tls_cache_get_counter(&sck, self->level);

exit:
  scratch_buffers_reclaim_marked(marker);
  return counter;
}

void
filterx_metrics_free(FilterXMetrics *self)
{
  filterx_expr_unref(self->key.expr);
  g_free(self->key.str);

  if (self->labels)
    filterx_metrics_labels_free(self->labels);

  stats_lock();
  {
    StatsCounterItem *counter = stats_cluster_single_get_counter(self->const_cluster);
    stats_unregister_dynamic_counter(self->const_cluster, SC_TYPE_SINGLE_VALUE, &counter);
  }
  stats_unlock();

  g_free(self);
}

static gboolean
_init_key(FilterXMetrics *self, FilterXExpr *key, GError **error)
{
  if (!key)
    {
      // error
      return FALSE;
    }

  if (!filterx_expr_is_literal(key))
    {
      self->key.expr = filterx_expr_ref(key);
      return TRUE;
    }

  FilterXObject *key_obj = filterx_expr_eval_typed(key);
  if (!filterx_object_is_type(key_obj, &FILTERX_TYPE_NAME(string)))
    {
      // error;
      filterx_object_unref(key_obj);
      return FALSE;
    }

  self->key.str = g_strdup(filterx_string_get_value(key_obj, NULL));
  return TRUE;
}

static gboolean
_init_labels(FilterXMetrics *self, FilterXExpr *labels, GError **error)
{
  self->labels = filterx_metrics_labels_new(labels, error);
  return !!self->labels;
}

static void
_optimize(FilterXMetrics *self)
{
  if (!self->key.str || filterx_metrics_labels_is_const(self->labels))
    return;

  ScratchBuffersMarker marker;
  scratch_buffers_mark(&marker);

  StatsClusterKey sck;
  g_assert(_format_sck(self, &sck));

  stats_lock();
  {
    StatsCounterItem *counter;
    self->const_cluster = stats_register_dynamic_counter(self->level, &sck, SC_TYPE_SINGLE_VALUE, &counter);
  }
  stats_unlock();

  scratch_buffers_reclaim_marked(marker);

  g_free(self->key.str);
  self->key.str = NULL;

  filterx_metrics_labels_free(self->labels);
  self->labels = NULL;
}

FilterXMetrics *
filterx_metrics_new(gint level, FilterXExpr *key, FilterXExpr *labels, GError **error)
{
  FilterXMetrics *self = g_new0(FilterXMetrics, 1);

  self->level = level;

  if (!_init_key(self, key, error))
    goto error;

  if (!_init_labels(self, key, error))
    goto error;

  _optimize(self);

error:
  filterx_metrics_free(self);
  return NULL;
}
