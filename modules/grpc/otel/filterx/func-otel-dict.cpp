/*
 * Copyright (c) 2026 Axoflow
 * Copyright (c) 2026 Attila Szakacs-Bertok <attila.szakacs@axoflow.com>
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

#include "func-otel-dict.h"
#include "otel-dict-converter.hpp"

#include "compat/cpp-start.h"
#include "filterx/object-extractor.h"
#include "filterx/object-string.h"
#include "filterx/filterx-eval.h"
#include "compat/cpp-end.h"

#include "opentelemetry/proto/resource/v1/resource.pb.h"
#include "opentelemetry/proto/common/v1/common.pb.h"
#include "opentelemetry/proto/logs/v1/logs.pb.h"
#include "opentelemetry/proto/metrics/v1/metrics.pb.h"
#include "opentelemetry/proto/trace/v1/trace.pb.h"

using namespace syslogng::grpc::otel;
using opentelemetry::proto::resource::v1::Resource;
using opentelemetry::proto::common::v1::InstrumentationScope;
using opentelemetry::proto::logs::v1::LogRecord;
using opentelemetry::proto::trace::v1::Span;
using opentelemetry::proto::metrics::v1::Metric;

template <typename M>
static FilterXObject *
_parse(FilterXExpr *s, FilterXObject *args[], gsize args_len)
{
  std::string type_name(M::descriptor()->name());

  if (!args || args_len != 1)
    {
      filterx_eval_push_error_info_printf("Failed to parse OTel message", "%s: requires exactly one argument",
                                          type_name.c_str());
      return NULL;
    }

  const gchar *data;
  gsize len;
  if (!filterx_object_extract_protobuf_ref(args[0], &data, &len)
      && !filterx_object_extract_bytes_ref(args[0], &data, &len))
    {
      filterx_eval_push_error_info_printf("Failed to parse OTel message", "%s: argument must be protobuf or bytes, got %s",
                                          type_name.c_str(), filterx_object_get_type_name(args[0]));
      return NULL;
    }

  M message;
  if (!message.ParsePartialFromArray(data, len))
    {
      filterx_eval_push_error_info_printf("Failed to parse OTel message", "%s: malformed protobuf data",
                                          type_name.c_str());
      return NULL;
    }

  return otel_protobuf_message_to_filterx_dict(message);
}

FILTERX_SIMPLE_FUNCTION(parse_otel_resource, _parse<Resource>);
FILTERX_SIMPLE_FUNCTION(parse_otel_scope, _parse<InstrumentationScope>);
FILTERX_SIMPLE_FUNCTION(parse_otel_logrecord, _parse<LogRecord>);
FILTERX_SIMPLE_FUNCTION(parse_otel_span, _parse<Span>);
FILTERX_SIMPLE_FUNCTION(parse_otel_metric, _parse<Metric>);

template <typename M>
static FilterXObject *
_format(FilterXExpr *s, FilterXObject *args[], gsize args_len)
{
  if (!args || args_len != 1)
    {
      filterx_eval_push_error_info_printf("Failed to format OTel message", "%s: requires exactly one argument",
                                          std::string(M::descriptor()->name()).c_str());
      return NULL;
    }

  M message;
  if (!otel_filterx_dict_to_protobuf_message(args[0], message))
    return NULL;

  std::string serialized = message.SerializeAsString();
  return filterx_protobuf_new(serialized.data(), serialized.size());
}

FILTERX_SIMPLE_FUNCTION(format_otel_resource, _format<Resource>);
FILTERX_SIMPLE_FUNCTION(format_otel_scope, _format<InstrumentationScope>);
FILTERX_SIMPLE_FUNCTION(format_otel_logrecord, _format<LogRecord>);
FILTERX_SIMPLE_FUNCTION(format_otel_span, _format<Span>);
FILTERX_SIMPLE_FUNCTION(format_otel_metric, _format<Metric>);
