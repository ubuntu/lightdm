# Makefile for LightDM Performance Benchmarks

CC ?= gcc
CFLAGS ?= -O3 -g -Wall -Wextra -D_GNU_SOURCE
GLIB_CFLAGS = $(shell pkg-config --cflags glib-2.0 gio-2.0 gobject-2.0)
GLIB_LIBS   = $(shell pkg-config --libs glib-2.0 gio-2.0 gobject-2.0) -lm

INCLUDES = -I. -I../common -I../src -I..

COMMON_LIB = ../common/.libs/libcommon.a
XAUTH_OBJ  = ../src/lightdm-x-authority.o
XDMCP_OBJ  = ../src/lightdm-xdmcp-protocol.o

SRCS = benchmark-framework.c \
       bench-config.c \
       bench-user-list.c \
       bench-xauthority.c \
       bench-xdmcp.c \
       lightdm-benchmark.c

OBJS = $(SRCS:.c=.o)
TARGET = lightdm-benchmark

all: $(TARGET)

$(TARGET): $(OBJS) $(COMMON_LIB) $(XAUTH_OBJ) $(XDMCP_OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(XAUTH_OBJ) $(XDMCP_OBJ) $(COMMON_LIB) $(GLIB_LIBS)

%.o: %.c benchmark-framework.h
	$(CC) $(CFLAGS) $(INCLUDES) $(GLIB_CFLAGS) -c $< -o $@

$(COMMON_LIB):
	$(MAKE) -C ../common

$(XAUTH_OBJ) $(XDMCP_OBJ):
	$(MAKE) -C ../src

run: $(TARGET)
	./$(TARGET)

quick: $(TARGET)
	./$(TARGET) --warmup=2 --iterations=5

clean:
	rm -f $(OBJS) $(TARGET) baseline.json

.PHONY: all run quick clean
