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

#ifndef OTEL_DICT_CONVERTER_HPP
#define OTEL_DICT_CONVERTER_HPP

#include "syslog-ng.h"

#include "compat/cpp-start.h"
#include "filterx/filterx-object.h"
#include "compat/cpp-end.h"

#include <google/protobuf/message.h>

namespace syslogng {
namespace grpc {
namespace otel {

/* Plain FilterX dict of any OTel message: keys are the proto field names,
 * only set fields appear, AnyValue is flattened to its value, repeated
 * KeyValue is a dict, *_unix_nano is a datetime, enums are integers. */
FilterXObject *otel_protobuf_message_to_filterx_dict(const google::protobuf::Message &message);

/* The inverse. Every key must be a field of the message, null leaves the
 * field unset and naming two members of one oneof is an error. */
bool otel_filterx_dict_to_protobuf_message(FilterXObject *dict, google::protobuf::Message &message);

}
}
}

#endif
