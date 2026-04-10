CC = clang
TARGET = clayterm.wasm
SRC = src/module.c

CFLAGS = --target=wasm32 -nostdlib -O2 \
         -DCLAY_IMPLEMENTATION -DCLAY_WASM \
         -Isrc -I.

LDFLAGS = -Wl,--no-entry \
          -Wl,--import-memory \
          -Wl,--stack-first \
          -Wl,--export-all \
          -Wl,--undefined=Clay__MeasureText \
          -Wl,--undefined=Clay__QueryScrollOffset

all: $(TARGET) wasm.ts
	@echo "Built $(TARGET) ($$(wc -c < $(TARGET)) bytes)"

DEPS = $(wildcard src/*.c src/*.h)

$(TARGET): $(DEPS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(SRC)

wasm.ts: $(TARGET)
	deno run --allow-read --allow-write tasks/bundle-wasm.ts

clean:
	rm -f $(TARGET) wasm.ts

.PHONY: all clean
