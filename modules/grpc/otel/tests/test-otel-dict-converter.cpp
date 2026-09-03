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

#include "filterx/otel-dict-converter.hpp"
#include "filterx/func-otel-dict.h"

#include "compat/cpp-start.h"
#include "apphook.h"
#include "libtest/filterx-lib.h"
#include "filterx/json-repr.h"
#include "filterx/object-dict.h"
#include "filterx/object-string.h"
#include "filterx/object-datetime.h"
#include "filterx/filterx-eval.h"
#include "filterx/filterx-private.h"
#include "compat/cpp-end.h"

#include "opentelemetry/proto/common/v1/common.pb.h"
#include "opentelemetry/proto/resource/v1/resource.pb.h"
#include "opentelemetry/proto/logs/v1/logs.pb.h"
#include "opentelemetry/proto/metrics/v1/metrics.pb.h"
#include "opentelemetry/proto/trace/v1/trace.pb.h"

#include <criterion/criterion.h>

using namespace syslogng::grpc::otel;
using namespace opentelemetry::proto::common::v1;
using namespace opentelemetry::proto::resource::v1;
using namespace opentelemetry::proto::logs::v1;
using namespace opentelemetry::proto::metrics::v1;
using namespace opentelemetry::proto::trace::v1;

static const uint64_t T1 = 1111111111000000000;
static const uint64_t T2 = 1111111112000000000;

static void
_assert_dict(const google::protobuf::Message &message, const gchar *expected_json)
{
  FilterXObject *dict = otel_protobuf_message_to_filterx_dict(message);
  cr_assert(dict);
  assert_object_json_equals(dict, expected_json);

  google::protobuf::Message *round_tripped = message.New();
  cr_assert(otel_filterx_dict_to_protobuf_message(dict, *round_tripped));
  cr_assert_eq(round_tripped->SerializeAsString().compare(message.SerializeAsString()), 0);

  delete round_tripped;
  filterx_object_unref(dict);
}

static bool
_format_json(const gchar *json, google::protobuf::Message &message)
{
  FilterXObject *dict = filterx_object_from_json(json, -1, NULL);
  cr_assert(dict);
  bool success = otel_filterx_dict_to_protobuf_message(dict, message);
  filterx_object_unref(dict);
  return success;
}

#define SIMPLE_FN(name) ((FilterXSimpleFunctionProto) filterx_function_##name##_construct(NULL))

/* arg may be NULL, its reference is taken over */
static FilterXObject *
_call(FilterXSimpleFunctionProto fn, FilterXObject *arg)
{
  GList *args = arg ? g_list_append(NULL, filterx_function_arg_new(NULL, filterx_object_expr_new(arg))) : NULL;
  GError *error = NULL;
  FilterXExpr *expr = filterx_simple_function_new("test", filterx_function_args_new(args, NULL), fn, &error);
  cr_assert_null(error);

  FilterXObject *result = init_and_eval_expr(expr);
  if (!result)
    filterx_eval_clear_errors();
  filterx_expr_unref(expr);
  return result;
}

static void
_set_dict_entry(FilterXObject *dict, const gchar *key, FilterXObject *value)
{
  FilterXObject *key_object = filterx_string_new(key, -1);
  cr_assert(filterx_object_set_subscript(dict, key_object, &value));
  filterx_object_unref(key_object);
  filterx_object_unref(value);
}

static KeyValue *
_add_attribute(google::protobuf::RepeatedPtrField<KeyValue> *attributes, const char *key)
{
  KeyValue *key_value = attributes->Add();
  key_value->set_key(key);
  return key_value;
}

Test(otel_dict_converter, empty_message)
{
  _assert_dict(Resource(), "{}");
}

Test(otel_dict_converter, resource_with_every_any_value_kind)
{
  Resource resource;
  _add_attribute(resource.mutable_attributes(), "string")->mutable_value()->set_string_value("foo");
  _add_attribute(resource.mutable_attributes(), "bool")->mutable_value()->set_bool_value(true);
  _add_attribute(resource.mutable_attributes(), "int")->mutable_value()->set_int_value(42);
  _add_attribute(resource.mutable_attributes(), "double")->mutable_value()->set_double_value(1.5);
  _add_attribute(resource.mutable_attributes(), "bytes")->mutable_value()->set_bytes_value("foo");
  KeyValue *nested = _add_attribute(resource.mutable_attributes(), "kvlist")
                     ->mutable_value()->mutable_kvlist_value()->add_values();
  nested->set_key("nested");
  nested->mutable_value()->set_string_value("bar");
  ArrayValue *array = _add_attribute(resource.mutable_attributes(), "array")->mutable_value()->mutable_array_value();
  array->add_values()->set_string_value("a");
  array->add_values()->set_int_value(1);
  _add_attribute(resource.mutable_attributes(), "unset")->mutable_value();
  resource.set_dropped_attributes_count(3);

  _assert_dict(resource,
               "{\"attributes\":{\"string\":\"foo\",\"bool\":true,\"int\":42,\"double\":1.5,\"bytes\":\"Zm9v\","
               "\"kvlist\":{\"nested\":\"bar\"},\"array\":[\"a\",1],\"unset\":null},"
               "\"dropped_attributes_count\":3}");
}

Test(otel_dict_converter, scope)
{
  InstrumentationScope scope;
  scope.set_name("scope");
  scope.set_version("1.0");
  _add_attribute(scope.mutable_attributes(), "a")->mutable_value()->set_string_value("b");
  scope.set_dropped_attributes_count(1);

  _assert_dict(scope,
               "{\"name\":\"scope\",\"version\":\"1.0\",\"attributes\":{\"a\":\"b\"},\"dropped_attributes_count\":1}");
}

Test(otel_dict_converter, log_record)
{
  LogRecord log_record;
  log_record.set_time_unix_nano(T1);
  log_record.set_observed_time_unix_nano(T2);
  log_record.set_severity_number(SEVERITY_NUMBER_INFO);
  log_record.set_severity_text("INFO");
  log_record.mutable_body()->set_string_value("hello");
  _add_attribute(log_record.mutable_attributes(), "a")->mutable_value()->set_int_value(1);
  log_record.set_dropped_attributes_count(1);
  log_record.set_flags(1);
  log_record.set_trace_id(std::string("\x01\x02", 2));
  log_record.set_span_id(std::string("\x03", 1));

  _assert_dict(log_record,
               "{\"time_unix_nano\":\"1111111111.000000\",\"severity_number\":9,\"severity_text\":\"INFO\","
               "\"body\":\"hello\",\"attributes\":{\"a\":1},\"dropped_attributes_count\":1,\"flags\":1,"
               "\"trace_id\":\"AQI=\",\"span_id\":\"Aw==\",\"observed_time_unix_nano\":\"1111111112.000000\"}");
}

Test(otel_dict_converter, span)
{
  Span span;
  span.set_trace_id(std::string("\x01\x02", 2));
  span.set_span_id(std::string("\x03", 1));
  span.set_trace_state("k=v");
  span.set_parent_span_id(std::string("\x04", 1));
  span.set_name("GET /");
  span.set_kind(Span::SPAN_KIND_SERVER);
  span.set_start_time_unix_nano(T1);
  span.set_end_time_unix_nano(T2);
  _add_attribute(span.mutable_attributes(), "http.method")->mutable_value()->set_string_value("GET");
  span.set_dropped_attributes_count(1);

  Span::Event *event = span.add_events();
  event->set_time_unix_nano(1111111111500000000);
  event->set_name("ev");
  _add_attribute(event->mutable_attributes(), "e")->mutable_value()->set_bool_value(true);
  event->set_dropped_attributes_count(2);
  span.set_dropped_events_count(3);

  Span::Link *link = span.add_links();
  link->set_trace_id(std::string("\x05", 1));
  link->set_span_id(std::string("\x06", 1));
  link->set_trace_state("x=y");
  _add_attribute(link->mutable_attributes(), "l")->mutable_value()->set_int_value(2);
  link->set_dropped_attributes_count(4);
  span.set_dropped_links_count(5);

  span.mutable_status()->set_message("boom");
  span.mutable_status()->set_code(Status::STATUS_CODE_ERROR);

  _assert_dict(span,
               "{\"trace_id\":\"AQI=\",\"span_id\":\"Aw==\",\"trace_state\":\"k=v\",\"parent_span_id\":\"BA==\","
               "\"name\":\"GET /\",\"kind\":2,\"start_time_unix_nano\":\"1111111111.000000\","
               "\"end_time_unix_nano\":\"1111111112.000000\",\"attributes\":{\"http.method\":\"GET\"},"
               "\"dropped_attributes_count\":1,"
               "\"events\":[{\"time_unix_nano\":\"1111111111.500000\",\"name\":\"ev\",\"attributes\":{\"e\":true},"
               "\"dropped_attributes_count\":2}],\"dropped_events_count\":3,"
               "\"links\":[{\"trace_id\":\"BQ==\",\"span_id\":\"Bg==\",\"trace_state\":\"x=y\",\"attributes\":{\"l\":2},"
               "\"dropped_attributes_count\":4}],\"dropped_links_count\":5,"
               "\"status\":{\"message\":\"boom\",\"code\":2}}");
}

Test(otel_dict_converter, metric_gauge)
{
  Metric metric;
  metric.set_name("g");
  metric.set_description("d");
  metric.set_unit("1");

  NumberDataPoint *data_point = metric.mutable_gauge()->add_data_points();
  data_point->set_start_time_unix_nano(T1);
  data_point->set_time_unix_nano(T2);
  data_point->set_as_double(1.5);
  Exemplar *exemplar = data_point->add_exemplars();
  exemplar->set_time_unix_nano(T1);
  exemplar->set_as_int(7);
  exemplar->set_span_id(std::string("\x03", 1));
  exemplar->set_trace_id(std::string("\x01\x02", 2));
  _add_attribute(exemplar->mutable_filtered_attributes(), "f")->mutable_value()->set_string_value("x");
  _add_attribute(data_point->mutable_attributes(), "a")->mutable_value()->set_int_value(1);
  data_point->set_flags(1);

  data_point = metric.mutable_gauge()->add_data_points();
  data_point->set_time_unix_nano(T2);
  data_point->set_as_int(3);

  _assert_dict(metric,
               "{\"name\":\"g\",\"description\":\"d\",\"unit\":\"1\",\"gauge\":{\"data_points\":["
               "{\"start_time_unix_nano\":\"1111111111.000000\",\"time_unix_nano\":\"1111111112.000000\","
               "\"as_double\":1.5,\"exemplars\":[{\"time_unix_nano\":\"1111111111.000000\",\"span_id\":\"Aw==\","
               "\"trace_id\":\"AQI=\",\"as_int\":7,\"filtered_attributes\":{\"f\":\"x\"}}],"
               "\"attributes\":{\"a\":1},\"flags\":1},"
               "{\"time_unix_nano\":\"1111111112.000000\",\"as_int\":3}]}}");
}

Test(otel_dict_converter, metric_sum)
{
  Metric metric;
  metric.set_name("s");
  NumberDataPoint *data_point = metric.mutable_sum()->add_data_points();
  data_point->set_time_unix_nano(T2);
  data_point->set_as_int(5);
  metric.mutable_sum()->set_aggregation_temporality(AGGREGATION_TEMPORALITY_CUMULATIVE);
  metric.mutable_sum()->set_is_monotonic(true);

  _assert_dict(metric,
               "{\"name\":\"s\",\"sum\":{\"data_points\":[{\"time_unix_nano\":\"1111111112.000000\",\"as_int\":5}],"
               "\"aggregation_temporality\":2,\"is_monotonic\":true}}");
}

Test(otel_dict_converter, metric_histogram)
{
  Metric metric;
  metric.set_name("h");
  HistogramDataPoint *data_point = metric.mutable_histogram()->add_data_points();
  data_point->set_start_time_unix_nano(T1);
  data_point->set_time_unix_nano(T2);
  data_point->set_count(3);
  data_point->set_sum(6.5);
  data_point->add_bucket_counts(1);
  data_point->add_bucket_counts(2);
  data_point->add_explicit_bounds(0.5);
  _add_attribute(data_point->mutable_attributes(), "h")->mutable_value()->set_string_value("x");
  data_point->set_flags(1);
  data_point->set_min(0.5);
  data_point->set_max(2.5);
  metric.mutable_histogram()->set_aggregation_temporality(AGGREGATION_TEMPORALITY_DELTA);

  _assert_dict(metric,
               "{\"name\":\"h\",\"histogram\":{\"data_points\":[{\"start_time_unix_nano\":\"1111111111.000000\","
               "\"time_unix_nano\":\"1111111112.000000\",\"count\":3,\"sum\":6.5,\"bucket_counts\":[1,2],"
               "\"explicit_bounds\":[0.5],\"attributes\":{\"h\":\"x\"},\"flags\":1,\"min\":0.5,\"max\":2.5}],"
               "\"aggregation_temporality\":1}}");
}

Test(otel_dict_converter, metric_exponential_histogram)
{
  Metric metric;
  metric.set_name("eh");
  ExponentialHistogramDataPoint *data_point = metric.mutable_exponential_histogram()->add_data_points();
  _add_attribute(data_point->mutable_attributes(), "e")->mutable_value()->set_int_value(1);
  data_point->set_time_unix_nano(T2);
  data_point->set_count(4);
  data_point->set_sum(2.5);
  data_point->set_scale(-1);
  data_point->set_zero_count(1);
  data_point->mutable_positive()->set_offset(2);
  data_point->mutable_positive()->add_bucket_counts(1);
  data_point->mutable_positive()->add_bucket_counts(1);
  data_point->mutable_negative()->set_offset(-3);
  data_point->mutable_negative()->add_bucket_counts(1);
  data_point->set_flags(1);
  data_point->set_min(0.5);
  data_point->set_max(1.5);
  data_point->set_zero_threshold(0.25);
  metric.mutable_exponential_histogram()->set_aggregation_temporality(AGGREGATION_TEMPORALITY_CUMULATIVE);

  _assert_dict(metric,
               "{\"name\":\"eh\",\"exponential_histogram\":{\"data_points\":[{\"attributes\":{\"e\":1},"
               "\"time_unix_nano\":\"1111111112.000000\",\"count\":4,\"sum\":2.5,\"scale\":-1,\"zero_count\":1,"
               "\"positive\":{\"offset\":2,\"bucket_counts\":[1,1]},\"negative\":{\"offset\":-3,\"bucket_counts\":[1]},"
               "\"flags\":1,\"min\":0.5,\"max\":1.5,\"zero_threshold\":0.25}],\"aggregation_temporality\":2}}");
}

Test(otel_dict_converter, metric_summary)
{
  Metric metric;
  metric.set_name("sm");
  SummaryDataPoint *data_point = metric.mutable_summary()->add_data_points();
  data_point->set_time_unix_nano(T2);
  data_point->set_count(2);
  data_point->set_sum(3.5);
  SummaryDataPoint::ValueAtQuantile *quantile = data_point->add_quantile_values();
  quantile->set_quantile(0.5);
  quantile->set_value(1.5);
  quantile = data_point->add_quantile_values();
  quantile->set_quantile(0.75);
  quantile->set_value(2.5);
  _add_attribute(data_point->mutable_attributes(), "s")->mutable_value()->set_string_value("x");
  data_point->set_flags(1);

  _assert_dict(metric,
               "{\"name\":\"sm\",\"summary\":{\"data_points\":[{\"time_unix_nano\":\"1111111112.000000\",\"count\":2,"
               "\"sum\":3.5,\"quantile_values\":[{\"quantile\":0.5,\"value\":1.5},{\"quantile\":0.75,\"value\":2.5}],"
               "\"attributes\":{\"s\":\"x\"},\"flags\":1}]}}");
}

Test(otel_dict_converter, uint64_above_int64_max_fails)
{
  Metric metric;
  metric.mutable_histogram()->add_data_points()->set_count(UINT64_MAX);

  cr_assert_not(otel_protobuf_message_to_filterx_dict(metric));
}

Test(otel_dict_converter, format_unknown_key_fails)
{
  LogRecord log_record;
  cr_assert_not(_format_json("{\"severity_numbre\": 9}", log_record));
}

Test(otel_dict_converter, format_two_members_of_a_oneof_fails)
{
  Metric metric;
  cr_assert_not(_format_json("{\"gauge\": {}, \"sum\": {}}", metric));

  NumberDataPoint data_point;
  cr_assert_not(_format_json("{\"as_int\": 1, \"as_double\": 1.5}", data_point));
}

Test(otel_dict_converter, format_type_mismatch_fails)
{
  Resource resource;
  cr_assert_not(_format_json("{\"dropped_attributes_count\": \"one\"}", resource));
  cr_assert_not(_format_json("{\"dropped_attributes_count\": -1}", resource));
  cr_assert_not(_format_json("{\"attributes\": [1]}", resource));

  Span span;
  cr_assert_not(_format_json("{\"name\": 1}", span));
  cr_assert_not(_format_json("{\"trace_id\": \"not bytes\"}", span));
  cr_assert_not(_format_json("{\"events\": {}}", span));
  cr_assert_not(_format_json("{\"status\": []}", span));
  cr_assert_not(_format_json("{\"start_time_unix_nano\": \"now\"}", span));
}

Test(otel_dict_converter, format_null_leaves_the_field_unset)
{
  LogRecord log_record;
  cr_assert(_format_json("{\"body\": null, \"severity_text\": null, \"attributes\": {\"a\": null}}", log_record));

  cr_assert(log_record.has_body());
  cr_assert_eq(log_record.body().value_case(), AnyValue::VALUE_NOT_SET);
  cr_assert(log_record.severity_text().empty());
  cr_assert_eq(log_record.attributes_size(), 1);
  cr_assert_eq(log_record.attributes(0).value().value_case(), AnyValue::VALUE_NOT_SET);

  _assert_dict(log_record, "{\"body\":null,\"attributes\":{\"a\":null}}");
}

Test(otel_dict_converter, format_accepts_any_enum_value_and_integer_timestamps)
{
  Span span;
  cr_assert(_format_json("{\"kind\": 99, \"start_time_unix_nano\": 5}", span));
  cr_assert_eq((int) span.kind(), 99);
  cr_assert_eq(span.start_time_unix_nano(), 5);
}

Test(otel_dict_converter, format_proto3_optional_field)
{
  HistogramDataPoint data_point;
  cr_assert(_format_json("{\"sum\": 1.5, \"count\": 2}", data_point));
  cr_assert(data_point.has_sum());
  cr_assert_not(data_point.has_min());
}

Test(otel_dict_converter, format_datetime_attribute_as_microseconds)
{
  UnixTime utime = unix_time_from_unix_epoch_usec(1111111111000005);
  FilterXObject *attributes = filterx_dict_new();
  _set_dict_entry(attributes, "ts", filterx_datetime_new(&utime));
  FilterXObject *dict = filterx_dict_new();
  _set_dict_entry(dict, "attributes", attributes);

  Resource resource;
  cr_assert(otel_filterx_dict_to_protobuf_message(dict, resource));
  cr_assert_eq(resource.attributes(0).value().int_value(), 1111111111000005);

  filterx_object_unref(dict);
}

Test(otel_dict_converter, parse_otel_function)
{
  LogRecord log_record;
  log_record.set_time_unix_nano(T1);
  log_record.mutable_body()->set_string_value("hello");
  std::string serialized = log_record.SerializeAsString();

  FilterXObject *dict = _call(SIMPLE_FN(parse_otel_logrecord),
                              filterx_protobuf_new(serialized.data(), serialized.size()));
  cr_assert(dict);
  assert_object_json_equals(dict, "{\"time_unix_nano\":\"1111111111.000000\",\"body\":\"hello\"}");
  filterx_object_unref(dict);

  dict = _call(SIMPLE_FN(parse_otel_logrecord), filterx_bytes_new(serialized.data(), serialized.size()));
  cr_assert(dict);
  filterx_object_unref(dict);
}

Test(otel_dict_converter, parse_otel_function_rejects_bad_arguments)
{
  cr_assert_not(_call(SIMPLE_FN(parse_otel_span), NULL));
  cr_assert_not(_call(SIMPLE_FN(parse_otel_span), filterx_string_new("not protobuf", -1)));
  cr_assert_not(_call(SIMPLE_FN(parse_otel_span), filterx_bytes_new("\xff\xff\xff", 3)));
}

static void
setup(void)
{
  app_startup();
  init_libtest_filterx();
}

static void
teardown(void)
{
  deinit_libtest_filterx();
  app_shutdown();
}

TestSuite(otel_dict_converter, .init = setup, .fini = teardown);
