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

#include "otel-dict-converter.hpp"

#include "compat/cpp-start.h"
#include "filterx/object-dict.h"
#include "filterx/object-list.h"
#include "filterx/object-string.h"
#include "filterx/object-primitive.h"
#include "filterx/object-datetime.h"
#include "filterx/object-null.h"
#include "filterx/filterx-sequence.h"
#include "filterx/filterx-eval.h"
#include "timeutils/unixtime.h"
#include "compat/cpp-end.h"

#include "opentelemetry/proto/common/v1/common.pb.h"

#include <string.h>
#include <vector>

using namespace google::protobuf;
using opentelemetry::proto::common::v1::AnyValue;
using opentelemetry::proto::common::v1::KeyValue;

static FilterXObject *_message_to_dict(const Message &message);

template <typename S>
static bool
_ends_with(const S &str, const char *suffix)
{
  size_t suffix_len = strlen(suffix);
  return str.size() >= suffix_len && memcmp(str.data() + str.size() - suffix_len, suffix, suffix_len) == 0;
}

/* OTLP timestamps are fixed64 nanoseconds, named *_unix_nano */
static bool
_is_timestamp_field(const FieldDescriptor *fd)
{
  return fd->type() == FieldDescriptor::TYPE_FIXED64 && _ends_with(fd->name(), "_unix_nano");
}

/* takes over the reference of value */
static bool
_set_dict_entry(FilterXObject *dict, const char *key_str, size_t key_len, FilterXObject *value)
{
  if (!value)
    return false;

  FILTERX_STRING_DECLARE_ON_STACK(key, key_str, key_len);
  gboolean success = filterx_object_set_subscript(dict, key, &value);
  filterx_object_unref(value);
  FILTERX_STRING_CLEAR_FROM_STACK(key);
  return success;
}

/* takes over the reference of value */
static bool
_append_list_element(FilterXObject *list, FilterXObject *value)
{
  if (!value)
    return false;

  gboolean success = filterx_sequence_append(list, &value);
  filterx_object_unref(value);
  return success;
}

static FilterXObject *_any_value_to_object(const AnyValue &any_value);

static bool
_add_key_value_to_dict(FilterXObject *dict, const KeyValue &key_value)
{
  return _set_dict_entry(dict, key_value.key().data(), key_value.key().size(),
                         _any_value_to_object(key_value.value()));
}

static FilterXObject *
_any_value_to_object(const AnyValue &any_value)
{
  switch (any_value.value_case())
    {
    case AnyValue::kStringValue:
      return filterx_string_new(any_value.string_value().data(), any_value.string_value().size());
    case AnyValue::kBoolValue:
      return filterx_boolean_new(any_value.bool_value());
    case AnyValue::kIntValue:
      return filterx_integer_new(any_value.int_value());
    case AnyValue::kDoubleValue:
      return filterx_double_new(any_value.double_value());
    case AnyValue::kBytesValue:
      return filterx_bytes_new(any_value.bytes_value().data(), any_value.bytes_value().size());
    case AnyValue::kKvlistValue:
    {
      FilterXObject *dict = filterx_dict_new();
      for (const KeyValue &key_value : any_value.kvlist_value().values())
        {
          if (!_add_key_value_to_dict(dict, key_value))
            {
              filterx_object_unref(dict);
              return NULL;
            }
        }
      return dict;
    }
    case AnyValue::kArrayValue:
    {
      FilterXObject *list = filterx_list_new();
      for (const AnyValue &element : any_value.array_value().values())
        {
          if (!_append_list_element(list, _any_value_to_object(element)))
            {
              filterx_object_unref(list);
              return NULL;
            }
        }
      return list;
    }
    case AnyValue::VALUE_NOT_SET:
      return filterx_null_new();
    default:
      g_assert_not_reached();
    }
}

static FilterXObject *
_uint64_to_object(const FieldDescriptor *fd, uint64_t value)
{
  if (_is_timestamp_field(fd))
    {
      UnixTime utime = unix_time_from_unix_epoch_nsec(value);
      return filterx_datetime_new(&utime);
    }

  if (value > INT64_MAX)
    {
      filterx_eval_push_error_info_printf("Failed to convert OTel field",
                                          "Value of %s exceeds the FilterX integer range: %" G_GUINT64_FORMAT,
                                          std::string(fd->name()).c_str(), value);
      return NULL;
    }

  return filterx_integer_new((gint64) value);
}

/* index < 0 addresses a singular field, otherwise an element of a repeated one */
static FilterXObject *
_field_value_to_object(const Message &message, const Reflection *reflection, const FieldDescriptor *fd, int index)
{
  switch (fd->cpp_type())
    {
    case FieldDescriptor::CPPTYPE_INT32:
      return filterx_integer_new(index < 0 ? reflection->GetInt32(message, fd)
                                 : reflection->GetRepeatedInt32(message, fd, index));
    case FieldDescriptor::CPPTYPE_INT64:
      return filterx_integer_new(index < 0 ? reflection->GetInt64(message, fd)
                                 : reflection->GetRepeatedInt64(message, fd, index));
    case FieldDescriptor::CPPTYPE_UINT32:
      return filterx_integer_new(index < 0 ? reflection->GetUInt32(message, fd)
                                 : reflection->GetRepeatedUInt32(message, fd, index));
    case FieldDescriptor::CPPTYPE_UINT64:
      return _uint64_to_object(fd, index < 0 ? reflection->GetUInt64(message, fd)
                               : reflection->GetRepeatedUInt64(message, fd, index));
    case FieldDescriptor::CPPTYPE_DOUBLE:
      return filterx_double_new(index < 0 ? reflection->GetDouble(message, fd)
                                : reflection->GetRepeatedDouble(message, fd, index));
    case FieldDescriptor::CPPTYPE_FLOAT:
      return filterx_double_new((gdouble) (index < 0 ? reflection->GetFloat(message, fd)
                                           : reflection->GetRepeatedFloat(message, fd, index)));
    case FieldDescriptor::CPPTYPE_BOOL:
      return filterx_boolean_new(index < 0 ? reflection->GetBool(message, fd)
                                 : reflection->GetRepeatedBool(message, fd, index));
    case FieldDescriptor::CPPTYPE_ENUM:
      return filterx_integer_new(index < 0 ? reflection->GetEnumValue(message, fd)
                                 : reflection->GetRepeatedEnumValue(message, fd, index));
    case FieldDescriptor::CPPTYPE_STRING:
    {
      std::string scratch;
      const std::string &value = index < 0 ? reflection->GetStringReference(message, fd, &scratch)
                                 : reflection->GetRepeatedStringReference(message, fd, index, &scratch);
      if (fd->type() == FieldDescriptor::TYPE_BYTES)
        return filterx_bytes_new(value.data(), value.size());
      return filterx_string_new(value.data(), value.size());
    }
    case FieldDescriptor::CPPTYPE_MESSAGE:
    {
      const Message &sub_message = index < 0 ? reflection->GetMessage(message, fd)
                                   : reflection->GetRepeatedMessage(message, fd, index);
      if (fd->message_type() == AnyValue::descriptor())
        return _any_value_to_object(static_cast<const AnyValue &>(sub_message));
      return _message_to_dict(sub_message);
    }
    default:
      g_assert_not_reached();
    }
}

static FilterXObject *
_field_to_object(const Message &message, const Reflection *reflection, const FieldDescriptor *fd)
{
  if (!fd->is_repeated())
    return _field_value_to_object(message, reflection, fd, -1);

  int size = reflection->FieldSize(message, fd);

  if (fd->cpp_type() == FieldDescriptor::CPPTYPE_MESSAGE && fd->message_type() == KeyValue::descriptor())
    {
      FilterXObject *dict = filterx_dict_new();
      for (int i = 0; i < size; i++)
        {
          const KeyValue &key_value = static_cast<const KeyValue &>(reflection->GetRepeatedMessage(message, fd, i));
          if (!_add_key_value_to_dict(dict, key_value))
            {
              filterx_object_unref(dict);
              return NULL;
            }
        }
      return dict;
    }

  FilterXObject *list = filterx_list_new();
  for (int i = 0; i < size; i++)
    {
      if (!_append_list_element(list, _field_value_to_object(message, reflection, fd, i)))
        {
          filterx_object_unref(list);
          return NULL;
        }
    }
  return list;
}

/* no recursion depth limit here: input nesting is already bounded by the
 * protobuf parser's recursion limit (100 by default) */
static FilterXObject *
_message_to_dict(const Message &message)
{
  const Reflection *reflection = message.GetReflection();
  std::vector<const FieldDescriptor *> fields;
  reflection->ListFields(message, &fields);

  FilterXObject *dict = filterx_dict_new();
  for (const FieldDescriptor *fd : fields)
    {
      if (!_set_dict_entry(dict, fd->name().data(), fd->name().size(), _field_to_object(message, reflection, fd)))
        {
          filterx_object_unref(dict);
          return NULL;
        }
    }
  return dict;
}

FilterXObject *
syslogng::grpc::otel::otel_protobuf_message_to_filterx_dict(const Message &message)
{
  return _message_to_dict(message);
}
