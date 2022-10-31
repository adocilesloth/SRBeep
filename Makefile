ifndef OBS_INCLUDE
OBS_INCLUDE = /usr/include/obs
endif
ifndef OBS_API_INCLUDE
OBS_API_INCLUDE = ./
endif
ifndef OBS_LIB
OBS_LIB = /usr/lib
endif
ifndef FFmpegPath
FFmpegPath = $(HOME)/ffmpeg_sources/ffmpeg
endif
ifndef FFmpegLib
FFmpegLib = $(HOME)/ffmpeg_build/lib
endif
ifndef SDL_INCLUDE
SDL_INCLUDE = /usr/local/include/SDL2
endif
ifndef SDL_LIB
SDL_LIB = /usr/local/lib
endif

RM = rm -f

CXX = g++
CXXFLAGS = -g -Wall -std=c++20 -fPIC

INCLUDE = -I$(OBS_INCLUDE) -I$(OBS_API_INCLUDE) -I$(SDL_INCLUDE) -I$(FFmpegPath)
LDFLAGS = -L$(OBS_LIB) -L$(SDL_LIB) -L$(FFmpegLib)
LDLIBS_LIB   = -lobs -lavcodec -lavformat -lswresample -lavutil -lSDL2 #libs for ffmpeg and SDL

LIB = SRBeep.so
LIB_OBJ = SRBeep.o
SRC = SRBeep.cpp

all: $(LIB)

$(LIB): $(LIB_OBJ)
	$(CXX) -shared $(LDFLAGS) $^ $(LDLIBS_LIB) -o $@

$(LIB_OBJ): $(SRC)
	$(CXX) -c $(CXXFLAGS) $^ $(INCLUDE) -o $@

#Install for obs-studio from PPA
.PHONY: install
install:
	mkdir -p $(HOME)/.config/obs-studio/plugins/SRBeep/bin/64bit
    cp $(LIB) $(HOME)/.config/obs-studio/plugins/SRBeep/bin/64bit/
	mkdir -p $(HOME}/.config/obs-studio/plugins/SRBeep/data
	cp ./resource/*.mp3 $(HOME}/.config/obs-studio/plugins/SRBeep/data/
	#sudo $(FFmpegLib)/libavcodec.so.59 /usr/lib/
	#sudo $(FFmpegLib)/libavformat.so.59 /usr/lib/
	#sudo $(FFmpegLib)/libswresample.so.4 /usr/lib/
	#sudo $(FFmpegLib)/libavutil.so.57 /usr/lib/
	sudo ./depends/lib* /usr/lib/

.PHONY: clean
clean:
	$(RM) $(LIB_OBJ) $(LIB)

.PHONY: uninstall
uninstall:
	rm -r $(HOME)/.config/obs-studio/plugins/SRBeep
