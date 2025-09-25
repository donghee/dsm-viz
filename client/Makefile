CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2
INCLUDES = -I/usr/include/nlohmann -I/usr/include/websocketpp -I/usr/include/asio
LIBS = -pthread
DEFINES = -DASIO_STANDALONE -D_WEBSOCKETPP_CPP11_STL_

TARGET = client
SOURCES = client.cpp

$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFINES) -o $(TARGET) $(SOURCES) $(LIBS)

clean:
	rm -f $(TARGET)

install-deps:
	@echo "Installing dependencies..."
	@echo "For Ubuntu/Debian:"
	@echo "sudo apt-get install nlohmann-json3-dev libwebsocketpp-dev libasio-dev"
	@echo "For CentOS/RHEL:"
	@echo "sudo yum install nlohmann-json-devel websocketpp-devel asio-devel"

.PHONY: clean install-deps