/*
 * Copyright (c) 2023 shifter
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

#include "protobuf-field-converter.hpp"

#include "compat/cpp-start.h"

#include "filterx/object-extractor.h"
#include "filterx/object-string.h"
#include "filterx/object-message-value.h"
#include "filterx/object-datetime.h"
#include "filterx/object-primitive.h"
#include "filterx/object-dict.h"
#include "scratch-buffers.h"
#include "generic-number.h"
#include "filterx/object-list-interface.h"
#include "filterx/object-dict-interface.h"
#include "filterx/json-repr.h"
#include "filterx/object-null.h"
#include "compat/cpp-end.h"

#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <memory>

using namespace syslogng::grpc;
using google::protobuf::Message;

void
log_type_error(ProtoReflectors reflectors, const char *type)
{
  msg_error("protobuf-field: Failed to convert field, type is unsupported",
            evt_tag_str("field", reflectors.field_descriptor->name().data()),
            evt_tag_str("expected_type", reflectors.field_type_name()),
            evt_tag_str("type", type));
}

float
double_to_float_safe(double val)
{
  if (val < (double)(-FLT_MAX))
    return -FLT_MAX;
  else if (val > (double)(FLT_MAX))
    return FLT_MAX;
  return (float)val;
}

/* C++ Implementations */

void
SingleProtobufFieldConverter::set_repeated(google::protobuf::Message *message, ProtoReflectors reflectors,
                                           FilterXObject *object, FilterXObject **assoc_object)
{
  if (!reflectors.field_descriptor->is_repeated())
    throw ProtobufFieldConverter::ProtobufFieldConverter::SetException(reflectors, "Field is not a repeated field");

  FilterXObject *list = filterx_ref_unwrap_ro(object);
  if (!filterx_object_is_type(list, &FILTERX_TYPE_NAME(list)))
    throw ProtobufFieldConverter::ProtobufFieldConverter::SetException(
      reflectors, "Expected a list object for repeated field, got: " + std::string(list->type->name)
    );

  reflectors.reflection->ClearField(message, reflectors.field_descriptor);

  guint64 len;
  g_assert(filterx_object_len(list, &len));

  for (gsize i = 0; i < len; i++)
    {
      FilterXObject *elem = filterx_list_get_subscript(list, i);

      try
        {
          this->add(message, reflectors, elem);
        }
      catch (const ProtobufFieldConverter::ProtobufFieldConverter::AddException &)
        {
          filterx_object_unref(elem);
          throw;
        }

      filterx_object_unref(elem);
    }

  *assoc_object = filterx_object_ref(object);
}

ProtobufFieldConverter syslogng::grpc::protobuf_field_converter;

class BoolFieldConverter : public SingleProtobufFieldConverter
{
private:
  static gboolean extract(FilterXObject *object)
  {
    return filterx_object_truthy(object);
  }

public:
  FilterXObject *get(Message *message, ProtoReflectors reflectors)
  {
    return filterx_boolean_new(reflectors.reflection->GetBool(*message, reflectors.field_descriptor));
  }

  void set(Message *message, ProtoReflectors reflectors, FilterXObject *object, FilterXObject **assoc_object)
  {
    reflectors.reflection->SetBool(message, reflectors.field_descriptor, this->extract(object));
  }

  void add(Message *message, ProtoReflectors reflectors, FilterXObject *object)
  {
    reflectors.reflection->AddBool(message, reflectors.field_descriptor, this->extract(object));
  }
};

class i32FieldConverter : public SingleProtobufFieldConverter
{
private:
  static int32_t extract(FilterXObject *object)
  {
    gint64 i;
    if (filterx_object_extract_integer(object, &i))
      return MAX(INT32_MIN, MIN(INT32_MAX, i));

    throw ProtobufFieldConverter::TypeNotSupportedException(object, "integer");
  }

  public:
  FilterXObject *get(Message *message, ProtoReflectors reflectors)
  {
    return filterx_integer_new(gint64(reflectors.reflection->GetInt64(*message, reflectors.field_descriptor)));
  }

  void set(Message *message, ProtoReflectors reflectors, FilterXObject *object, FilterXObject **assoc_object)
  {
    try
      {
        reflectors.reflection->SetInt32(message, reflectors.field_descriptor, this->extract(object));
      }
    catch (const ProtobufFieldConverter::TypeNotSupportedException &e)
      {
        throw ProtobufFieldConverter::SetException(reflectors, e.what());
      }
  }

  void add(Message *message, ProtoReflectors reflectors, FilterXObject *object)
  {
    try
      {
        reflectors.reflection->AddInt32(message, reflectors.field_descriptor, this->extract(object));
      }
    catch (const ProtobufFieldConverter::TypeNotSupportedException &e)
      {
        throw ProtobufFieldConverter::AddException(reflectors, e.what());
      }
  }
};

class i64FieldConverter : public SingleProtobufFieldConverter
{
private:
  static int64_t extract(FilterXObject *object)
  {
    gint64 i;
    if (filterx_object_extract_integer(object, &i))
      return i;

    UnixTime utime;
    if (filterx_object_extract_datetime(object, &utime))
      return (int64_t) unix_time_to_unix_epoch_usec(utime);

    throw ProtobufFieldConverter::TypeNotSupportedException(object, "integer or datetime");
  }

public:
  FilterXObject *get(Message *message, ProtoReflectors reflectors)
  {
    return filterx_integer_new(gint64(reflectors.reflection->GetInt64(*message, reflectors.field_descriptor)));
  }

  void set(Message *message, ProtoReflectors reflectors, FilterXObject *object, FilterXObject **assoc_object)
  {
    try
      {
        reflectors.reflection->SetInt64(message, reflectors.field_descriptor, this->extract(object));
      }
    catch (const ProtobufFieldConverter::TypeNotSupportedException &e)
      {
        throw ProtobufFieldConverter::SetException(reflectors, e.what());
      }
  }

  void add(Message *message, ProtoReflectors reflectors, FilterXObject *object)
  {
    try
      {
        reflectors.reflection->AddInt64(message, reflectors.field_descriptor, this->extract(object));
      }
    catch (const ProtobufFieldConverter::TypeNotSupportedException &e)
      {
        throw ProtobufFieldConverter::AddException(reflectors, e.what());
      }
  }
};

class u32FieldConverter : public SingleProtobufFieldConverter
{
private:
  static uint32_t extract(FilterXObject *object)
  {
    gint64 i;
    if (filterx_object_extract_integer(object, &i))
      return MAX(0, MIN(UINT32_MAX, i));

    throw ProtobufFieldConverter::TypeNotSupportedException(object, "integer");
  }

public:
  FilterXObject *get(Message *message, ProtoReflectors reflectors)
  {
    return filterx_integer_new(guint32(reflectors.reflection->GetUInt32(*message, reflectors.field_descriptor)));
  }

  void set(Message *message, ProtoReflectors reflectors, FilterXObject *object, FilterXObject **assoc_object)
  {
    try
      {
        reflectors.reflection->SetUInt32(message, reflectors.field_descriptor, this->extract(object));
      }
    catch (const ProtobufFieldConverter::TypeNotSupportedException &e)
      {
        throw ProtobufFieldConverter::SetException(reflectors, e.what());
      }
  }

  void add(Message *message, ProtoReflectors reflectors, FilterXObject *object)
  {
    try
      {
        reflectors.reflection->AddUInt32(message, reflectors.field_descriptor, this->extract(object));
      }
    catch (const ProtobufFieldConverter::TypeNotSupportedException &e)
      {
        throw ProtobufFieldConverter::AddException(reflectors, e.what());
      }
  }
};

class u64FieldConverter : public SingleProtobufFieldConverter
{
private:
  static uint64_t extract(FilterXObject *object)
  {
    gint64 i;
    if (filterx_object_extract_integer(object, &i))
      return MAX(0, MIN(UINT64_MAX, (uint64_t)i));
    throw ProtobufFieldConverter::TypeNotSupportedException(object, "integer");
  }

public:
  FilterXObject *get(Message *message, ProtoReflectors reflectors)
  {
    uint64_t val = reflectors.reflection->GetUInt64(*message, reflectors.field_descriptor);
    if (val > INT64_MAX)
      throw ProtobufFieldConverter::GetException(reflectors,
                         std::string("Field value exceeds FilterX integer value range: ") +
                         std::to_string(val) + " > " + std::to_string(INT64_MAX));
    return filterx_integer_new(guint64(val));
  }

  void set(Message *message, ProtoReflectors reflectors, FilterXObject *object, FilterXObject **assoc_object)
  {
    try
      {
        reflectors.reflection->SetUInt64(message, reflectors.field_descriptor, this->extract(object));
      }
    catch (const ProtobufFieldConverter::TypeNotSupportedException &e)
      {
        throw ProtobufFieldConverter::SetException(reflectors, e.what());
      }
  }

  void add(Message *message, ProtoReflectors reflectors, FilterXObject *object)
  {
    try
      {
        reflectors.reflection->AddUInt64(message, reflectors.field_descriptor, this->extract(object));
      }
    catch (const ProtobufFieldConverter::TypeNotSupportedException &e)
      {
        throw ProtobufFieldConverter::AddException(reflectors, e.what());
      }
  }
};

class StringFieldConverter : public SingleProtobufFieldConverter
{
private:
  static std::string extract(FilterXObject *object, ProtoReflectors reflectors)
  {
    const gchar *str;
    gsize len;

    if (filterx_object_extract_string_ref(object, &str, &len))
      return std::string(str, len);

    if (filterx_object_is_type(object, &FILTERX_TYPE_NAME(message_value)) &&
        filterx_message_value_get_type(object) == LM_VT_JSON)
      {
        str = filterx_message_value_get_value(object, &len);
        return std::string(str, len);
      }

    object = filterx_ref_unwrap_ro(object);
    if (filterx_object_is_type(object, &FILTERX_TYPE_NAME(dict)) ||
        filterx_object_is_type(object, &FILTERX_TYPE_NAME(list)))
      {
        GString *repr = scratch_buffers_alloc();
        if (!filterx_object_to_json(object, repr))
          throw std::runtime_error("JSON serialization error");
        return std::string(repr->str, repr->len);
      }

    throw ProtobufFieldConverter::TypeNotSupportedException(object, "string, dict or list");
  }

public:
  FilterXObject *get(Message *message, ProtoReflectors reflectors)
  {
    std::string bytes_buffer;
    const google::protobuf::Reflection *reflection = reflectors.reflection;
    const std::string &bytes_ref = reflection->GetStringReference(*message, reflectors.field_descriptor, &bytes_buffer);
    return filterx_string_new(bytes_ref.c_str(), bytes_ref.length());
  }

  void set(Message *message, ProtoReflectors reflectors, FilterXObject *object, FilterXObject **assoc_object)
  {
    try
      {
        reflectors.reflection->SetString(message, reflectors.field_descriptor, this->extract(object, reflectors));
      }
    catch (const ProtobufFieldConverter::TypeNotSupportedException &e)
      {
        throw ProtobufFieldConverter::SetException(reflectors, e.what());
      }
    catch (const std::runtime_error &e)
      {
        throw ProtobufFieldConverter::SetException(reflectors, e.what());
      }
  }

  void add(Message *message, ProtoReflectors reflectors, FilterXObject *object)
  {
    try
      {
        reflectors.reflection->AddString(message, reflectors.field_descriptor, this->extract(object, reflectors));
      }
      catch (const ProtobufFieldConverter::TypeNotSupportedException &e)
      {
        throw ProtobufFieldConverter::AddException(reflectors, e.what());
      }
    catch (const std::runtime_error &e)
      {
        throw ProtobufFieldConverter::AddException(reflectors, e.what());
      }
  }
};

class DoubleFieldConverter : public SingleProtobufFieldConverter
{
private:
  static double extract(FilterXObject *object)
  {
    gint64 i;
    if (filterx_object_extract_integer(object, &i))
      return static_cast<double>(i);

    gdouble d;
    if (filterx_object_extract_double(object, &d))
      return d;

    throw ProtobufFieldConverter::TypeNotSupportedException(object, "integer or double");
  }

public:
  FilterXObject *get(Message *message, ProtoReflectors reflectors)
  {
    return filterx_double_new(gdouble(reflectors.reflection->GetDouble(*message, reflectors.field_descriptor)));
  }

  void set(Message *message, ProtoReflectors reflectors, FilterXObject *object, FilterXObject **assoc_object)
  {
    try
      {
        reflectors.reflection->SetDouble(message, reflectors.field_descriptor, this->extract(object));
      }
    catch (const ProtobufFieldConverter::TypeNotSupportedException &e)
      {
        throw ProtobufFieldConverter::SetException(reflectors, e.what());
      }
  }

  void add(Message *message, ProtoReflectors reflectors, FilterXObject *object)
  {
    try
      {
        reflectors.reflection->AddDouble(message, reflectors.field_descriptor, this->extract(object));
      }
      catch (const ProtobufFieldConverter::TypeNotSupportedException &e)
      {
        throw ProtobufFieldConverter::AddException(reflectors, e.what());
      }
  }
};

class FloatFieldConverter : public SingleProtobufFieldConverter
{
private:
  static float extract(FilterXObject *object)
  {
    gint64 i;
    if (filterx_object_extract_integer(object, &i))
      return double_to_float_safe(static_cast<double>(i));

    gdouble d;
    if (filterx_object_extract_double(object, &d))
      return double_to_float_safe(d);

    throw ProtobufFieldConverter::TypeNotSupportedException(object, "integer or double");
  }

public:
  FilterXObject *get(Message *message, ProtoReflectors reflectors)
  {
    return filterx_double_new(gdouble(reflectors.reflection->GetFloat(*message, reflectors.field_descriptor)));
  }

  void set(Message *message, ProtoReflectors reflectors, FilterXObject *object, FilterXObject **assoc_object)
  {
    try
      {
        reflectors.reflection->SetFloat(message, reflectors.field_descriptor, this->extract(object));
      }
    catch (const ProtobufFieldConverter::TypeNotSupportedException &e)
      {
        throw ProtobufFieldConverter::SetException(reflectors, e.what());
      }
  }

  void add(Message *message, ProtoReflectors reflectors, FilterXObject *object)
  {
    try
      {
        reflectors.reflection->AddFloat(message, reflectors.field_descriptor, this->extract(object));
      }
    catch (const ProtobufFieldConverter::TypeNotSupportedException &e)
      {
        throw ProtobufFieldConverter::AddException(reflectors, e.what());
      }
  }
};

class BytesFieldConverter : public SingleProtobufFieldConverter
{
private:
  static std::string extract(FilterXObject *object)
  {
    const gchar *str;
    gsize len;

    if (filterx_object_extract_bytes_ref(object, &str, &len) ||
        filterx_object_extract_protobuf_ref(object, &str, &len))
      return std::string{str, len};

    throw ProtobufFieldConverter::TypeNotSupportedException(object, "bytes or protobuf");
  }

public:
  FilterXObject *get(Message *message, ProtoReflectors reflectors)
  {
    std::string bytes_buffer;
    const google::protobuf::Reflection *reflection = reflectors.reflection;
    const std::string &bytes_ref = reflection->GetStringReference(*message, reflectors.field_descriptor, &bytes_buffer);
    return filterx_bytes_new(bytes_ref.c_str(), bytes_ref.length());
  }

  void set(Message *message, ProtoReflectors reflectors, FilterXObject *object, FilterXObject **assoc_object)
  {
    try
      {
        reflectors.reflection->SetString(message, reflectors.field_descriptor, this->extract(object));
      }
    catch (const ProtobufFieldConverter::TypeNotSupportedException &e)
      {
        throw ProtobufFieldConverter::SetException(reflectors, e.what());
      }
  }

  void add(Message *message, ProtoReflectors reflectors, FilterXObject *object)
  {
    try
      {
        reflectors.reflection->AddString(message, reflectors.field_descriptor, this->extract(object));
      }
    catch (const ProtobufFieldConverter::TypeNotSupportedException &e)
      {
        throw ProtobufFieldConverter::AddException(reflectors, e.what());
      }
  }
};

FilterXObject *
MapFieldConverter::get(Message *message, ProtoReflectors reflectors)
{
  FilterXObject *dict = filterx_dict_new();
  FilterXObject *key_object = nullptr, *value_object = nullptr;

  try
    {
      int len = reflectors.reflection->FieldSize(*message, reflectors.field_descriptor);
      for (int i = 0; i < len; i++)
        {
          Message *elem_message = reflectors.reflection->MutableRepeatedMessage(message, reflectors.field_descriptor, i);

          try
            {
              key_object = protobuf_field_converter.get(elem_message, "key");
            }
          catch (const ProtobufFieldConverter::GetException &e)
            {
              std::throw_with_nested(ProtobufFieldConverter::GetException(reflectors, "Failed to convert map key"));
            }

          try
            {
              value_object = protobuf_field_converter.get(elem_message, "value");
            }
          catch (const ProtobufFieldConverter::GetException &e)
            {
              std::throw_with_nested(ProtobufFieldConverter::GetException(reflectors, "Failed to convert map value"));
            }

          if (!filterx_object_set_subscript(dict, key_object, &value_object))
            {
              throw ProtobufFieldConverter::GetException(reflectors, "Failed to set element of FilterX dict");
            }

          filterx_object_unref(key_object);
          filterx_object_unref(value_object);
          key_object = value_object = nullptr;
        }
    }
  catch (...)
    {
      filterx_object_unref(value_object);
      filterx_object_unref(key_object);
      filterx_object_unref(dict);
      throw;
    }

    return dict;
}

static gboolean
_map_add_elem(FilterXObject *key, FilterXObject *value, gpointer user_data)
{
  google::protobuf::Message *message = static_cast<google::protobuf::Message *>(((gpointer *) user_data)[0]);
  ProtoReflectors *reflectors = static_cast<ProtoReflectors *>(((gpointer *) user_data)[1]);

  Message *elem_message = reflectors->reflection->AddMessage(message, reflectors->field_descriptor);

  try
    {
      protobuf_field_converter.set(elem_message, "key", key);
    }
  catch (const ProtobufFieldConverter::SetException &e)
    {
      std::throw_with_nested(ProtobufFieldConverter::SetException(*reflectors, "Failed to convert map key"));
    }

  try
    {
      protobuf_field_converter.set(elem_message, "value", value);
    }
  catch (const ProtobufFieldConverter::SetException &e)
    {
      std::throw_with_nested(ProtobufFieldConverter::SetException(*reflectors, "Failed to convert map value"));
    }

  return TRUE;
}

void
MapFieldConverter::set_repeated(Message *message, ProtoReflectors reflectors, FilterXObject *object,
                                FilterXObject **assoc_object)
{
  FilterXObject *dict = filterx_ref_unwrap_ro(object);
  if (!filterx_object_is_type(dict, &FILTERX_TYPE_NAME(dict)))
    throw ProtobufFieldConverter::SetException(reflectors, "Expected a dict object for map field, got: " + std::string(dict->type->name));

  gpointer user_data[] = { message, &reflectors };
  filterx_dict_iter(dict, _map_add_elem, user_data);
}

void
MapFieldConverter::set(Message *message, ProtoReflectors reflectors, FilterXObject *object,
                       FilterXObject **assoc_object)
{
  /* Map is always repeated. */
  g_assert_not_reached();
}

void
MapFieldConverter::add(Message *message, ProtoReflectors reflectors, FilterXObject *object)
{
  /* Map has its own set_repeated implementation. */
  g_assert_not_reached();
}

static gboolean
_message_add_elem(FilterXObject *key, FilterXObject *value, gpointer user_data)
{
  google::protobuf::Message *message = static_cast<google::protobuf::Message *>(((gpointer *) user_data)[0]);
  ProtoReflectors *reflectors = static_cast<ProtoReflectors *>(((gpointer *) user_data)[1]);

  try
    {
      protobuf_field_converter.set(message, key, value);
    }
  catch (const ProtobufFieldConverter::SetException &e)
    {
      std::throw_with_nested(ProtobufFieldConverter::SetException(*reflectors, "Failed to convert map value"));
    }

  return TRUE;
}

class MessageFieldConverter : public SingleProtobufFieldConverter
{
public:
  FilterXObject *get(Message *message, ProtoReflectors reflectors)
  {
    if (reflectors.field_descriptor->is_map())
      return map_field_converter.get(message, reflectors);

    FilterXObject *dict = filterx_dict_new();

    int len = reflectors.reflection->FieldSize(*message, reflectors.field_descriptor);
    for (int i = 0; i < len; i++)
      {
        Message *elem_message = reflectors.reflection->MutableRepeatedMessage(message, reflectors.field_descriptor, i);

        const std::string &field_name = reflectors.field_descriptor->name();

        FilterXObject *value_object = protobuf_field_converter.get(elem_message, field_name);
        if (!value_object)
          return NULL;

        FilterXObject *key_object = filterx_string_new(field_name.c_str(), field_name.length());
        if (!filterx_object_set_subscript(dict, key_object, &value_object))
          {
            filterx_object_unref(key_object);
            filterx_object_unref(value_object);
            return NULL; // TODO
          }

        filterx_object_unref(key_object);
        filterx_object_unref(value_object);
      }

    return dict;
  }

  void set_repeated(Message *message, ProtoReflectors reflectors, FilterXObject *object,
                    FilterXObject **assoc_object)
  {
    if (reflectors.field_descriptor->is_map())
      return map_field_converter.set_repeated(message, reflectors, object, assoc_object);
    return SingleProtobufFieldConverter::set_repeated(message, reflectors, object, assoc_object);
  }

  void set(Message *message, ProtoReflectors reflectors, FilterXObject *object, FilterXObject **assoc_object)
  {
    FilterXObject *dict = filterx_ref_unwrap_ro(object);
    if (!filterx_object_is_type(dict, &FILTERX_TYPE_NAME(dict)))
      throw ProtobufFieldConverter::SetException(reflectors, "Expected a dict object for message field, got: " + std::string(dict->type->name));

    try
      {
        Message *inner_message = reflectors.reflection->MutableMessage(message, reflectors.field_descriptor);
        gpointer user_data[] = { inner_message, &reflectors };
        filterx_dict_iter(dict, _message_add_elem, user_data);
      }
    catch (const ProtobufFieldConverter::SetException &e)
      {
        throw ProtobufFieldConverter::AddException(e);
      }
  }

  void add(Message *message, ProtoReflectors reflectors, FilterXObject *object)
  {
    FilterXObject *dict = filterx_ref_unwrap_ro(object);
    if (!filterx_object_is_type(dict, &FILTERX_TYPE_NAME(dict)))
      throw ProtobufFieldConverter::SetException(reflectors, "Expected a dict object for message field, got: " + std::string(dict->type->name));

    try
      {
        Message *inner_message = reflectors.reflection->AddMessage(message, reflectors.field_descriptor);
        gpointer user_data[] = { inner_message, &reflectors };
        filterx_dict_iter(dict, _message_add_elem, user_data);
      }
    catch (const ProtobufFieldConverter::SetException &e)
      {
        throw ProtobufFieldConverter::AddException(e);
      }
  }
};

MapFieldConverter syslogng::grpc::map_field_converter;

SingleProtobufFieldConverter *
syslogng::grpc::ProtobufFieldConverter::get_single_converter(google::protobuf::FieldDescriptor::Type field_type)
{
  g_assert(field_type <= google::protobuf::FieldDescriptor::MAX_TYPE && field_type > 0);
  static std::unique_ptr<SingleProtobufFieldConverter> converters[google::protobuf::FieldDescriptor::MAX_TYPE] =
  {
    std::make_unique<DoubleFieldConverter>(),  // TYPE_DOUBLE = 1,       double, exactly eight bytes on the wire.
    std::make_unique<FloatFieldConverter>(),   // TYPE_FLOAT = 2,        float, exactly four bytes on the wire.
    std::make_unique<i64FieldConverter>(),     // TYPE_INT64 = 3,        int64, varint on the wire.
    //                                                                   Negative numbers take 10 bytes.
    //                                                                   Use TYPE_SINT64 if negative values are likely.
    std::make_unique<u64FieldConverter>(),     // TYPE_UINT64 = 4,       uint64, varint on the wire.
    std::make_unique<i32FieldConverter>(),     // TYPE_INT32 = 5,        int32, varint on the wire.
    //                                                                   Negative numbers take 10 bytes.
    //                                                                   Use TYPE_SINT32 if negative values are likely.
    std::make_unique<u64FieldConverter>(),     // TYPE_FIXED64 = 6,      uint64, exactly eight bytes on the wire.
    std::make_unique<u32FieldConverter>(),     // TYPE_FIXED32 = 7,      uint32, exactly four bytes on the wire.
    std::make_unique<BoolFieldConverter>(),    // TYPE_BOOL = 8,         bool, varint on the wire.
    std::make_unique<StringFieldConverter>(),  // TYPE_STRING = 9,       UTF-8 text.
    nullptr,                                   // TYPE_GROUP = 10,       Tag-delimited message.  Deprecated.
    std::make_unique<MessageFieldConverter>(), // TYPE_MESSAGE = 11,     Length-delimited message.
    std::make_unique<BytesFieldConverter>(),   // TYPE_BYTES = 12,       Arbitrary byte array.
    std::make_unique<u32FieldConverter>(),     // TYPE_UINT32 = 13,      uint32, varint on the wire
    nullptr,                                   // TYPE_ENUM = 14,        Enum, varint on the wire
    std::make_unique<i32FieldConverter>(),     // TYPE_SFIXED32 = 15,    int32, exactly four bytes on the wire
    std::make_unique<i64FieldConverter>(),     // TYPE_SFIXED64 = 16,    int64, exactly eight bytes on the wire
    std::make_unique<i32FieldConverter>(),     // TYPE_SINT32 = 17,      int32, ZigZag-encoded varint on the wire
    std::make_unique<i64FieldConverter>(),     // TYPE_SINT64 = 18,      int64, ZigZag-encoded varint on the wire
  };
  return converters[field_type - 1].get();
}

std::string
syslogng::grpc::extract_string_from_object(FilterXObject *object)
{
  const gchar *key_c_str;
  gsize len;

  if (!filterx_object_extract_string_ref(object, &key_c_str, &len))
    throw std::invalid_argument("not a string instance");

  return std::string{key_c_str, len};
}

uint64_t
syslogng::grpc::get_protobuf_message_set_field_count(const Message &message)
{
  const google::protobuf::Reflection *reflection = message.GetReflection();
  std::vector<const google::protobuf::FieldDescriptor *> fields;
  reflection->ListFields(message, &fields);
  return fields.size();
}
