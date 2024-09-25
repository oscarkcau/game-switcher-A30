#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <cstdio>
#include <cmath>
#include <list>
#include <iostream>

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>

#include "global.h"
#include "image_item.h"
#include "fileutils.h"

using namespace std;

// settings 
int scrollingFrames = 20; // how many frames used for scrolling
int fontSize = 28;
SDL_Color text_color = {255, 255, 255, 255};

// global variables used in main.cpp
string programName;
list<ImageItem*> imageItems; // all loaded images
std::list<ImageItem*>::iterator currentIter; // iterator point to current image item 
TTF_Font* font = nullptr;
SDL_Texture* messageBGTexture = nullptr;
SDL_Texture* messageTexture = nullptr;
SDL_Rect overlay_bg_render_rect = {0};
SDL_Rect overlay_text_render_rect = {0};
bool isMultipleLineTitle = false;

char *ltrim(char *s)
{
    while(isspace(*s)) s++;
    return s;
}

char *rtrim(char *s)
{
    char* back = s + strlen(s);
    while(isspace(*--back));
    *(back+1) = '\0';
    return s;
}

double easeInOutQuart(double x) {
    return x < 0.5 ? 8 * x * x * x * x : 1 - pow(-2 * x + 2, 4) / 2;
}

void printUsage() {
	cout << endl
		<< "Usage: switcher image_list title_list [-s speed] [-m on|off]" << endl << endl
		<< "-s:\tscrolling speed in frames (default is 20), larger value means slower." << endl
		<< "-m:\tdisplay title in multiple lines (default is off)." << endl
		<< "-h,--help\tshow this help message." << endl
		<< endl;
}

void printErrorAndExit(const char* message, const char* extraMessage = nullptr) {
	cerr << programName << ": " << message;
	if (extraMessage != nullptr) cerr << extraMessage;
	cerr << endl << endl;
	exit(0);
}

void printErrorUsageAndExit(const char* message, const char* extraMessage = nullptr)
{
	cerr << programName << ": " << message;
	if (extraMessage != nullptr) cerr << extraMessage;
	cerr << endl;
	printUsage();
	exit(0);
}

void handleOptions(int argc, char *argv[]) {
	programName = File_utils::getFileName(argv[0]);

	// ensuer enough number of arguments
	if (argc < 3) printErrorUsageAndExit("Arguments missing");
    
	// handle options
	int i = 3;
	while (i<argc) {
		auto option = argv[i];
		if (strcmp(option, "-s") == 0) {
			if (i == argc - 1) printErrorUsageAndExit("-s: Missing option value");
			int s = atoi(argv[i+1]);
			if (s <= 0) printErrorUsageAndExit("-s: Invalue scrolling speed");
			scrollingFrames = s;
			i += 2;
		}
		else if (strcmp(option, "-m") == 0) {
			if (i == argc - 1) printErrorUsageAndExit("-m: Missing option value");
			if (strcmp(argv[i+1], "on") == 0) isMultipleLineTitle = true;
			else if (strcmp(argv[i+1], "off") == 0) isMultipleLineTitle = false;
			else printErrorUsageAndExit("-m: Invalue option value, expects on/off\n");
			i += 2;
		}
		else if (strcmp(option, "-h") == 0 || strcmp(option, "--help") == 0) {
			printUsage();
			exit(0);
		} 
		else printErrorUsageAndExit("Invalue option: ", option);
	}
}

int loadImageFiles(const char * filename) {	
	// open file
	FILE* file = fopen(filename, "r");
	if (file == nullptr) printErrorAndExit("cannot open file: ", filename);
	
	// read lines of image filename and load images
	char line[1024];
	int index = 1;
	while (fgets(line, sizeof(line), file)) {

		// trim input line
		char *trimmed = rtrim(ltrim(line));

		// skip empty line
		if (strlen(trimmed) == 0) continue;

		// create imageItem and add to list
		imageItems.push_back(new ImageItem(index, trimmed));
		index++;
	}
	
	// close file
	fclose(file);

	return 0;
}

int loadImageDescriptions(const char * filename) {	
	// open file
	FILE* file = fopen(filename, "r");
	if (file == nullptr) printErrorAndExit("cannot open file: ", filename);

	
	// read lines of description from filename
	char line[1024];
	auto iter = imageItems.begin();
	while (fgets(line, sizeof(line), file)) {

		// trim input line
		char *trimmed = rtrim(ltrim(line));

		// skip empty line
		if (strlen(trimmed) == 0) continue;

		// set description
		(*iter)->setDescription(trimmed);

		// move to next imageItem
		iter++;

		// exit loop after reading enough lines
		if (iter == imageItems.end()) break;
	}
	
	// close file
	fclose(file);

	return 0;
}

int loadAllImages(void* data) {
	auto front = imageItems.begin();
	auto back = --imageItems.end();
	
	// load images from both directions, 
	// make sure the images close to the first shown image will be loaded earlier.
	while (true) {
		(*front)->loadImage();
		(*back)->loadImage();
		
		front++;
		if (back != imageItems.begin()) back--;
		if (front == imageItems.end()) break;
	}

	return 0;
}

void updateMessageTexture(const char * message) {
	// create new message text texture
	int wrapLength = isMultipleLineTitle ? global::SCREEN_HEIGHT - 20 : 2048;
    SDL_Surface* surfaceMessage = TTF_RenderText_Blended_Wrapped(
		font, 
		message, 
		text_color, 
		wrapLength);
    messageTexture = SDL_CreateTextureFromSurface(
		global::renderer, 
		surfaceMessage);
    SDL_FreeSurface(surfaceMessage);
	overlay_text_render_rect.x = 
		(global::SCREEN_WIDTH - surfaceMessage->w) + 
		(surfaceMessage->w - surfaceMessage->h) / 2;    
	overlay_text_render_rect.y = (global::SCREEN_HEIGHT - surfaceMessage->h) / 2;
	overlay_text_render_rect.w = surfaceMessage->w;
	overlay_text_render_rect.h = surfaceMessage->h;

	if (isMultipleLineTitle) {
		overlay_bg_render_rect.x = global::SCREEN_WIDTH - surfaceMessage->h;
		overlay_bg_render_rect.y = 0;
		overlay_bg_render_rect.w = surfaceMessage->h;
		overlay_bg_render_rect.h = global::SCREEN_HEIGHT;
	}
}

void scrollLeft() {
	// get current and previous images
	auto prevIter = currentIter;
	prevIter = (prevIter != imageItems.begin()) ? --prevIter : --(imageItems.end());
	ImageItem *curr = *currentIter;
	ImageItem *prev = *prevIter;

	// update new text first
	updateMessageTexture(prev->getDescription().c_str());

	// scroll images
	double offset = 0;
	double step = 1.0 / scrollingFrames;
	for (int i = 0; i < scrollingFrames; i++) {
		double easing = easeInOutQuart(offset);
		SDL_RenderClear(global::renderer);
		curr->renderOffset(0, easing);
		prev->renderOffset(0, easing - 1);
		SDL_RenderCopy(global::renderer, messageBGTexture, NULL, &overlay_bg_render_rect);
	    int text_alpha = static_cast<int>((i * 255.0) / scrollingFrames);
		SDL_SetTextureAlphaMod(messageTexture, text_alpha);
		SDL_RenderCopyEx(global::renderer, messageTexture, NULL, &overlay_text_render_rect, 270, NULL, SDL_FLIP_NONE);
		offset += step;
		SDL_RenderPresent(global::renderer);
		SDL_Delay(30);

		if (i == scrollingFrames / 2) {
			// clear all key events during scrolling
			SDL_PumpEvents();
			SDL_FlushEvents(SDL_KEYDOWN, SDL_KEYUP);
		}
	}
	prev->renderOffset(0, 0);
	SDL_RenderCopy(global::renderer, messageBGTexture, NULL, &overlay_bg_render_rect);
	SDL_SetTextureAlphaMod(messageTexture, 255);
	SDL_RenderCopyEx(global::renderer, messageTexture, NULL, &overlay_text_render_rect, 270, NULL, SDL_FLIP_NONE);
	SDL_RenderPresent(global::renderer);
	
	// update iterator
	currentIter = prevIter;
}

void scrollRight() {
	// get current and next images
	auto nextIter = currentIter;
	nextIter++;
	if (nextIter == imageItems.end()) nextIter = imageItems.begin();
	ImageItem *curr = *currentIter;
	ImageItem *next = *nextIter;
	
	// update new text first
	updateMessageTexture(next->getDescription().c_str());

	// scroll images
	double offset = 1.0;
	double step = 1.0 / scrollingFrames;
	for (int i = 0; i < scrollingFrames; i++) {
		double easing = easeInOutQuart(offset);
		SDL_RenderClear(global::renderer);
		curr->renderOffset(0, easing - 1);
		next->renderOffset(0, easing);
	    SDL_RenderCopy(global::renderer, messageBGTexture, NULL, &overlay_bg_render_rect);
	    int text_alpha = static_cast<int>((i * 255.0) / scrollingFrames);
		SDL_SetTextureAlphaMod(messageTexture, text_alpha);
		SDL_RenderCopyEx(global::renderer, messageTexture, NULL, &overlay_text_render_rect, 270, NULL, SDL_FLIP_NONE);
		offset -= step;
		SDL_RenderPresent(global::renderer);
		SDL_Delay(30);
	}
	next->renderOffset(0, 0);
	SDL_RenderCopy(global::renderer, messageBGTexture, NULL, &overlay_bg_render_rect);
	SDL_SetTextureAlphaMod(messageTexture, 255);
	SDL_RenderCopyEx(global::renderer, messageTexture, NULL, &overlay_text_render_rect, 270, NULL, SDL_FLIP_NONE);
	SDL_RenderPresent(global::renderer);
	
	// update iterator
	currentIter = nextIter;
}

void keyPress(const SDL_Event &event) {
    if (event.type != SDL_KEYDOWN) return;
    const auto sym = event.key.keysym.sym;
    switch (sym) {
		// button A (Space key)
        case SDLK_SPACE: exit((*currentIter)->getIndex()); break;
		// button LEFT (Left arrow key)
        case SDLK_LEFT: scrollLeft(); break;
		// button RIGHT (Right arrow key)
        case SDLK_RIGHT: scrollRight(); break;
    }
    
    // button B (Left control key)
    if (event.key.keysym.mod == KMOD_LCTRL) {
		exit(0);
	}
}

int main(int argc, char *argv[])
{
	std::string image_path;
    SDL_Window *window = NULL;
	SDL_Surface *surface = NULL;
    SDL_Surface *screen = NULL;

	handleOptions(argc, argv);

    // Init SDL
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK);
    if (IMG_Init(IMG_INIT_JPG | IMG_INIT_PNG | IMG_INIT_TIF | IMG_INIT_WEBP) == 0) {
		printErrorAndExit("IMG_Init failed");
    } else {
        // Clear the errors for image libraries that did not initialize.
        SDL_ClearError();
    }

    // Init font
    if (TTF_Init() == -1) printErrorAndExit("TTF_Init failed: ", SDL_GetError());

    font = TTF_OpenFont("./nunwen.ttf", fontSize);
    if (font == nullptr) printErrorAndExit("Font loading failed: ", TTF_GetError());

    // Hide cursor before creating the output surface.
    SDL_ShowCursor(SDL_DISABLE);
	
	// Create window and renderer
	window = SDL_CreateWindow("Main", 
		0, 
		0, 
		global::SCREEN_WIDTH, 
		global::SCREEN_HEIGHT, 
		SDL_WINDOW_SHOWN);				
    global::renderer = SDL_CreateRenderer(window, 
		-1, 
		SDL_RENDERER_PRESENTVSYNC);
	if (global::renderer == nullptr) printErrorAndExit("Renderer creation failed");

	// create message overlay background texture
	int overlay_height = fontSize + fontSize / 2;	
	SDL_Rect overlay_bg_rect = {0};
		overlay_bg_rect.x = 0;
		overlay_bg_rect.y = 0;
		overlay_bg_rect.w = overlay_height;
		overlay_bg_rect.h = global::SCREEN_HEIGHT;
	overlay_bg_render_rect.x = global::SCREEN_WIDTH - overlay_height;
	overlay_bg_render_rect.y = 0;
	overlay_bg_render_rect.w = overlay_height;
	overlay_bg_render_rect.h = global::SCREEN_HEIGHT;
    SDL_Surface* surfacebg = SDL_CreateRGBSurface(
		0 , 
		overlay_height, 
		global::SCREEN_HEIGHT, 
		32 , 0 , 0 , 0 , 0 );
    SDL_FillRect(
		surfacebg, 
		&overlay_bg_rect, 
		SDL_MapRGB(surfacebg->format, 0, 0, 0));
    SDL_SetSurfaceBlendMode(surfacebg, SDL_BLENDMODE_BLEND);
    messageBGTexture = SDL_CreateTextureFromSurface(
		global::renderer, 
		surfacebg);
    SDL_SetTextureAlphaMod(messageBGTexture, 128);
    SDL_FreeSurface(surfacebg);

	// load all image filenames and create imageItem instances
	loadImageFiles(argv[1]);
	if (imageItems.size() == 0) printErrorAndExit("Cannot load image list");

	loadImageDescriptions(argv[2]);

	// load first texture
	imageItems.back()->loadImage();
	imageItems.back()->createTexture();

	// load all other image fiies in background thread
	SDL_Thread* threadID = SDL_CreateThread(loadAllImages, "load_images", nullptr);

	// set current image as last image in list
	currentIter = --imageItems.end();
	
	// create message text texture
	updateMessageTexture((*currentIter)->getDescription().c_str());

	// Execute main loop of the window
	while (true) {
		
		// handle input events
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
            switch (event.type) {
				case SDL_KEYDOWN: keyPress(event); break;
				case SDL_QUIT: return 0; break;
			}
		}
		
		(*currentIter)->renderOffset(0, 0);
		SDL_RenderCopy(global::renderer, messageBGTexture, NULL, &overlay_bg_render_rect);
		SDL_SetTextureAlphaMod(messageTexture, 255);
		SDL_RenderCopyEx(global::renderer, messageTexture, NULL, &overlay_text_render_rect, 270, NULL, SDL_FLIP_NONE);
		SDL_RenderPresent(global::renderer);
	
		SDL_Delay(30);		
	}

    SDL_DestroyTexture(messageBGTexture);
    SDL_DestroyRenderer(global::renderer);
	TTF_CloseFont(font);
    SDL_DestroyWindow(window);
    SDL_Quit();
	return 0;
}
