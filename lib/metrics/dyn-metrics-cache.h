/*
 * Copyright (c) 2023-2024 Attila Szakacs <attila.szakacs@axoflow.com>
 * Copyright (c) 2024 Balazs Scheidler <balazs.scheidler@axoflow.com>
 * Copyright (c) 2024 László Várady
 * Copyright (c) 2024 Axoflow
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

#ifndef DYN_METRICS_CACHE_H_INCLUDED
#define DYN_METRICS_CACHE_H_INCLUDED

#include "stats/stats-registry.h"
#include "dyn-metrics-store.h"

DynMetricsStore *dyn_metrics_cache(void);

void dyn_metrics_cache_global_init(void);
void dyn_metrics_cache_global_deinit(void);

#endif
