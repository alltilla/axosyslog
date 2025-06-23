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

#ifndef PROTOBUF_FIELD_CONVERTER_HPP
#define PROTOBUF_FIELD_CONVERTER_HPP

#include "syslog-ng.h"

#include "compat/cpp-start.h"
#include "filterx/filterx-object.h"
#include "filterx/object-list-interface.h"
#include "compat/cpp-end.h"

#include "filterx-eval-exception.hpp"

#include <google/protobuf/message.h>
#include <google/protobuf/reflection.h>
#include <google/protobuf/descriptor.h>

#include <vector>

namespace syslogng {
namespace grpc {

struct ProtoReflectors
{
  class FieldNotFoundException : public std::out_of_range
  {
  public:
    FieldNotFoundException(const std::string &field_name) :
      std::out_of_range(std::string("Protobuf field does not exist, name: " + field_name)) {}
  };

  ProtoReflectors(const google::protobuf::Message &message, const std::string &field_name) :
    reflection(message.GetReflection()),
    descriptor(message.GetDescriptor()),
    field_descriptor(this->descriptor->FindFieldByName(field_name))
  {
    if (!this->field_descriptor)
      throw FieldNotFoundException(field_name);

    this->field_type = this->field_descriptor->type();
  };

  const char *
  field_type_name() const
  {
#if GOOGLE_PROTOBUF_VERSION >= 6030000
    return this->field_descriptor->type_name().data();
#else
    return this->field_descriptor->type_name();
#endif
  }

public:
  const google::protobuf::Reflection *reflection;
  const google::protobuf::Descriptor *descriptor;
  const google::protobuf::FieldDescriptor *field_descriptor;
  google::protobuf::FieldDescriptor::Type field_type;
};


class SingleProtobufFieldConverter
{
public:
  virtual ~SingleProtobufFieldConverter() {}

  virtual FilterXObject *get(google::protobuf::Message *message, ProtoReflectors reflectors) = 0;
  virtual void set(google::protobuf::Message *message, ProtoReflectors reflectors,
                   FilterXObject *object, FilterXObject **assoc_object) = 0;
  virtual void add(google::protobuf::Message *message, ProtoReflectors reflectors, FilterXObject *object) = 0;

  virtual void set_repeated(google::protobuf::Message *message, ProtoReflectors reflectors,
                            FilterXObject *object, FilterXObject **assoc_object);
};


class ProtobufFieldConverter
{
  public:
  class TypeNotSupportedException : public std::invalid_argument
  {
  public:
    TypeNotSupportedException(FilterXObject *object, const std::string &expected_type) :
      std::invalid_argument(std::string("FilterX type must be: " + expected_type + ", got: " + object->type->name)) {}
  };

  class Exception : public FilterXEvalException
  {
  public:
    using FilterXEvalException::FilterXEvalException;
    Exception(const gchar *message_, const std::string &info_) : FilterXEvalException(message_, info_, nullptr) {}
    Exception(const ProtoReflectors &reflectors, const gchar *message_, const std::string &info_) :
      FilterXEvalException(
        message_,
        std::string("name: ") + reflectors.field_descriptor->name() +
        ", type: " + reflectors.field_type_name() +
        ": " + info_,
        nullptr
      ) {}
  };

  class SetException : public Exception
  {
  public:
    SetException(const std::string &i) : Exception("Failed to set protobuf field", i) {}
    SetException(const ProtoReflectors &r, const std::string &i) : Exception(r, "Failed to set protobuf field", i) {}
  };

  class AddException : public Exception
  {
  public:
    AddException(const SetException &e) : Exception(e) {}
    AddException(const std::string &info_) : Exception("Failed to add protobuf field", info_) {}
    AddException(const ProtoReflectors &r, const std::string &i) : Exception(r, "Failed to add protobuf field", i) {}
  };

  class GetException : public Exception
  {
  public:
    GetException(const std::string &i) : Exception("Failed to get protobuf field", i) {}
    GetException(const ProtoReflectors &r, const std::string &i) : Exception(r, "Failed to get protobuf field", i) {}
  };

  class UnsetException : public Exception
  {
  public:
    UnsetException(const std::string &i) : Exception("Failed to unset protobuf field", i) {}
    UnsetException(const ProtoReflectors &r, const std::string &i) : Exception(r, "Failed to unset protobuf field", i) {}
  };

  class CheckException : public Exception
  {
  public:
    CheckException(const std::string &i) : Exception("Failed to check protobuf field", i) {}
    CheckException(const ProtoReflectors &r, const std::string &i) : Exception(r, "Failed to check protobuf field", i) {}
  };

public:
  FilterXObject *get(google::protobuf::Message *message, FilterXObject *field)
  {
    try
      {
        std::string field_name = extract_string_from_object(field);
        return ProtobufFieldConverter::get(message, field_name);
      }
    catch (const std::invalid_argument &e)
      {
        throw GetException(e.what());
      }
    }

  FilterXObject *get(google::protobuf::Message *message, const std::string &field_name)
  {
    try
      {
        ProtoReflectors reflectors(*message, field_name);
        return get_single_converter(reflectors.field_type)->get(message, reflectors);
      }
    catch (const ProtoReflectors::FieldNotFoundException &e)
      {
        throw GetException(e.what());
      }
  }

  void set(google::protobuf::Message *message, FilterXObject *field, FilterXObject *object,
                  FilterXObject **assoc_object = nullptr)
  {
    try
      {
        std::string field_name = extract_string_from_object(field);
        ProtobufFieldConverter::set(message, field_name, object, assoc_object);
      }
    catch (const std::invalid_argument &e)
      {
        throw SetException(e.what());
      }
  }

  void set(google::protobuf::Message *message, const std::string &field_name, FilterXObject *object,
                  FilterXObject **assoc_object = nullptr)
  {
    FilterXObject *local_assoc_object = nullptr;
    if (!assoc_object)
      assoc_object = &local_assoc_object;

    try
      {
        ProtoReflectors reflectors(*message, field_name);
        get_single_converter(reflectors.field_type)->set(message, reflectors, object, assoc_object);
      }
    catch (const ProtoReflectors::FieldNotFoundException &e)
      {
        throw SetException(e.what());
      }

    if (local_assoc_object)
      filterx_object_unref(local_assoc_object);
    else if (!(*assoc_object))
      *assoc_object = filterx_object_ref(object);
  }

  void set_repeated(google::protobuf::Message *message, FilterXObject *field, FilterXObject *object,
                           FilterXObject **assoc_object)
  {
    try
      {
        std::string field_name = extract_string_from_object(field);
        ProtoReflectors reflectors(*message, field_name);
        get_single_converter(reflectors.field_type)->set_repeated(message, reflectors, object, assoc_object);
      }
    catch (const std::invalid_argument &e)
      {
        throw SetException(e.what());
      }
    catch (const ProtoReflectors::FieldNotFoundException &e)
      {
        throw SetException(e.what());
      }
  }

  void unset(google::protobuf::Message *message, FilterXObject *field)
  {
    try
      {
        std::string field_name = extract_string_from_object(field);
        ProtoReflectors reflectors(*message, field_name);
        reflectors.reflection->ClearField(message, reflectors.field_descriptor);
      }
    catch (const std::invalid_argument &e)
      {
        throw UnsetException(e.what());
      }
    catch (const ProtoReflectors::FieldNotFoundException &e)
      {
        throw UnsetException(e.what());
      }
  }

  bool is_set(google::protobuf::Message *message, FilterXObject *field)
  {
    try
      {
        std::string field_name = extract_string_from_object(field);
        ProtoReflectors reflectors(*message, field_name);
        return reflectors.reflection->HasField(*message, reflectors.field_descriptor);
      }
    catch (const std::invalid_argument &e)
      {
        throw UnsetException(e.what());
      }
    catch (const ProtoReflectors::FieldNotFoundException &e)
      {
        throw UnsetException(e.what());
      }
  }

protected:
  SingleProtobufFieldConverter *get_single_converter(google::protobuf::FieldDescriptor::Type field_type);
};

extern ProtobufFieldConverter protobuf_field_converter;


class MapFieldConverter : public SingleProtobufFieldConverter
{
public:
  void set_repeated(google::protobuf::Message *message, ProtoReflectors reflectors, FilterXObject *object,
                    FilterXObject **assoc_object);

  FilterXObject *get(google::protobuf::Message *message, ProtoReflectors reflectors);
  void set(google::protobuf::Message *message, ProtoReflectors reflectors, FilterXObject *object,
           FilterXObject **assoc_object);
  void add(google::protobuf::Message *message, ProtoReflectors reflectors, FilterXObject *object);
};

extern MapFieldConverter map_field_converter;

std::string extract_string_from_object(FilterXObject *object);

uint64_t get_protobuf_message_set_field_count(const google::protobuf::Message &message);


}
}

#endif
