/*
 * MIT/X Consortium License
 *
 * © 2020 Paweł Szynkiewicz <pszynk  at  gmail  dot  com>
 * © 2016-2020 Safronov Kirill <hoskeowl  at  gmail  dot  com>
 * © 2013-2016 Jakub Klinkovský <kuba.klinkovsky at gmail dot com>
 * © 2010-2011 Ben Ruijl
 * © 2006-2008 Anselm R Garbe <garbeam at gmail dot com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 */

#include <stdarg.h>     // variable arguments number
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>      // isprint()
#include <time.h>       // time()
#include <getopt.h>     // getopt_long()
#include <unistd.h>
#include <signal.h>
#include <sys/mman.h>   // mlock()
#include <X11/keysym.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/dpms.h>
#include <X11/extensions/Xrandr.h>
#include <X11/XKBlib.h> // XkbDescRec, XkbAllocKeyboard, XkbGetNames, XkbSymbolsNameMask, Atom
#include <security/pam_appl.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <fcntl.h>
#include <linux/vt.h>
#include <time.h>

#ifdef __GNUC__
    #define UNUSED(x) UNUSED_ ## x __attribute__((__unused__))
#else
    #define UNUSED(x) UNUSED_ ## x
#endif

#ifndef VERSION
    #define VERSION "develop"
#endif

#ifndef PROGNAME
    #define PROGNAME "csxlock-dev"
#endif


/* used colors keys (for command-line) */
/* looks stupid I know, but how the hell getopts_long works? */
#define BACKGROUND_COLOR_KEY (1 << 8)
#define TEXT_COLOR_KEY       ((1 << 8) + 1)
#define ERRMSG_COLOR_KEY     ((1 << 8) + 2)

/* default command-line argument values */
#define DEF_FONT              "-xos4-terminus-bold-r-normal--16-*"
#define DEF_PASSCHAR          "*"
#define DEF_BACKGROUND_COLOR  "#C3BfB0"
#define DEF_TEXT_COLOR        "#423638"
#define DEF_ERRMSG_COLOR      "#F80009"

typedef struct Dpms {
    BOOL state;
    CARD16 level;  // why?
    CARD16 standby, suspend, off;
} Dpms;

typedef struct WindowPositionInfo {
    int display_width, display_height;
    int output_x, output_y;
    int output_width, output_height;
} WindowPositionInfo;

static int conv_callback(int num_msgs, const struct pam_message **msg, struct pam_response **resp, void *appdata_ptr);


/* command-line arguments */
static char* opt_font;
static char* opt_username;
static char* opt_passchar;
static char* opt_background_color;
static char* opt_text_color;
static char* opt_errmsg_color;
static Bool  opt_hidelength;
static Bool  opt_usedpms;

/* need globals for signal handling */
Display *dpy;
Dpms dpms_original = { .state = True, .level = 0, .standby = 600, .suspend = 600, .off = 600 };  // holds original values
int dpms_timeout = 10;  // dpms timeout until program exits
Bool using_dpms;

pam_handle_t *pam_handle;
struct pam_conv conv = { conv_callback, NULL };

/* Holds the password you enter */
static char password[256];


static void
die(const char *errstr, ...) {
    va_list ap;
    va_start(ap, errstr);
    fprintf(stderr, "%s: ", PROGNAME);
    vfprintf(stderr, errstr, ap);
    va_end(ap);
    exit(EXIT_FAILURE);
}

/*
 * Clears the memory which stored the password to be a bit safer against
 * cold-boot attacks.
 *
 */
static void
clear_password_memory(void) {
    /* A volatile pointer to the password buffer to prevent the compiler from
     * optimizing this out. */
    volatile char *vpassword = password;
    for (unsigned int c = 0; c < sizeof(password); c++)
        /* rewrite with random values */
        vpassword[c] = rand();
}

/*
 * Callback function for PAM. We only react on password request callbacks.
 *
 */
static int
conv_callback(int num_msgs, const struct pam_message **msg, struct pam_response **resp, void *UNUSED(appdata_ptr)) {
    if (num_msgs == 0)
        return PAM_BUF_ERR;

    // PAM expects an array of responses, one for each message
    if ((*resp = calloc(num_msgs, sizeof(struct pam_message))) == NULL)
        return PAM_BUF_ERR;

    for (int i = 0; i < num_msgs; i++) {
        if (msg[i]->msg_style != PAM_PROMPT_ECHO_OFF &&
            msg[i]->msg_style != PAM_PROMPT_ECHO_ON)
            continue;

        // return code is currently not used but should be set to zero
        resp[i]->resp_retcode = 0;
        if ((resp[i]->resp = strdup(password)) == NULL) {
            free(*resp);
            return PAM_BUF_ERR;
        }
    }

    return PAM_SUCCESS;
}

void
handle_signal(int sig) {
    /* restore dpms settings */
    if (using_dpms) {
        DPMSSetTimeouts(dpy, dpms_original.standby, dpms_original.suspend, dpms_original.off);
        if (!dpms_original.state)
            DPMSDisable(dpy);
    }

    die("Caught signal %d; dying\n", sig);
}

void
main_loop(Window w, GC gc, XFontStruct* font, WindowPositionInfo* info, char passdisp[256], char* username, XColor UNUSED(background), XColor text_color, XColor errmsg_color, Bool hidelength, char **layoutGroups, int groupSize) {
    XEvent event;
    KeySym ksym;

    unsigned int len = 0;
    Bool running = True;
    Bool sleepmode = False;
    Bool failed = False;

    char datetime[] = "YYYY-MM-DD HH:MM";
    int datelen=strlen(datetime);
    char *format = "%Y-%m-%d %H:%M";
    char sep[] = " | ";
    time_t t = time(0);
    int line_gap = 20;

    XSync(dpy, False);

    /* define base coordinates - middle of screen */
    int base_x = info->output_x + info->output_width / 2;
    int base_y = info->output_y + info->output_height / 2;    /* y-position of the line */

    /* not changed in the loop */
    int line_x_left = base_x - info->output_width / 8;
    int line_x_right = base_x + info->output_width / 8;

    /* font properties */
    int ascent, descent;
    {
        int dir;
        XCharStruct overall;
        XTextExtents(font, passdisp, strlen(username), &dir, &ascent, &descent, &overall);
    }

    /* main event loop */
    while(running && !XNextEvent(dpy, &event)) {
        if (sleepmode && using_dpms)
            DPMSForceLevel(dpy, DPMSModeOff);

        /* update window if no events pending */
        if (!XPending(dpy)) {
            int x;
            /* draw username and line */
            x = base_x - XTextWidth(font, username, strlen(username)) / 2;
            XDrawString(dpy, w, gc, x, base_y - (line_gap/2) - descent, username, strlen(username));
            XDrawLine(dpy, w, gc, line_x_left, base_y, line_x_right, base_y);

            /* clear old passdisp */
            XClearArea(dpy, w, info->output_x, base_y + line_gap, info->output_width, ascent + descent, False);

            /* draw new passdisp or 'auth failed' */
            if (failed) {
                x = base_x - XTextWidth(font, "authentication failed", 21) / 2;
                XSetForeground(dpy, gc, errmsg_color.pixel);
                XDrawString(dpy, w, gc, x, base_y + ascent + line_gap, "authentication failed", 21);
                XSetForeground(dpy, gc, text_color.pixel);
            } else {
                int lendisp = len;
                if (hidelength && len > 0)
                    lendisp += (passdisp[len] * len) % 5;
                x = base_x - XTextWidth(font, passdisp, lendisp) / 2;
                XDrawString(dpy, w, gc, x, base_y + ascent + line_gap, passdisp, lendisp % 256);
            }

            char *text;

            /* get time */
            t = time(NULL);
            memset(datetime, 0, datelen);
            strftime(datetime, datelen+1, format, localtime(&t));

            /* get layout name */
            int currentGroup;
            {
                XkbStateRec xkbState;
                XkbGetState(dpy, XkbUseCoreKbd, &xkbState);
                currentGroup = (int)(xkbState.group);
            }
            if (groupSize > currentGroup){
                size_t txtLen = strlen(datetime) + strlen(sep) + strlen(layoutGroups[currentGroup]) + sizeof(char);
                text = (char*)malloc(sizeof(char) * txtLen);
                strcpy(text, datetime);
                strcat(text, sep);
                strcat(text, layoutGroups[currentGroup]);
            }else{
                text = (char*)malloc(sizeof(char) * strlen(datetime) + sizeof(char));
                strcpy(text, datetime);
            }

            /* write text */
            /* font properties */
            // http://filonenko-mikhail.github.io/clx-truetype/ttf-metrics.png
            int height = ascent + descent;
            int width = XTextWidth(font, text, strlen(text));
            x = base_x - width / 2;
            // base_y the middle of the screen. height*2 because
            // we pass two lines: username and line of date (get left up of corner of text)
            // line+gap*1.5: 0.5 line gap between username and line and 1 line gap between date and username
            XClearArea(dpy, w, x, base_y - (line_gap*1.5) - (height*2), width, height, False);
            XDrawString(dpy, w, gc, x, base_y - (line_gap*1.5) - height - descent, text, strlen(text));
            free(text);

            /* Check capslock state */
            unsigned int state;
            char caps[] = "Caps lock is on";
            size_t capsLen = strlen(caps);
            XkbGetIndicatorState (dpy, XkbUseCoreKbd, &state);
            width = XTextWidth(font, caps, capsLen);
            x = base_x - width / 2;
            XClearArea(dpy, w, x, base_y + (line_gap*2) + height, width, height, False);
            if (state & 1){
                XSetForeground(dpy, gc, errmsg_color.pixel);
                XDrawString(dpy, w, gc, x, base_y + (line_gap*2) + height + ascent, caps, capsLen);
                XSetForeground(dpy, gc, text_color.pixel);
            }
        }

        /* draw date, time, keyboard layout, capslock state */
        if (event.type == MotionNotify || event.type == KeyPress) {
            sleepmode = False;
            failed = False;
        }

        if (event.type == KeyPress) {
            char inputChar = 0;
            XLookupString(&event.xkey, &inputChar, sizeof(inputChar), &ksym, 0);

            switch (ksym) {
                case XK_Return:
                case XK_KP_Enter:
                    password[len] = 0;
                    if (pam_authenticate(pam_handle, 0) == PAM_SUCCESS) {
                        clear_password_memory();
                        running = False;
                    } else {
                        failed = True;
                    }
                    len = 0;
                    break;
                case XK_Escape:
                    len = 0;
                    sleepmode = True;
                    break;
                case XK_BackSpace:
                    if (len)
                        --len;
                    break;
                default:
                    if (isprint(inputChar) && (len + sizeof(inputChar) < sizeof password)) {
                        memcpy(password + len, &inputChar, sizeof(inputChar));
                        len += sizeof(inputChar);
                    }
                    break;
            }
        }
    }
}


Bool
parse_options(int argc, char** argv)
{
    static struct option opts[] = {
        { "help",             no_argument,       0, 'h' },
        { "version",          no_argument,       0, 'v' },
        { "nodpms",           no_argument,       0, 'd' },
        { "hidelength",       no_argument,       0, 'l' },
        { "font",             required_argument, 0, 'f' },
        { "passchar",         required_argument, 0, 'p' },
        { "username",         required_argument, 0, 'u' },
        { "background-color", required_argument, 0, BACKGROUND_COLOR_KEY },
        { "text-color",       required_argument, 0, TEXT_COLOR_KEY },
        { "errmsg-color",     required_argument, 0, ERRMSG_COLOR_KEY },
        { 0, 0, 0, 0 },
    };

    for (;;) {
        int opt = getopt_long(argc, argv, "hvdlf:p:u:", opts, NULL);
        if (opt == -1)
            break;

        switch (opt) {
            case 'h':
                die("usage: "PROGNAME" [OPTS]...\n"
                    "Simple X screenlocker\n"
                    "\n"
                    "Mandatory arguments to long options are mandatory for short options too.\n"
                    "   -h, --help              show this help page and exit\n"
                    "   -v, --version           show version info and exit\n"
                    "   -d, --nodpms            do not handle DPMS\n"
                    "   -l, --hidelength        derange the password length indicator\n"
                    "   -u, --username=USER     user name to be displayed at the lockscreen\n"
                    "                             (default: getenv(USER))\n"
                    "   -f, --font=XFONTDESC    use this font (expects X logical font description)\n"
                    "                             (default: \""DEF_FONT"\")\n"
                    "   -p, --passchar=C        characters used to obfuscate the password\n"
                    "                             (default: '"DEF_PASSCHAR"')\n"
                    "       --background-color=HEXCOLOR\n"
                    "                           background color for lockscreen in hex value\n"
                    "                             (default: \""DEF_BACKGROUND_COLOR"\")\n"
                    "       --text-color=HEXCOLOR\n"
                    "                           text color for lockscreen in hex value\n"
                    "                             (default: \""DEF_TEXT_COLOR"\")\n"
                    "       --errmsg-color=HEXCOLOR\n"
                    "                           message color for autentification error in hex value\n"
                    "                             (default: \""DEF_ERRMSG_COLOR"\")\n"
                );
                break;
            case 'v':
              die(PROGNAME "-" VERSION ", © 2020 Paweł Szynkiewicz\n");
              break;
            case 'f':
              opt_font = optarg;
              break;
            case 'p':
                if(strlen(optarg) >= 1) {
                    opt_passchar = optarg;
                }
                else {
                    fprintf(stderr, "Warning: -p must be 1 character at least, using the default.\n");
                }
                break;
            case 'u':
                opt_username = optarg;
                break;
            case 'd':
              opt_usedpms = False;
              break;
            case 'l':
                opt_hidelength = True;
                break;
            case BACKGROUND_COLOR_KEY:
                opt_background_color = optarg;
                break;
            case TEXT_COLOR_KEY:
                opt_text_color = optarg;
                break;
            case ERRMSG_COLOR_KEY:
                opt_errmsg_color = optarg;
                break;
            default:
                return False;
        }
    }

    return True;
}

int
main(int argc, char** argv) {
    char passdisp[256];
    int screen_num;
    WindowPositionInfo info;

    Cursor invisible;
    Window root, w;
    XColor background, text_color, errmsg_color;
    XFontStruct* font;
    GC gc;

    /* get username (used for PAM authentication) */
    char* username;
    if ((username = getenv("USER")) == NULL)
        die("USER environment variable not set, please set it.\n");

    /* set default values for command-line arguments */
    opt_hidelength = False;
    opt_usedpms = True;

    opt_username = username;
    opt_font = DEF_FONT;
    opt_passchar = DEF_PASSCHAR;

    opt_background_color = DEF_BACKGROUND_COLOR;
    opt_text_color = DEF_TEXT_COLOR;
    opt_errmsg_color = DEF_ERRMSG_COLOR;

    if (!parse_options(argc, argv))
        exit(EXIT_FAILURE);

    /* register signal handler function */
    if (signal (SIGINT, handle_signal) == SIG_IGN)
        signal (SIGINT, SIG_IGN);
    if (signal (SIGHUP, handle_signal) == SIG_IGN)
        signal (SIGHUP, SIG_IGN);
    if (signal (SIGTERM, handle_signal) == SIG_IGN)
        signal (SIGTERM, SIG_IGN);

    /* fill with password characters */
    for (unsigned int i = 0; i < sizeof(passdisp); i += strlen(opt_passchar))
        for (unsigned int j = 0; j < strlen(opt_passchar) && i + j < sizeof(passdisp); j++)
            passdisp[i + j] = opt_passchar[j];

    /* initialize random number generator */
    srand(time(NULL));

    if (!(dpy = XOpenDisplay(NULL)))
        die("cannot open dpy\n");

    if (!(font = XLoadQueryFont(dpy, opt_font)))
        die("error: could not find font. Try using a full description.\n");

    screen_num = DefaultScreen(dpy);
    root = DefaultRootWindow(dpy);

    /* get display/output size and position */
    {
        XRRScreenResources* screen = NULL;
        RROutput output;
        XRROutputInfo* output_info = NULL;
        XRRCrtcInfo* crtc_info = NULL;

        screen = XRRGetScreenResources (dpy, root);
        output = XRRGetOutputPrimary(dpy, root);

        /* When there is no primary output, the return value of XRRGetOutputPrimary
         * is undocumented, probably it is 0. Fall back to the first output in this
         * case, connected state will be checked later.
         */
        if (output == 0) {
            output = screen->outputs[0];
        }
        output_info = XRRGetOutputInfo(dpy, screen, output);

        /* Iterate through screen->outputs until connected output is found. */
        int i = 0;
        while (output_info->connection != RR_Connected || output_info->crtc == 0) {
            XRRFreeOutputInfo(output_info);
            output_info = XRRGetOutputInfo(dpy, screen, screen->outputs[i]);
            fprintf(stderr, "Warning: no primary output detected, trying %s.\n", output_info->name);
            if (i == screen->noutput)
                die("error: no connected output detected.\n");
            i++;
        }

        crtc_info = XRRGetCrtcInfo (dpy, screen, output_info->crtc);

        info.output_x = crtc_info->x;
        info.output_y = crtc_info->y;
        info.output_width = crtc_info->width;
        info.output_height = crtc_info->height;
        info.display_width = DisplayWidth(dpy, screen_num);
        info.display_height = DisplayHeight(dpy, screen_num);

        XRRFreeScreenResources(screen);
        XRRFreeOutputInfo(output_info);
        XRRFreeCrtcInfo(crtc_info);
    }

    /* allocate colors */
    {
        Colormap cmap = DefaultColormap(dpy, screen_num);
        /* background */
        if(!XParseColor(dpy, cmap, opt_background_color, &background)){
            die("error: can not parse background color: %s\n", opt_background_color);
        }else
            XAllocColor(dpy, cmap, &background);
        /* text_color */
        if(!XParseColor(dpy, cmap, opt_text_color, &text_color)){
            die("error: can not parse text color: %s\n", opt_text_color);
        }else
            XAllocColor(dpy, cmap, &text_color);
        /* errmsg_color */
        if(!XParseColor(dpy, cmap, opt_errmsg_color, &errmsg_color)){
            die("error: can not parse color for unauthenticated error message: %s\n", opt_errmsg_color);
        }else
            XAllocColor(dpy, cmap, &errmsg_color);

    }

    /* get layout groups */
    int tokenCount = 0;
    char* layoutString = NULL;
    {
        XkbDescRec* kbdDescPtr = XkbAllocKeyboard();
        XkbGetNames(dpy, XkbSymbolsNameMask, kbdDescPtr);
        Atom symName = kbdDescPtr -> names -> symbols;
        layoutString = XGetAtomName(dpy, symName);

        char *tmp = layoutString;
        while(*tmp != '\0' && *tmp != ':'){
            if(*tmp == '+')
                tokenCount ++;
            tmp++;
        }
    }
    char *layoutGroups[tokenCount];
    {
        strtok(layoutString, "+:"); //skip the first token, like 'pc'
        for(int i=0; i<tokenCount; i++)
            layoutGroups[i] = strtok(NULL, "+:");
    }

    /* create window */
    {
        XSetWindowAttributes wa;
        wa.override_redirect = 1;
        wa.background_pixel = background.pixel;
        w = XCreateWindow(dpy, root, 0, 0, info.display_width, info.display_height,
                0, DefaultDepth(dpy, screen_num), CopyFromParent,
                DefaultVisual(dpy, screen_num), CWOverrideRedirect | CWBackPixel, &wa);
        XMapRaised(dpy, w);
    }

    /* define cursor */
    {
        char curs[] = {0, 0, 0, 0, 0, 0, 0, 0};
        Pixmap pmap = XCreateBitmapFromData(dpy, w, curs, 8, 8);
        invisible = XCreatePixmapCursor(dpy, pmap, pmap, &background, &background, 0, 0);
        XDefineCursor(dpy, w, invisible);
        XFreePixmap(dpy, pmap);
    }

    /* create Graphics Context */
    {
        XGCValues values;
        gc = XCreateGC(dpy, w, (unsigned long)0, &values);
        XSetFont(dpy, gc, font->fid);
        XSetForeground(dpy, gc, text_color.pixel);
    }

    /* grab pointer and keyboard */
    int len = 1000;
    while (len-- > 0) {
        if (XGrabPointer(dpy, root, False, ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                    GrabModeAsync, GrabModeAsync, None, invisible, CurrentTime) == GrabSuccess)
            break;
        usleep(50);
    }
    while (len-- > 0) {
        if (XGrabKeyboard(dpy, root, True, GrabModeAsync, GrabModeAsync, CurrentTime) == GrabSuccess)
            break;
        usleep(50);
    }
    if (len <= 0)
        die("Cannot grab pointer/keyboard\n");

    /* set up PAM */
    {
        int ret = pam_start("csxlock", username, &conv, &pam_handle);
        if (ret != PAM_SUCCESS)
            die("PAM: %s\n", pam_strerror(pam_handle, ret));
    }

    /* Lock the area where we store the password in memory, we don’t want it to
     * be swapped to disk. Since Linux 2.6.9, this does not require any
     * privileges, just enough bytes in the RLIMIT_MEMLOCK limit. */
    if (mlock(password, sizeof(password)) != 0)
        die("Could not lock page in memory, check RLIMIT_MEMLOCK\n");

    /* handle dpms */
    using_dpms = opt_usedpms && DPMSCapable(dpy);
    if (using_dpms) {
        /* save dpms timeouts to restore on exit */
        DPMSGetTimeouts(dpy, &dpms_original.standby, &dpms_original.suspend, &dpms_original.off);
        DPMSInfo(dpy, &dpms_original.level, &dpms_original.state);

        /* set program specific dpms timeouts */
        DPMSSetTimeouts(dpy, dpms_timeout, dpms_timeout, dpms_timeout);

        /* force dpms enabled until exit */
        DPMSEnable(dpy);
    }

    /* disable tty switching */
    int term;
    if ((term = open("/dev/console", O_RDWR)) == -1) {
        fprintf(stderr, "error opening console\n");
    }
    int ioterm = ioctl(term, VT_LOCKSWITCH);
    if (ioterm == -1) {
        fprintf(stderr, "error locking console\n");
    }


    /* run main loop */
    main_loop(w, gc, font, &info, passdisp, opt_username, background, text_color, errmsg_color, opt_hidelength, layoutGroups, tokenCount);

    /* enable tty switching */
    if (ioterm >= 0)
        if ((ioctl(term, VT_UNLOCKSWITCH)) == -1) {
            fprintf(stderr, "error unlocking console\n");
        }
    if(term >= 0)
        close(term);

    /* restore dpms settings */
    if (using_dpms) {
        DPMSSetTimeouts(dpy, dpms_original.standby, dpms_original.suspend, dpms_original.off);
        if (!dpms_original.state)
            DPMSDisable(dpy);
    }

    XUngrabPointer(dpy, CurrentTime);
    XFreeFont(dpy, font);
    XFreeGC(dpy, gc);
    XDestroyWindow(dpy, w);
    XCloseDisplay(dpy);
    return 0;
}
