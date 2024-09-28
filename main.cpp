#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <cstdio>
#include <cmath>
#include <list>
#include <iostream>
#include <fstream>
#include <algorithm>

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>

#include "global.h"
#include "image_item.h"
#include "fileutils.h"

using namespace std;

// settings
int scrollingFrames = 20; // how many frames used for image scrolling
int fontSize = 28;
SDL_Color text_color = {255, 255, 255, 255};
bool isMultipleLineTitle = false;
bool isShowDescription = true;
bool isSwapLeftRight = false;
int scrollingSpeed = 4;	  // title scrolling speed in pixel per frame

// global variables used in main.cpp
string programName;
list<ImageItem *> imageItems;				  // all loaded images
std::list<ImageItem *>::iterator currentIter; // iterator point to current image item
TTF_Font *font = nullptr;
SDL_Texture *messageBGTexture = nullptr;
SDL_Texture *messageTexture = nullptr;
SDL_Rect overlay_bg_render_rect = {0, 0, 0, 0};
SDL_Rect overlay_text_render_rect = {0, 0, 0, 0};
bool isScrollingMessage = false;
int scrollingOffset = 0;  // current title scrolling offset
int scrollingLength = 0;  // length of scrolling title with space
int scrollingTargetY = 0; // starting value of texture target y coordinate
int scrollingPause = 10;  // number of frames to pause when text touch left screen boundary

namespace
{
	// trim from start (in place)
	inline void ltrim(std::string &s) {
		s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
			return !std::isspace(ch);
		}));
	}

	// trim from end (in place)
	inline void rtrim(std::string &s) {
		s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
			return !std::isspace(ch);
		}).base(), s.end());
	}

	double easeInOutQuart(double x)
	{
		return x < 0.5 ? 8 * x * x * x * x : 1 - pow(-2 * x + 2, 4) / 2;
	}

	void printUsage()
	{
		cout << endl
			 << "Usage: switcher image_list title_list [-s speed] [-b on|off] [-m on|off] [-t on|off] [-ts speed]" << endl
			 << endl
			 << "-s:\timage scrolling speed in frames (default is 20), larger value means slower." << endl
			 << "-b:\tswap left/right buttons for image scrolling (default is off)." << endl
			 << "-m:\tdisplay title in multiple lines (default is off)." << endl
			 << "-t:\tdisplay title at start (default is on)." << endl
			 << "-ts:\ttitle scrolling speed in pixel per frame (default is 4)." << endl
			 << "-h,--help\tshow this help message." << endl
			 << endl
			 << "Control: Left/Right: Switch games, A: Confirm, B: Cancel, R1: Toggle title" << endl
			 << endl
			 << "return value: the 1-based index of the selected image" << endl
			 << endl;
	}

	void printErrorAndExit(const char *message, const char *extraMessage = nullptr)
	{
		cerr << programName << ": " << message;
		if (extraMessage != nullptr)
			cerr << extraMessage;
		cerr << endl
			 << endl;
		exit(0);
	}

	void printErrorUsageAndExit(const char *message, const char *extraMessage = nullptr)
	{
		cerr << programName << ": " << message;
		if (extraMessage != nullptr)
			cerr << extraMessage;
		cerr << endl;
		printUsage();
		exit(0);
	}

	void handleOptions(int argc, char *argv[])
	{
		programName = File_utils::getFileName(argv[0]);

		// ensuer enough number of arguments
		if (argc < 3)
			printErrorUsageAndExit("Arguments missing");

		// handle options
		int i = 3;
		while (i < argc)
		{
			auto option = argv[i];
			if (strcmp(option, "-s") == 0)
			{
				if (i == argc - 1)
					printErrorUsageAndExit("-s: Missing option value");
				int s = atoi(argv[i + 1]);
				if (s <= 0)
					printErrorUsageAndExit("-s: Invalue scrolling speed");
				scrollingFrames = s;
				i += 2;
			}
			else if (strcmp(option, "-b") == 0)
			{
				if (i == argc - 1)
					printErrorUsageAndExit("-b: Missing option value");
				if (strcmp(argv[i + 1], "on") == 0)
					isSwapLeftRight = true;
				else if (strcmp(argv[i + 1], "off") == 0)
					isSwapLeftRight = false;
				else
					printErrorUsageAndExit("-m: Invalue option value, expects on/off\n");
				i += 2;
			}
			else if (strcmp(option, "-m") == 0)
			{
				if (i == argc - 1)
					printErrorUsageAndExit("-m: Missing option value");
				if (strcmp(argv[i + 1], "on") == 0)
					isMultipleLineTitle = true;
				else if (strcmp(argv[i + 1], "off") == 0)
					isMultipleLineTitle = false;
				else
					printErrorUsageAndExit("-m: Invalue option value, expects on/off\n");
				i += 2;
			}
			else if (strcmp(option, "-t") == 0)
			{
				if (i == argc - 1)
					printErrorUsageAndExit("-t: Missing option value");
				if (strcmp(argv[i + 1], "on") == 0)
					isShowDescription = true;
				else if (strcmp(argv[i + 1], "off") == 0)
					isShowDescription = false;
				else
					printErrorUsageAndExit("-t: Invalue option value, expects on/off\n");
				i += 2;
			}
			else if (strcmp(option, "-ts") == 0)
			{
				if (i == argc - 1)
					printErrorUsageAndExit("-ts: Missing option value");
				int s = atoi(argv[i + 1]);
				if (s <= 0)
					printErrorUsageAndExit("-ts: Invalue scrolling speed");
				scrollingSpeed = s;
				i += 2;
			}
			else if (strcmp(option, "-h") == 0 || strcmp(option, "--help") == 0)
			{
				printUsage();
				exit(0);
			}
			else
				printErrorUsageAndExit("Invalue option: ", option);
		}
	}

	void loadImageFiles(const char *filename)
	{
		// open file
		std::ifstream file;
		file.open(filename);
		
		if (file.is_open())
		{
			string line;
			int index = 1;

			// iterate all input line
			while (std::getline(file, line))
			{
				// trim input line
				rtrim(line);
				ltrim(line);

				// skip empty line
				if (line.empty()) continue;

				// create imageItem and add to list
				imageItems.push_back(new ImageItem(index, line));
				index++;
			}
		}
		else
		{
			printErrorAndExit("cannot open file: ", filename);
		}

		// close file
		file.close();
	}

	int loadImageDescriptions(const char *filename)
	{
		// open file
		std::ifstream file;
		file.open(filename);

		if (file.is_open())
		{
			string line;
			auto iter = imageItems.begin();

			// iterate all input line
			while (std::getline(file, line))
			{
				// trim input line
				rtrim(line);
				ltrim(line);

				// skip empty line
				if (line.empty()) continue;

				// set description
				(*iter)->setDescription(line);

				// move to next imageItem
				iter++;

				// exit loop after reading enough lines
				if (iter == imageItems.end()) break;
			}
		}
		else
		{
			printErrorAndExit("cannot open file: ", filename);
		}

		// close file
		file.close();

		return 0;
	}

	int loadAllImages(void *)
	{
		auto front = imageItems.begin();
		auto back = --imageItems.end();

		// load images from both directions,
		// make sure the images close to the first shown image will be loaded earlier.
		while (true)
		{
			(*front)->loadImage();
			(*back)->loadImage();

			front++;
			if (back != imageItems.begin())
				back--;
			if (front == imageItems.end())
				break;
		}

		return 0;
	}

	void updateMessageTexture(string message)
	{
		// create new message text texture
		SDL_Surface *surfaceMessage = nullptr;
		if (isMultipleLineTitle) 
		{
			surfaceMessage = TTF_RenderUTF8_Blended_Wrapped(
				font,
				message.c_str(),
				text_color,
				global::SCREEN_HEIGHT - 20
			);
		}
		else
		{
			surfaceMessage = TTF_RenderUTF8_Blended(
				font,
				message.c_str(),
				text_color
			);
		}
		messageTexture = SDL_CreateTextureFromSurface(
			global::renderer,
			surfaceMessage);

		overlay_text_render_rect.x =
			(global::SCREEN_WIDTH - surfaceMessage->w) +
			(surfaceMessage->w - surfaceMessage->h) / 2;
		overlay_text_render_rect.y =
			(global::SCREEN_HEIGHT - surfaceMessage->h) / 2;
		overlay_text_render_rect.w = surfaceMessage->w;
		overlay_text_render_rect.h = surfaceMessage->h;

		// initial variables for scrolling title
		isScrollingMessage = false;
		if (!isMultipleLineTitle && surfaceMessage->w > global::SCREEN_HEIGHT)
		{
			isScrollingMessage = true;
			scrollingPause = 10;
			scrollingOffset = 0;
			scrollingLength = surfaceMessage->w + 40;
			scrollingLength -= scrollingLength % 4;
			overlay_text_render_rect.y += (global::SCREEN_HEIGHT - surfaceMessage->w) / 2;
			scrollingTargetY = overlay_text_render_rect.y;
		}

		if (isMultipleLineTitle)
		{
			overlay_bg_render_rect.x = global::SCREEN_WIDTH - surfaceMessage->h;
			overlay_bg_render_rect.y = 0;
			overlay_bg_render_rect.w = surfaceMessage->h;
			overlay_bg_render_rect.h = global::SCREEN_HEIGHT;
		}

		SDL_FreeSurface(surfaceMessage);
	}

	void renderDescription(Uint8 alpha)
	{
		if (!isShowDescription)
			return;
		SDL_RenderCopy(global::renderer, messageBGTexture, nullptr, &overlay_bg_render_rect);
		SDL_SetTextureAlphaMod(messageTexture, alpha);
		SDL_RenderCopyEx(global::renderer, messageTexture, nullptr, &overlay_text_render_rect, 270, nullptr, SDL_FLIP_NONE);

		// render the second message if using rolling message
		if (isScrollingMessage && scrollingLength - scrollingOffset < global::SCREEN_HEIGHT)
		{
			auto rect = overlay_text_render_rect; // struct copy here
			rect.y -= scrollingLength;			  // shift right with scrolling text length
			SDL_RenderCopyEx(global::renderer, messageTexture, nullptr, &rect, 270, nullptr, SDL_FLIP_NONE);
		}
	}

	void scrollingDescription()
	{
		// pause few frames in the begining
		if (scrollingPause > 0)
		{
			scrollingPause--;
			return;
		}

		// update offset and texture target y coordinate
		scrollingOffset += scrollingSpeed;
		overlay_text_render_rect.y += scrollingSpeed;

		// reset if the text is completely scrolled outside of screen
		if (scrollingOffset >= scrollingLength)
		{
			scrollingPause = 10;
			scrollingOffset = 0;
			overlay_text_render_rect.y = scrollingTargetY;
		}
	}

	void scrollRight()
	{
		// get current and previous images
		auto prevIter = currentIter;
		prevIter = (prevIter != imageItems.begin()) ? --prevIter : --(imageItems.end());
		ImageItem *curr = *currentIter;
		ImageItem *prev = *prevIter;

		// update new text first
		updateMessageTexture(prev->getDescription());

		// scroll images
		double offset = 0;
		double step = 1.0 / scrollingFrames;
		for (int i = 0; i < scrollingFrames; i++)
		{
			double easing = easeInOutQuart(offset);
			SDL_RenderClear(global::renderer);
			curr->renderOffset(0, easing);
			prev->renderOffset(0, easing - 1);
			int text_alpha = static_cast<int>((i * 255.0) / scrollingFrames);
			renderDescription(text_alpha);
			offset += step;
			SDL_RenderPresent(global::renderer);
			SDL_Delay(30);
		}
		prev->renderOffset(0, 0);
		renderDescription(255);
		SDL_RenderPresent(global::renderer);

		// update iterator
		currentIter = prevIter;
	}

	void scrollLeft()
	{
		// get current and next images
		auto nextIter = currentIter;
		nextIter++;
		if (nextIter == imageItems.end())
			nextIter = imageItems.begin();
		ImageItem *curr = *currentIter;
		ImageItem *next = *nextIter;

		// update new text first
		updateMessageTexture(next->getDescription());

		// scroll images
		double offset = 1.0;
		double step = 1.0 / scrollingFrames;
		for (int i = 0; i < scrollingFrames; i++)
		{
			double easing = easeInOutQuart(offset);
			SDL_RenderClear(global::renderer);
			curr->renderOffset(0, easing - 1);
			next->renderOffset(0, easing);
			int text_alpha = static_cast<int>((i * 255.0) / scrollingFrames);
			renderDescription(text_alpha);
			offset -= step;
			SDL_RenderPresent(global::renderer);
			SDL_Delay(30);
		}
		next->renderOffset(0, 0);
		renderDescription(255);
		SDL_RenderPresent(global::renderer);

		// update iterator
		currentIter = nextIter;
	}

	void keyPress(const SDL_Event &event)
	{
		if (event.type != SDL_KEYDOWN)
			return;
		const auto sym = event.key.keysym.sym;
		switch (sym)
		{
		// button A (Space key)
		case SDLK_SPACE:
			exit((*currentIter)->getIndex());
			break;
		// button LEFT (Left arrow key)
		case SDLK_LEFT:
			if (isSwapLeftRight) scrollRight(); else scrollLeft();
			break;
		// button RIGHT (Right arrow key)
		case SDLK_RIGHT:
			if (isSwapLeftRight) scrollLeft(); else scrollRight();
			break;
		case SDLK_BACKSPACE:
			isShowDescription = !isShowDescription;
			break;
		}

		// button B (Left control key)
		if (event.key.keysym.mod == KMOD_LCTRL)
		{
			exit(0);
		}
	}

}

int main(int argc, char *argv[])
{
	// handle CLI options
	handleOptions(argc, argv);

	// Init SDL
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK);
	if (IMG_Init(IMG_INIT_JPG | IMG_INIT_PNG | IMG_INIT_TIF | IMG_INIT_WEBP) == 0)
	{
		printErrorAndExit("IMG_Init failed");
	}
	else
	{
		// Clear the errors for image libraries that did not initialize.
		SDL_ClearError();
	}

	// Init font
	if (TTF_Init() == -1)
		printErrorAndExit("TTF_Init failed: ", SDL_GetError());

	font = TTF_OpenFont("./nunwen.ttf", fontSize);
	if (font == nullptr)
		printErrorAndExit("Font loading failed: ", TTF_GetError());

	// Hide cursor before creating the output surface.
	SDL_ShowCursor(SDL_DISABLE);

	// Create window and renderer
	SDL_Window *window = SDL_CreateWindow("Main", 0, 0, global::SCREEN_WIDTH, global::SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
	global::renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC);
	if (global::renderer == nullptr)
		printErrorAndExit("Renderer creation failed");

	// create message overlay background texture
	int overlay_height = fontSize + fontSize / 2;
	SDL_Rect overlay_bg_rect = {0, 0, overlay_height, global::SCREEN_HEIGHT};
	overlay_bg_render_rect.x = global::SCREEN_WIDTH - overlay_height;
	overlay_bg_render_rect.y = 0;
	overlay_bg_render_rect.w = overlay_height;
	overlay_bg_render_rect.h = global::SCREEN_HEIGHT;
	SDL_Surface *surfacebg = SDL_CreateRGBSurface(
		0,
		overlay_height,
		global::SCREEN_HEIGHT,
		32, 0, 0, 0, 0);
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
	if (imageItems.size() == 0)
		printErrorAndExit("Cannot load image list");

	// load all image titles
	loadImageDescriptions(argv[2]);

	// load first texture
	imageItems.back()->loadImage();
	imageItems.back()->createTexture();

	// load all other image fiies in background thread
	SDL_CreateThread(loadAllImages, "load_images", nullptr);

	// set current image as last image in list
	currentIter = --imageItems.end();

	// create message text texture
	updateMessageTexture((*currentIter)->getDescription());

	// Execute main loop of the window
	while (true)
	{
		// handle input events
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			switch (event.type)
			{
			case SDL_KEYDOWN:
				keyPress(event);
				break;
			case SDL_QUIT:
				return 0;
				break;
			}
		}

		// render current image and title
		(*currentIter)->renderOffset(0, 0);
		if (isScrollingMessage) scrollingDescription();
		renderDescription(255);
		SDL_RenderPresent(global::renderer);

		// delay for around 30 fps
		SDL_Delay(30);
	}

	// the lines below should never reach, just for code completeness
	SDL_DestroyTexture(messageBGTexture);
	SDL_DestroyRenderer(global::renderer);
	TTF_CloseFont(font);
	SDL_DestroyWindow(window);
	SDL_Quit();
	return 0;
}
