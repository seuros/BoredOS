#ifndef _SYS_KD_H
#define _SYS_KD_H

/* Keyboard LED definitions for KDGETLED/KDSETLED */
#define KDGETLED        0x4B29          /* Get keyboard LED state */
#define KDSETLED        0x4B2A          /* Set keyboard LED state */
#define LED_SCR         0x01            /* Scroll Lock LED flag */
#define LED_NUM         0x02            /* Num Lock LED flag */
#define LED_CAP         0x04            /* Caps Lock LED flag */

/* Keyboard type identification */
#define KDGKBTYPE       0x4B33          /* Get keyboard type */
#define KB_84           0x01            /* Standard 84-key keyboard */
#define KB_101          0x02            /* Standard 101/102-key keyboard */
#define KB_OTHER        0x03            /* Other keyboard type */

/* I/O port accessibility control (historical x86 PC BIOS compatibility) */
#define KDADDIO         0x4B34          /* Add I/O port to permission list */
#define KDDELIO         0x4B35          /* Delete I/O port from permission list */
#define KDENABIO        0x4B36          /* Enable direct access to video board I/O */
#define KDDISABIO       0x4B37          /* Disable direct access to video board I/O */

/* Console display mode switching */
#define KDSETMODE       0x4B3A          /* Set text/graphics display mode */
#define KDGETMODE       0x4B3B          /* Get text/graphics display mode */
#define KD_TEXT         0x00            /* Text mode (default console output) */
#define KD_GRAPHICS     0x01            /* Graphics mode (disable console text blitting) */
#define KD_TEXT0        0x02            /* Historical/obsolete */
#define KD_TEXT1        0x03            /* Historical/obsolete */

/* Console bell and speaker sound generation */
#define KDMKTONE        0x4B30          /* Generate a timed sound tone */
#define KIOCSOUND       0x4B2F          /* Start or stop sound generation */

/* Console screen-font management */
#define GIO_FONT        0x4B60          /* Get current console font */
#define PIO_FONT        0x4B61          /* Set current console font */
#define GIO_FONTX       0x4B6B          /* Get font with extended parameters */
#define PIO_FONTX       0x4B6C          /* Set font with extended parameters */
#define PIO_FONTRESET   0x4B6D          /* Reset console to default system font */

struct consolefontdesc {
    unsigned short charcount;           /* Characters in font map (usually 256 or 512) */
    unsigned short charheight;          /* Scan lines per character (1 to 32) */
    char *chardata;                     /* Pointer to font raw pixel-glyph data */
};

/* Keyboard input translation modes */
#define KDGKBMODE       0x4B44          /* Get keyboard translation mode */
#define KDSKBMODE       0x4B45          /* Set keyboard translation mode */
#define K_RAW           0x00            /* Raw scancodes mode */
#define K_XLATE         0x01            /* Translated keycodes mode */
#define K_MEDIUMRAW     0x02            /* Medium-raw (scancode sequence) mode */
#define K_UNICODE       0x03            /* UTF-8 Unicode characters mode */
#define K_OFF           0x04            /* Turn off keyboard input */

/* Keyboard meta-key handling */
#define KDGKBMETA       0x4B62          /* Get meta key processing state */
#define KDSKBMETA       0x4B63          /* Set meta key processing state */
#define K_METABIT       0x03            /* Set high bit (0x80) on meta key */
#define K_ESCPREFIX     0x04            /* Prepend ESC character prefix on meta key */

/* Keyboard led flags control */
#define KDGKBLED        0x4B64          /* Get keyboard LED flags */
#define KDSKBLED        0x4B65          /* Set keyboard LED flags */
#define K_SCROLLLOCK    0x01            /* Scroll Lock active flag */
#define K_NUMLOCK       0x02            /* Num Lock active flag */
#define K_CAPSLOCK      0x04            /* Caps Lock active flag */

/* Keyboard translation keymap entries */
struct kbentry {
    unsigned char kb_table;             /* Keymap table index */
    unsigned char kb_index;             /* Keycode index (0 to 127) */
    unsigned short kb_value;            /* Translated key value (action code) */
};

#define KDGKBENT        0x4B46          /* Get key translation table entry */
#define KDSKBENT        0x4B47          /* Set key translation table entry */

#define NR_KEYS         256             /* Max keys supported in map */
#define MAX_NR_KEYMAPS  256             /* Max keymaps supported */

/* Keymap table index mappings */
#define K_NORMTAB       0x00            /* Normal base map */
#define K_SHIFTTAB      0x01            /* Shift key map */
#define K_ALTTAB        0x02            /* Alt key map */
#define K_ALTSHIFTTAB   0x03            /* Alt-Shift key map */

/* Keyboard repeat rate adjustments */
#define KDKBDREP        0x4B40          /* Adjust keyboard delay and repeat rates */

struct kbd_repeat {
    int delay;                          /* Milliseconds before repeating starts */
    int period;                         /* Milliseconds between repetitions */
};

/* Keyboard diode state controls */
#define KDGETDIODE      0x4B4D          /* Get keyboard diode hardware status */
#define KDSETDIODE      0x4B4E          /* Set keyboard diode hardware status */

#endif /* _SYS_KD_H */
