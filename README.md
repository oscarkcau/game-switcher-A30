# Game Switcher for SpruceOS on Miyoo A30

Game switcher is a SDL2 program run on Miyoo A30 game console. It is used for fast switching between list of games.

```
Usage: switcher image_list title_list [-s speed] [-m on|off]
-s: scrolling speed in frames (default is 20), larger value means slower.
-m: display title in multiple lines (default is off).
```

Folder **script**: scripts used with game switcher. It uses screenspots of auto save states as default display images, 
and use game box-art if no screenspots is provided.

Folder **script_personal**: personal edition of scripts used with game switcher on my A30. It displays game box-arts 
only and uses MENU button to trigger game switcher.

# Links
SpruceOS
https://github.com/spruceUI/spruceOS

Miyoo webpage
https://www.lomiyoo.com/
