if ENABLE_KAFKA
module_LTLIBRARIES += modules/kafka/libkafka.la

modules_kafka_libkafka_la_CFLAGS  = \
  $(AM_CFLAGS) \
  $(LIBRDKAFKA_CFLAGS)

modules_kafka_libkafka_la_SOURCES = \
  modules/kafka/kafka-grammar.y \
  modules/kafka/kafka-parser.c \
  modules/kafka/kafka-parser.h \
  modules/kafka/kafka-props.c \
  modules/kafka/kafka-props.h \
  modules/kafka/kafka-dest-driver.h \
  modules/kafka/kafka-dest-driver.c \
  modules/kafka/kafka-dest-worker.h \
  modules/kafka/kafka-dest-worker.c \
  modules/kafka/kafka-internal.h \
  modules/kafka/kafka-plugin.c

modules_kafka_libkafka_la_CPPFLAGS  = \
  $(AM_CPPFLAGS) \
  $(LIBRDKAFKA_CFLAGS) \
  -I$(top_srcdir)/modules/kafka \
  -I$(top_builddir)/modules/kafka

modules_kafka_libkafka_la_LIBADD  = \
  $(MODULE_DEPS_LIBS) \
  $(LIBRDKAFKA_LIBS)

modules_kafka_libkafka_la_LDFLAGS = $(MODULE_LDFLAGS)

EXTRA_modules_kafka_libkafka_la_DEPENDENCIES = \
  $(MODULE_DEPS_LIBS)

modules/kafka modules/kafka/ mod-kafka: \
	modules/kafka/libkafka.la
else
modules/kafka modules/kafka/ mod-kafka:
endif

BUILT_SOURCES += \
  modules/kafka/kafka-grammar.y \
  modules/kafka/kafka-grammar.c \
  modules/kafka/kafka-grammar.h

EXTRA_DIST += \
  modules/kafka/kafka-grammar.ym \
  modules/kafka/CMakeLists.txt

.PHONY: modules/kafka/ mod-kafka

include modules/kafka/tests/Makefile.am
