CXX ?= clang++
CXXFLAGS ?= -std=c++17 -O2 -Iinclude

all: bin/selftest bin/tracktest bin/calibtest

bin:
	mkdir -p bin

bin/selftest: tests/selftest.cpp include/mouse_to_stick.h | bin
	$(CXX) $(CXXFLAGS) -o $@ tests/selftest.cpp

bin/tracktest: tests/tracktest.cpp include/m2s_track2d.h | bin
	$(CXX) $(CXXFLAGS) -o $@ tests/tracktest.cpp

bin/calibtest: tests/calibtest.cpp include/m2s_calib.h include/mouse_to_stick.h | bin
	$(CXX) $(CXXFLAGS) -o $@ tests/calibtest.cpp

# macOS specific test harnesses using GameController & SceneKit
ifeq ($(shell uname), Darwin)
all: bin/m2s_test bin/sim3d

bin/m2s_test: tests/m2s_test.mm src/macMouseBridge.mm include/macMouseBridge.h include/mouse_to_stick.h | bin
	$(CXX) $(CXXFLAGS) -fobjc-arc -o $@ tests/m2s_test.mm src/macMouseBridge.mm -framework Cocoa -framework GameController

bin/sim3d: tests/sim3d.mm src/macMouseBridge.mm include/macMouseBridge.h include/mouse_to_stick.h | bin
	$(CXX) $(CXXFLAGS) -fobjc-arc -o $@ tests/sim3d.mm src/macMouseBridge.mm -framework Cocoa -framework SceneKit -framework GameController
endif

test: bin/selftest bin/tracktest bin/calibtest
	./bin/selftest
	./bin/tracktest
	./bin/calibtest

clean:
	rm -rf bin

.PHONY: all test clean
