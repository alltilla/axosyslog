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
#include "filterx/object-extractor.h"
#include "filterx/filterx-mapping.h"
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
using opentelemetry::proto::common::v1::KeyValueList;
using opentelemetry::proto::common::v1::ArrayValue;

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

/* dict -> message */

static bool _dict_to_message(FilterXObject *dict, Message *message);
static bool _any_value_from_object(AnyValue *any_value, FilterXObject *value);

static bool
_push_type_error(const FieldDescriptor *fd, FilterXObject *value, const char *expected)
{
  filterx_eval_push_error_info_printf("Failed to convert dict to OTel message",
                                      "Field %s expects %s, got %s",
                                      std::string(fd->full_name()).c_str(), expected,
                                      filterx_object_get_type_name(value));
  return false;
}

static bool
_extract_integer_in_range(const FieldDescriptor *fd, FilterXObject *value, gint64 min, gint64 max, gint64 *result)
{
  if (!filterx_object_extract_integer(value, result))
    return _push_type_error(fd, value, "an integer");

  if (*result < min || *result > max)
    {
      filterx_eval_push_error_info_printf("Failed to convert dict to OTel message",
                                          "Field %s expects an integer between %" G_GINT64_FORMAT
                                          " and %" G_GINT64_FORMAT ", got %" G_GINT64_FORMAT,
                                          std::string(fd->full_name()).c_str(), min, max, *result);
      return false;
    }
  return true;
}

static bool
_extract_timestamp(const FieldDescriptor *fd, FilterXObject *value, uint64_t *result)
{
  UnixTime utime;
  if (filterx_object_extract_datetime(value, &utime))
    {
      *result = unix_time_to_unix_epoch_nsec(utime);
      return true;
    }

  gint64 nsec;
  if (filterx_object_extract_integer(value, &nsec))
    {
      if (nsec < 0)
        return _push_type_error(fd, value, "a non-negative integer of nanoseconds");
      *result = nsec;
      return true;
    }

  return _push_type_error(fd, value, "a datetime or an integer of nanoseconds");
}

static bool
_extract_double(const FieldDescriptor *fd, FilterXObject *value, gdouble *result)
{
  if (filterx_object_extract_double(value, result))
    return true;

  gint64 i;
  if (filterx_object_extract_integer(value, &i))
    {
      *result = (gdouble) i;
      return true;
    }

  return _push_type_error(fd, value, "a number");
}

static bool
_set_key_value(KeyValue *key_value, FilterXObject *key, FilterXObject *value)
{
  const gchar *key_str;
  gsize key_len;
  if (!filterx_object_extract_string_ref(key, &key_str, &key_len))
    {
      filterx_eval_push_error_info_printf("Failed to convert dict to OTel message",
                                          "Attribute keys must be strings, got %s",
                                          filterx_object_get_type_name(key));
      return false;
    }

  key_value->set_key(key_str, key_len);
  return _any_value_from_object(key_value->mutable_value(), value);
}

static gboolean
_add_kvlist_entry(FilterXObject *key, FilterXObject *value, gpointer user_data)
{
  KeyValueList *kvlist = (KeyValueList *) user_data;
  return _set_key_value(kvlist->add_values(), key, value);
}

static bool
_any_value_from_object(AnyValue *any_value, FilterXObject *value)
{
  gboolean b;
  gint64 i;
  gdouble d;
  const gchar *str;
  gsize len;
  UnixTime utime;

  if (filterx_object_extract_null(value))
    {
      any_value->clear_value();
      return true;
    }
  if (filterx_object_extract_boolean(value, &b))
    {
      any_value->set_bool_value(b);
      return true;
    }
  if (filterx_object_extract_integer(value, &i))
    {
      any_value->set_int_value(i);
      return true;
    }
  if (filterx_object_extract_double(value, &d))
    {
      any_value->set_double_value(d);
      return true;
    }
  if (filterx_object_extract_string_ref(value, &str, &len))
    {
      any_value->set_string_value(str, len);
      return true;
    }
  if (filterx_object_extract_bytes_ref(value, &str, &len) || filterx_object_extract_protobuf_ref(value, &str, &len))
    {
      any_value->set_bytes_value(str, len);
      return true;
    }
  if (filterx_object_extract_datetime(value, &utime))
    {
      /* the same unit the otel_logrecord() object family stores a datetime attribute in */
      any_value->set_int_value(unix_time_to_unix_epoch_usec(utime));
      return true;
    }

  FilterXObject *unwrapped = filterx_ref_unwrap_ro(value);
  if (filterx_object_is_type(unwrapped, &FILTERX_TYPE_NAME(mapping)))
    return filterx_object_iter(unwrapped, _add_kvlist_entry, any_value->mutable_kvlist_value());

  if (filterx_object_is_type(unwrapped, &FILTERX_TYPE_NAME(sequence)))
    {
      ArrayValue *array = any_value->mutable_array_value();
      guint64 array_len;
      g_assert(filterx_object_len(unwrapped, &array_len));
      for (guint64 idx = 0; idx < array_len; idx++)
        {
          FilterXObject *element = filterx_sequence_get_subscript(unwrapped, idx);
          bool success = _any_value_from_object(array->add_values(), element);
          filterx_object_unref(element);
          if (!success)
            return false;
        }
      return true;
    }

  filterx_eval_push_error_info_printf("Failed to convert dict to OTel message",
                                      "Cannot convert %s to an AnyValue",
                                      filterx_object_get_type_name(value));
  return false;
}

/* add == true appends to a repeated field, otherwise sets a singular one */
static bool
_set_field_value(Message *message, const Reflection *reflection, const FieldDescriptor *fd, FilterXObject *value,
                 bool add)
{
  switch (fd->cpp_type())
    {
    case FieldDescriptor::CPPTYPE_INT32:
    {
      gint64 i;
      if (!_extract_integer_in_range(fd, value, G_MININT32, G_MAXINT32, &i))
        return false;
      add ? reflection->AddInt32(message, fd, i) : reflection->SetInt32(message, fd, i);
      return true;
    }
    case FieldDescriptor::CPPTYPE_INT64:
    {
      gint64 i;
      if (!_extract_integer_in_range(fd, value, G_MININT64, G_MAXINT64, &i))
        return false;
      add ? reflection->AddInt64(message, fd, i) : reflection->SetInt64(message, fd, i);
      return true;
    }
    case FieldDescriptor::CPPTYPE_UINT32:
    {
      gint64 i;
      if (!_extract_integer_in_range(fd, value, 0, G_MAXUINT32, &i))
        return false;
      add ? reflection->AddUInt32(message, fd, i) : reflection->SetUInt32(message, fd, i);
      return true;
    }
    case FieldDescriptor::CPPTYPE_UINT64:
    {
      uint64_t u;
      if (_is_timestamp_field(fd))
        {
          if (!_extract_timestamp(fd, value, &u))
            return false;
        }
      else
        {
          gint64 i;
          if (!_extract_integer_in_range(fd, value, 0, G_MAXINT64, &i))
            return false;
          u = i;
        }
      add ? reflection->AddUInt64(message, fd, u) : reflection->SetUInt64(message, fd, u);
      return true;
    }
    case FieldDescriptor::CPPTYPE_DOUBLE:
    {
      gdouble d;
      if (!_extract_double(fd, value, &d))
        return false;
      add ? reflection->AddDouble(message, fd, d) : reflection->SetDouble(message, fd, d);
      return true;
    }
    case FieldDescriptor::CPPTYPE_FLOAT:
    {
      gdouble d;
      if (!_extract_double(fd, value, &d))
        return false;
      add ? reflection->AddFloat(message, fd, (float) d) : reflection->SetFloat(message, fd, (float) d);
      return true;
    }
    case FieldDescriptor::CPPTYPE_BOOL:
    {
      gboolean b;
      if (!filterx_object_extract_boolean(value, &b))
        return _push_type_error(fd, value, "a boolean");
      add ? reflection->AddBool(message, fd, b) : reflection->SetBool(message, fd, b);
      return true;
    }
    case FieldDescriptor::CPPTYPE_ENUM:
    {
      /* proto3 enums are open, any int32 is valid on the wire */
      gint64 i;
      if (!_extract_integer_in_range(fd, value, G_MININT32, G_MAXINT32, &i))
        return false;
      add ? reflection->AddEnumValue(message, fd, i) : reflection->SetEnumValue(message, fd, i);
      return true;
    }
    case FieldDescriptor::CPPTYPE_STRING:
    {
      const gchar *str;
      gsize len;
      if (fd->type() == FieldDescriptor::TYPE_BYTES)
        {
          if (!filterx_object_extract_bytes_ref(value, &str, &len) && !filterx_object_extract_protobuf_ref(value, &str, &len))
            return _push_type_error(fd, value, "bytes");
        }
      else if (!filterx_object_extract_string_ref(value, &str, &len))
        return _push_type_error(fd, value, "a string");
      std::string string_value(str, len);
      add ? reflection->AddString(message, fd, string_value) : reflection->SetString(message, fd, string_value);
      return true;
    }
    case FieldDescriptor::CPPTYPE_MESSAGE:
    {
      Message *sub_message = add ? reflection->AddMessage(message, fd) : reflection->MutableMessage(message, fd);
      if (fd->message_type() == AnyValue::descriptor())
        return _any_value_from_object(static_cast<AnyValue *>(sub_message), value);
      return _dict_to_message(value, sub_message);
    }
    default:
      g_assert_not_reached();
    }
}

struct RepeatedKeyValueField
{
  Message *message;
  const Reflection *reflection;
  const FieldDescriptor *fd;
};

static gboolean
_add_attribute_entry(FilterXObject *key, FilterXObject *value, gpointer user_data)
{
  RepeatedKeyValueField *field = (RepeatedKeyValueField *) user_data;
  KeyValue *key_value = static_cast<KeyValue *>(field->reflection->AddMessage(field->message, field->fd));
  return _set_key_value(key_value, key, value);
}

static bool
_set_field(Message *message, const FieldDescriptor *fd, FilterXObject *value)
{
  const Reflection *reflection = message->GetReflection();

  if (filterx_object_extract_null(value))
    {
      /* null is how the dict reports an AnyValue without a value, keep that on the way back */
      if (!fd->is_repeated() && fd->cpp_type() == FieldDescriptor::CPPTYPE_MESSAGE
          && fd->message_type() == AnyValue::descriptor())
        reflection->MutableMessage(message, fd);
      return true;
    }

  const OneofDescriptor *oneof = fd->real_containing_oneof();
  if (oneof && reflection->HasOneof(*message, oneof))
    {
      filterx_eval_push_error_info_printf("Failed to convert dict to OTel message",
                                          "Field %s cannot be set together with %s, only one of the %s fields is allowed",
                                          std::string(fd->full_name()).c_str(),
                                          std::string(reflection->GetOneofFieldDescriptor(*message, oneof)->name()).c_str(),
                                          std::string(oneof->name()).c_str());
      return false;
    }

  if (!fd->is_repeated())
    return _set_field_value(message, reflection, fd, value, false);

  FilterXObject *unwrapped = filterx_ref_unwrap_ro(value);

  if (fd->cpp_type() == FieldDescriptor::CPPTYPE_MESSAGE && fd->message_type() == KeyValue::descriptor())
    {
      if (!filterx_object_is_type(unwrapped, &FILTERX_TYPE_NAME(mapping)))
        return _push_type_error(fd, value, "a dict");
      RepeatedKeyValueField field = { message, reflection, fd };
      return filterx_object_iter(unwrapped, _add_attribute_entry, &field);
    }

  if (!filterx_object_is_type(unwrapped, &FILTERX_TYPE_NAME(sequence)))
    return _push_type_error(fd, value, "a list");

  guint64 len;
  g_assert(filterx_object_len(unwrapped, &len));
  for (guint64 i = 0; i < len; i++)
    {
      FilterXObject *element = filterx_sequence_get_subscript(unwrapped, i);
      bool success = _set_field_value(message, reflection, fd, element, true);
      filterx_object_unref(element);
      if (!success)
        return false;
    }
  return true;
}

static gboolean
_set_field_from_dict_entry(FilterXObject *key, FilterXObject *value, gpointer user_data)
{
  Message *message = (Message *) user_data;

  const gchar *key_str;
  gsize key_len;
  if (!filterx_object_extract_string_ref(key, &key_str, &key_len))
    {
      filterx_eval_push_error_info_printf("Failed to convert dict to OTel message",
                                          "Field names must be strings, got %s",
                                          filterx_object_get_type_name(key));
      return FALSE;
    }

  const FieldDescriptor *fd = message->GetDescriptor()->FindFieldByName(std::string(key_str, key_len));
  if (!fd)
    {
      filterx_eval_push_error_info_printf("Failed to convert dict to OTel message",
                                          "Unknown field %.*s in %s",
                                          (int) key_len, key_str,
                                          std::string(message->GetDescriptor()->full_name()).c_str());
      return FALSE;
    }

  return _set_field(message, fd, value);
}

static bool
_dict_to_message(FilterXObject *dict, Message *message)
{
  FilterXObject *unwrapped = filterx_ref_unwrap_ro(dict);
  if (!filterx_object_is_type(unwrapped, &FILTERX_TYPE_NAME(mapping)))
    {
      filterx_eval_push_error_info_printf("Failed to convert dict to OTel message",
                                          "%s expects a dict, got %s",
                                          std::string(message->GetDescriptor()->full_name()).c_str(),
                                          filterx_object_get_type_name(dict));
      return false;
    }

  return filterx_object_iter(unwrapped, _set_field_from_dict_entry, message);
}

bool
syslogng::grpc::otel::otel_filterx_dict_to_protobuf_message(FilterXObject *dict, Message &message)
{
  return _dict_to_message(dict, &message);
}
