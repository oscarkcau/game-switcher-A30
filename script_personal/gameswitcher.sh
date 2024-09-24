#!/bin/sh

FLAG_FILE="/mnt/SDCARD/.tmp_update/flags/gs.lock"
LIST_FILE="/mnt/SDCARD/.tmp_update/flags/gs_list"
IMAGES_FILE="/mnt/SDCARD/.tmp_update/flags/gs_images"
GAMENAMES_FILE="/mnt/SDCARD/.tmp_update/flags/gs_names"

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
# Usage: switcher image_list title_list [-s speed] [-m on|off]
# -s: scrolling speed in frames (default is 20), larger value means slower.
# -m: display title in multiple lines (default is off).
cd /mnt/SDCARD/.tmp_update/bin/
/mnt/SDCARD/.tmp_update/bin/switcher "$IMAGES_FILE" "$GAMENAMES_FILE" -s 10 -m on

# get return value and launch game with return index
RETURN_INDEX=$?
if [ $RETURN_INDEX -gt 0 ]; then
    CMD=`tail -n+$RETURN_INDEX "$LIST_FILE" | head -1`
    echo $CMD > /tmp/cmd_to_run.sh
    sync
fi
