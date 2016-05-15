ifndef OBS_INCLUDE
OBS_INCLUDE = /usr/include/obs
endif
ifndef OBS_LIB
OBS_LIB = /usr/lib
endif
ifndef FFmpegPath
FFmpegPath = /home/will/ffmpeg_build/include
endif
ifndef FFmpegLib
FFmpegLib = $(FFmpegPath)/../lib
endif
ifndef SDL_INCLUDE
SDL_INCLUDE = /usr/include/SDL2
endif
ifndef SDL_LIB
SDL_LIB = /usr/lib/x86_64-linux-gnu
endif

RM = rm -f

CXX = g++
CXXFLAGS = -g -Wall -std=c++11 -fPIC

INCLUDE = -I$(OBS_INCLUDE) -I$(FFmpegPath) -I$(SDL_INCLUDE)
LDFLAGS = -L$(OBS_LIB) -L$(FFmpegLib) -L$(SDL_LIB)
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
	sudo mkdir /usr/share/obs/obs-plugins/SRBeep
	sudo cp ./resource/*.mp3 /usr/share/obs/obs-plugins/SRBeep/
	sudo chmod 777 /usr/share/obs/obs-plugins/SRBeep/*.mp3
	sudo cp $(LIB) /usr/lib/obs-plugins/
	sudo cp ./depend/lib* /usr/lib/

.PHONY: clean
clean:
	$(RM) $(LIB_OBJ) $(LIB)
	sudo rm -r /usr/lib/obs-plugins/$(LIB)
	sudo rm -r /usr/share/obs/obs-plugins/SRBeep
	sudo rm /usr/lib/libavcodec.so.57
	sudo rm /usr/lib/libavformat.so.57
	sudo rm /usr/lib/libswresample.so.2
	sudo rm /usr/lib/libavutil.so.55
	sudo rm /usr/lib/libx264.so.148
	sudo rm /usr/lib/libvpx.so.3
	sudo rm /usr/lib/libfdk-aac.so.1
	sudo rm /usr/lib/libSDL2.so

#Install for selfbuilt obs-studio
#Place this folder in the obs-studio root folder (eg bah/rundir/obs-studio)
#should have three folders: bin, data and obs-plugins
#will need to change 64bit to 32bit if necessary
#.PHONY: install
#install:
#	sudo mkdir ./data/obs-plugins/SRBeep
#	sudo cp ./resource/*.mp3 ./data/64bit/obs-plugins/SRBeep/
#	sudo chmod 777 ./data/obs-plugins/SRBeep/*.mp3
#	sudo cp $(LIB) ./obs-plugins/64bit/
#	#sudo cp ./depend/libSDL2.so /usr/lib/
#	#sudo cp ./depend/lib* /usr/lib/
#	#may need to put:
#	#	libavcodec.so.##
#	#	libavcodec.so.##
#	#	libavformat.so.#
#	#	libswresample.so.#
#	#	libavutil.so.#
#	#	libx264.so.###
#	#	libvpx.so.#
#	#	libfdk-aac.so.#
#	#into /usr/lib/
#	#as well as:
	#	libSDL2.so
#	#OBS will throw a warning and tell you which is missing
#	#when the plugin is loaded

#.PHONY: clean
#clean:
#	$(RM) $(LIB_OBJ) $(LIB)
#	sudo rm -r ./obs-plugins/64bit/$(LIB)
#	sudo rm -r ./data/obs-plugins/SRBeep
#	sudo rm /usr/lib/libSDL2.so

