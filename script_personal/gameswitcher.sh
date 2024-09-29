#!/bin/sh

FLAG_FILE="/mnt/SDCARD/.tmp_update/flags/gs.lock"
LIST_FILE="/mnt/SDCARD/.tmp_update/flags/gs_list"
IMAGES_FILE="/mnt/SDCARD/.tmp_update/flags/gs_images"
GAMENAMES_FILE="/mnt/SDCARD/.tmp_update/flags/gs_names"
TEMP_FILE="/mnt/SDCARD/.tmp_update/flags/gs_list_temp"

INFO_DIR="/mnt/SDCARD/RetroArch/.retroarch/cores"
DEFAULT_IMG="/mnt/SDCARD/Themes/SPRUCE/icons/ports.png"

# remove flag for game switcher
rm "$FLAG_FILE" 

# exit if no game in list file
if [ ! -f "$LIST_FILE" ] ; then
    exit 0
fi

# prepare files for switcher program
rm -f "$IMAGES_FILE"
rm -f "$GAMENAMES_FILE"
while read -r CMD; do
    # get and store game name to file
    GAME_PATH=`echo $CMD | cut -d\" -f4`
    GAME_NAME="${GAME_PATH##*/}"
    SHORT_NAME="${GAME_NAME%.*}"
    echo "$SHORT_NAME" >> "$GAMENAMES_FILE"

    # try get box art file path
    BOX_ART_PATH="$(dirname "$GAME_PATH")/Imgs/$(basename "$GAME_PATH" | sed 's/\.[^.]*$/.png/')"

    # store screenshot / box art / default image to file
    if [ -f "$BOX_ART_PATH" ]; then
        echo "$BOX_ART_PATH" >> "$IMAGES_FILE"        
    else
        echo "$DEFAULT_IMG" >> "$IMAGES_FILE"
    fi
done <$LIST_FILE

# launch the switcher program
# Usage: switcher image_list title_list [-s speed] [-b on|off] [-m on|off] [-t on|off] [-ts speed] [-n on|off] [-d command]
# -s: scrolling speed in frames (default is 20), larger value means slower.
# -b: swap left/right buttons for image scrolling (default is off).
# -m: display title in multiple lines (default is off).
# -t: display title at start (default is on).
# -ts: title scrolling speed in pixel per frame (default is 4).
# -n: display item index (default is on).
# -d: enable item deletion with the deletion command provided (default is disable).
#     Use INDEX in command to take the selected index as input. e.g. "echo INDEX"
#     Pass "" as argument if no command is provided.
# -h,--help show this help message.
# return value: the 1-based index of the selected image
cd /mnt/SDCARD/.tmp_update/bin/
/mnt/SDCARD/.tmp_update/bin/switcher "$IMAGES_FILE" "$GAMENAMES_FILE" -s 10 \
-d "sed -i 'INDEXs/.*/removed/' $LIST_FILE"

# get return value and launch game with return index
RETURN_INDEX=$?
if [ $RETURN_INDEX -gt 0 ]; then
    # get command that launches the game
    CMD=`tail -n+$RETURN_INDEX "$LIST_FILE" | head -1`

    # move the selected game to the end of the list file & skip all removed items
    # 1. get all commands except the selected game or removed lines
    grep -Fxv "${CMD}" "$LIST_FILE" | grep -Fxv "removed" > "$TEMP_FILE"
    mv "$TEMP_FILE" "$LIST_FILE"
    # 2. append the command for current game to the end of game list file 
    echo "$CMD" >> "$LIST_FILE"

    # wrtie command to file which will be run by principle.sh
    echo $CMD > /tmp/cmd_to_run.sh
    sync
else
    # skip all removed items
    grep -Fxv "removed" "$LIST_FILE" > "$TEMP_FILE"
    mv "$TEMP_FILE" "$LIST_FILE"
fi
