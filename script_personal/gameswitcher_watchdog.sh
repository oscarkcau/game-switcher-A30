#!/bin/sh

INFO_DIR="/mnt/SDCARD/RetroArch/.retroarch/cores"
DEFAULT_IMG="/mnt/SDCARD/Themes/SPRUCE/icons/ports.png"

BIN_PATH="/mnt/SDCARD/.tmp_update/bin"
FLAG_PATH="/mnt/SDCARD/.tmp_update/flags"
FLAG_FILE="$FLAG_PATH/gs.lock"
LIST_FILE="$FLAG_PATH/gs_list"
TEMP_FILE="$FLAG_PATH/gs_list_temp"
LONG_PRESSED=false

add_game_to_list() {
    # get game path
    CMD=`cat /tmp/cmd_to_run.sh`

    # check command is emulator
    # exit if not emulator is in command
    if echo "$CMD" | grep -q -v '/mnt/SDCARD/Emu' ; then
        return
    fi

    # update switcher game list
    if [ -f "$LIST_FILE" ] ; then
        # if game list file exists
        # get all commands except the current game
        grep -Fxv "$CMD" "$LIST_FILE" > "$TEMP_FILE"
        mv "$TEMP_FILE" "$LIST_FILE"
        # append the command for current game to the end of game list file 
        echo "$CMD" >> "$LIST_FILE"
    else
        # if game list file does not exist
        # put command to new game list file
        echo "$CMD" > "$LIST_FILE"
    fi
}

prepare_run_switcher() {
    # makesure all emulators and games in list exist
    # remove all non existing games from list file
    rm -f "$TEMP_FILE"
    while read -r CMD; do
        EMU_PATH=`echo $CMD | cut -d\" -f2`
        GAME_PATH=`echo $CMD | cut -d\" -f4`
        if [ ! -f "$EMU_PATH" ] ; then continue ; fi
        if [ ! -f "$GAME_PATH" ] ; then continue ; fi
        echo "$CMD" >> "$TEMP_FILE"
    done <$LIST_FILE
    mv "$TEMP_FILE" "$LIST_FILE"

    # trim the game list to only recent 10 games
    tail -10 "$LIST_FILE" > "$TEMP_FILE"
    mv "$TEMP_FILE" "$LIST_FILE"

    if pgrep -x "./drastic" > /dev/null ; then
        # use sendevent to send menu + L1 combin buttons to drastic  
        {
            # echo 1 28 0  # START up, to avoid screen brightness is changed by L1 key press below
            # echo 1 1 1   # menu down
            echo 1 15 1  # L1 down
            echo 1 15 0  # L1 up
            echo 1 1 0   # menu up
            echo 0 0 0   # tell sendevent to exit
        } | $BIN_PATH/sendevent /dev/input/event3
    else
        killall -q -15 retroarch || \
        killall -q -15 ra32.miyoo || \
        killall -q -15 PPSSPPSDL || \    
        killall -q -9 MainUI
    fi
    
    # get return value of killall command
    RETURN_INDEX=$?

    # if kill some good emulator or MainUI
    # set flag file for principal.sh to load game switcher later
    if [ $RETURN_INDEX -eq 0 ]; then
        touch "$FLAG_FILE"
    fi
}

long_press_handler() {
    # set flag for long pressed event
    LONG_PRESSED=false
    sleep 2
    LONG_PRESSED=true    

    add_game_to_list
    prepare_run_switcher
}

# listen to log file and handle key press events
# the keypress logs are generated by keymon
# tail -F -n 1 /var/log/messages | while read line; do
$BIN_PATH/getevent /dev/input/event3 | while read line; do
    case $line in
        *"key 1 1 1"*) # MENU key down
            # if in game or app now
            if [ -f /tmp/cmd_to_run.sh ] ; then
                if pgrep -x "./drastic" > /dev/null ; then
                    long_press_handler &
                    PID=$!
                elif pgrep "PPSSPPSDL" > /dev/null ; then
                    sleep 2 # wait 2 second for saving been started
                    add_game_to_list
                    prepare_run_switcher
                else
                    add_game_to_list
                    prepare_run_switcher
                fi
            fi
        ;;
        *"key 1 28 1"*) # Start key down
            # if in MainUI menu now and game list exists
            if pgrep -x "./MainUI" > /dev/null && [ -f "$LIST_FILE" ] ; then
                prepare_run_switcher
            fi
        ;;
        *"key 1 1 0"*) # MENU key up
            # kill the long press handler if 
            # menu button is released within time limit
            # and is in game now
            if [ "$LONG_PRESSED" = false ] && [ -f /tmp/cmd_to_run.sh ] ; then
                kill $PID
            fi
        ;;
    esac
done 
