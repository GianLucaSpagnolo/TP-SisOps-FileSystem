CC = gcc
CFLAGS := -ggdb3 -O2 -Wall -std=c11
CFLAGS += -Wno-unused-function -Wvla

# Flags for FUSE
LDLIBS := $(shell pkg-config fuse --cflags --libs)

# Name for the filesystem!
FS_NAME := fisopfs

TEST_NAME := fs_test

TEST_FILES := ./fs.dat

# por cada módulo se agrega un nuevo item
# ejemplo:
#   si además tenemos un archivo llamado file.c
#   la siguiente linea quedaría
# $(FS_NAME): fs.o file.o
$(FS_NAME): fs_lib.o 

$(TEST_NAME): fs_test.o fs_lib.o 

all: build
	
build: $(FS_NAME)

test: $(TEST_NAME)
	./$(TEST_NAME)

format: .clang-files .clang-format
	xargs -r clang-format -i <$<

docker-build:
	docker build -t fisopfs:latest .

docker-run:
	docker container run -it --rm --name=fisopfs \
		-v $(shell pwd):/fisopfs \
		--device /dev/fuse \
		--cap-add SYS_ADMIN \
		--security-opt apparmor:unconfined \
		fisopfs:latest \
		bash 

docker-attach:
	docker exec -it fisopfs bash

clean:
	rm -rf $(EXEC) *.o core vgcore.* $(FS_NAME)

.PHONY: all build clean format docker-build docker-run docker-attach
