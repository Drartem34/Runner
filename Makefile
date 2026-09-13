TARGET = runner
CXX = g++
CXXFLAGS = -O3 $(shell pkg-config --cflags gtk4 gtk4-layer-shell-0)
LDFLAGS = $(shell pkg-config --libs gtk4 gtk4-layer-shell-0)

all: $(TARGET)

$(TARGET): runner.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

install: $(TARGET)
	install -Dm755 $(TARGET) ~/.config/SYSui/runner_osd

clean:
	rm -f $(TARGET)

.PHONY: all install clean
