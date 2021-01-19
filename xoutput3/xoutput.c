#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xft/Xft.h>

#define POSX    200
#define POSY    200
#define WIDTH   350
#define HEIGHT  250
#define BORDER  10

/* get length of array */
#define LEN(x) (sizeof (x) / sizeof (x[0]))

/* get whether x is between a and b (inclusive) */
#define BETWEEN(x, a, b)    ((a) <= (x) && (x) <= (b))

/* get maximum between x and y */
#define MAX(x,y) ((x)>(y)?(x):(y))

/* each entry of our fontset */
typedef struct FontEntry {
	FcPattern *match;       /* the pattern matching the font */
	FcCharSet *charset;     /* the charset of the font */
	XftFont *font;          /* the opened font */
	int open;               /* whether we have tried to open the font */
} FontEntry;

/* hold an array of FontEntry and its size */
typedef struct FontSet {
	struct FontEntry *fonts;
	size_t nfonts;
} FontSet;

static char *fontnames[] = {
	"monospace:size=16",
	"DejaVu Sans Mono:size=16",
	"NotoSans Mono CJK HK:size=16"
};
static char *colorname = "#000000";

static Display *dpy;
static Visual *visual;
static Window root;
static Colormap colormap;
static int depth;
static int screen;

/* setup top-level window */
static Window
createwin(int x, int y, int w, int h, int b)
{
	XSetWindowAttributes swa;
	Window win;

	/* create top-level window */
	swa.background_pixel = WhitePixel(dpy, screen);
	swa.border_pixel = BlackPixel(dpy, screen);
	swa.event_mask = ExposureMask | ButtonPressMask;
	win = XCreateWindow(dpy, root, x, y, w, h, b,
	                    CopyFromParent, CopyFromParent, CopyFromParent,
	                    CWBackPixel | CWBorderPixel | CWEventMask,
	                    &swa);

	return win;
}

static Pixmap
createpix(GC gc, int w, int h)
{
	Pixmap pix;

	pix = XCreatePixmap(dpy, root, w, h, depth);

	/*
	 * we change gc foreground for we to clean the pixmap,
	 * and then we revert it to its previous value
	 */
	XSetForeground(dpy, gc, WhitePixel(dpy, screen));
	XFillRectangle(dpy, pix, gc, 0, 0, w, h);
	XSetForeground(dpy, gc, BlackPixel(dpy, screen));

	return pix;
}

/* create fontset from an array of font names */
static FontSet *
createfontset(char *fontnames[], size_t nfonts)
{
	FontSet *fontset;
	FcCharSet *charset;
	FcPattern *pattern;
	FcPattern *match;
	FcResult result;
	FcBool status;
	size_t i;

	if ((fontset = malloc(sizeof *fontset)) == NULL)
		err(1, "malloc");
	if ((fontset->fonts = malloc(nfonts * sizeof *(fontset->fonts))) == NULL)
		err(1, "malloc");
	fontset->nfonts = nfonts;

	for (i = 0; i < nfonts; i++) {
		fontset->fonts[i].open = 0;
		fontset->fonts[i].font = NULL;
		fontset->fonts[i].match = NULL;
		fontset->fonts[i].charset = NULL;

		/* parse pattern */
		pattern = FcNameParse(fontnames[i]);
		if (pattern == NULL) {
			warnx("openfont: could not parse font name");
			continue;
		}

		/* fill in missing items in our pattern */
		FcDefaultSubstitute(pattern);
		status = FcConfigSubstitute(NULL, pattern, FcMatchPattern);
		if (status == FcFalse) {
			warnx("openfont: could not perform pattern substitution");
			FcPatternDestroy(pattern);
			continue;
		}

		/* find font matching pattern */
		match = FcFontMatch(NULL, pattern, &result);
		if (match == NULL || result != FcResultMatch) {
			warnx("openfont: could not find font matching pattern");
			FcPatternDestroy(pattern);
			continue;
		}

		/* get charset of font */
		charset = NULL;
		result = FcPatternGetCharSet(match, FC_CHARSET, 0, &charset);
		if (charset == NULL || result != FcResultMatch) {
			warnx("openfont: could not get charset of font %s", fontnames[i]);
			FcPatternDestroy(pattern);
			FcPatternDestroy(match);
			continue;
		}

		/* assign match pattern and charset to font in our fontset */
		fontset->fonts[i].match = match;
		fontset->fonts[i].charset = charset;

		/* close patterns */
		FcPatternDestroy(pattern);
	}
	return fontset;
}

/* destroy fontset and all its fonts */
static void
destroyfontset(FontSet *fontset)
{
	free(fontset->fonts);
	free(fontset);
}

/* allocate color for Xft */
static void
alloccolor(const char *colorname, XftColor *color)
{
	if (!XftColorAllocName(dpy, visual, colormap, colorname, color))
		errx(1, "could not allocate color");
}

/* get next utf8 char from s return its codepoint and set next_ret to pointer to end of character */
static FcChar32
getnextutf8char(const char *s, const char **next_ret)
{
	static const unsigned char utfbyte[] = {0x80, 0x00, 0xC0, 0xE0, 0xF0};
	static const unsigned char utfmask[] = {0xC0, 0x80, 0xE0, 0xF0, 0xF8};
	static const FcChar32 utfmin[] = {0, 0x00,  0x80,  0x800,  0x10000};
	static const FcChar32 utfmax[] = {0, 0x7F, 0x7FF, 0xFFFF, 0x10FFFF};
	/* 0xFFFD is the replacement character, used to represent unknown characters */
	static const FcChar32 unknown = 0xFFFD;
	FcChar32 ucode;         /* FcChar32 type holds 32 bits */
	size_t usize = 0;       /* n' of bytes of the utf8 character */
	size_t i;

	*next_ret = s+1;

	/* get code of first byte of utf8 character */
	for (i = 0; i < sizeof utfmask; i++) {
		if (((unsigned char)*s & utfmask[i]) == utfbyte[i]) {
			usize = i;
			ucode = (unsigned char)*s & ~utfmask[i];
			break;
		}
	}

	/* if first byte is a continuation byte or is not allowed, return unknown */
	if (i == sizeof utfmask || usize == 0)
		return unknown;

	/* check the other usize-1 bytes */
	s++;
	for (i = 1; i < usize; i++) {
		*next_ret = s+1;
		/* if byte is nul or is not a continuation byte, return unknown */
		if (*s == '\0' || ((unsigned char)*s & utfmask[0]) != utfbyte[0])
			return unknown;
		/* 6 is the number of relevant bits in the continuation byte */
		ucode = (ucode << 6) | ((unsigned char)*s & ~utfmask[0]);
		s++;
	}

	/* check if ucode is invalid or in utf-16 surrogate halves */
	if (!BETWEEN(ucode, utfmin[usize], utfmax[usize]) || BETWEEN (ucode, 0xD800, 0xDFFF))
		return unknown;

	return ucode;
}

/* get which font contains a given code point */
static XftFont *
getfontucode(FontSet *fontset, FcChar32 ucode)
{
	size_t i;

	for (i = 0; i < fontset->nfonts; i++) {
		if (fontset->fonts[i].charset == NULL) {
			continue;
		}
		if (FcCharSetHasChar(fontset->fonts[i].charset, ucode) == FcTrue) {
			if (fontset->fonts[i].open == 0) {
				fontset->fonts[i].open = 1;
				fontset->fonts[i].font = XftFontOpenPattern(dpy, fontset->fonts[i].match);
			}
			if (fontset->fonts[i].font != NULL) {
				return fontset->fonts[i].font;
			}
		}
	}

	/* if we have not found a font, we still have to return one */
	for (i = 0; i < fontset->nfonts; i++)
		if (fontset->fonts[i].font)
			return fontset->fonts[i].font;

	return NULL;
}

/* get width and ascend height of text when drawn */
static void
textextents(FontSet *fontset, const char *text, int *w, int *h)
{
	XftFont *currfont, *nextfont = NULL;
	XGlyphInfo ext;
	FcChar32 ucode;
	const char *next, *tmp;
	size_t len = 0;

	*w = *h = 0;
	while (*text) {
		tmp = text;
		do {
			next = tmp;
			currfont = nextfont;
			ucode = getnextutf8char(next, &tmp);
			nextfont = getfontucode(fontset, ucode);
			if (nextfont == NULL)
				errx(1, "drawtext: could not find font for character");
		} while (*next && currfont == nextfont);
		if (!currfont)
			continue;
		len = next - text;
		XftTextExtentsUtf8(dpy, currfont, (XftChar8 *)text, len, &ext);
		*w += ext.xOff;
		*h = MAX(*h, ext.height - ext.y);
		text = next;
	}
}

/* draw text into XftDraw */
static void
drawtext(FontSet *fontset, XftDraw *draw, XftColor *color, int x, int y, const char *text)
{
	XftFont *currfont, *nextfont = NULL;
	XGlyphInfo ext;
	FcChar32 ucode;
	const char *next, *tmp;
	size_t len = 0;

	while (*text) {
		tmp = text;
		do {
			next = tmp;
			currfont = nextfont;
			ucode = getnextutf8char(next, &tmp);
			nextfont = getfontucode(fontset, ucode);
			if (nextfont == NULL)
				errx(1, "drawtext: could not find font for character");
		} while (*next && currfont == nextfont);
		if (!currfont)
			continue;
		len = next - text;
		XftDrawStringUtf8(draw, color, currfont, x, y, (XftChar8 *)text, len);
		XftTextExtentsUtf8(dpy, currfont, (XftChar8 *)text, len, &ext);
		x += ext.xOff;
		text = next;
	}
}

/* draw text on pixmap */
static void
draw(Drawable pix, FontSet *fontset, XftColor *color, const char *text)
{
	XftDraw *draw;
	int x, y, w, h;

	draw = XftDrawCreate(dpy, pix, visual, colormap);

	/* get text extents */
	textextents(fontset, text, &w, &h);
	x = (WIDTH - w) / 2;
	y = (HEIGHT + h) / 2;

	/* draw text */
	drawtext(fontset, draw, color, x, y, text);

	XftDrawDestroy(draw);
}

/* enter the event loop */
static void
run(GC gc, Pixmap pix)
{
	XEvent ev;

	while(XNextEvent(dpy, &ev) == 0) {
		switch(ev.type) {
		case Expose:
			XCopyArea(dpy, pix, ev.xexpose.window, gc,
			          ev.xexpose.x, ev.xexpose.y,
			          ev.xexpose.width, ev.xexpose.height,
			          ev.xexpose.x, ev.xexpose.y);
			break;
		case ButtonPress:
			return;
		}
	}
}

/* xoutput: display text on window */
int
main(int argc, char *argv[])
{
	FontSet *fontset;
	XftColor color;
	Pixmap pix;
	Window win;
	GC gc;

	if (argc != 2) {        /* we need one argument */
		fprintf(stderr, "usage: xoutput string\n");
		return 1;
	}

	/* open connection to the server */
	if ((dpy = XOpenDisplay(NULL)) == NULL)
		errx(1, "could not open display");
	screen = DefaultScreen(dpy);
	colormap = DefaultColormap(dpy, screen);
	visual = DefaultVisual(dpy, screen);
	depth = DefaultDepth(dpy, screen);
	root = RootWindow(dpy, screen);

	/* setup resources */
	win = createwin(POSX, POSY, WIDTH, HEIGHT, BORDER);
	gc = XCreateGC(dpy, root, 0, NULL);
	pix = createpix(gc, WIDTH, HEIGHT);
	fontset = createfontset(fontnames, LEN(fontnames));
	alloccolor(colorname, &color);

	/* draw argv[1] on the pixmap */
	draw(pix, fontset, &color, argv[1]);

	/* map the window */
	XMapWindow(dpy, win);

	/* run main loop */
	run(gc, pix);

	/* unmap the window */
	XUnmapWindow(dpy, win);

	/* close resources */
	XDestroyWindow(dpy, win);
	XFreePixmap(dpy, pix);
	XFreeGC(dpy, gc);
	XftColorFree(dpy, visual, colormap, &color);
	destroyfontset(fontset);

	/* close connection to the server */
	XCloseDisplay(dpy);

	return 0;
}
