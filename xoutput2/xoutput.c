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

static char *fontname = "monospace:size=16";
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

/* open font from its name */
static XftFont *
openfont(const char *fontname)
{
	FcPattern *pattern;
	FcPattern *match;
	FcResult result;
	FcBool status;
	XftFont *font;

	/* parse pattern */
	pattern = FcNameParse(fontname);
	if (pattern == NULL)
		errx(1 ,"openfont: could not parse font name");

	/* fill in missing items in our pattern */
	FcDefaultSubstitute(pattern);
	status = FcConfigSubstitute(NULL, pattern, FcMatchPattern);
	if (status == FcFalse)
		errx(1, "openfont: could not perform pattern substitution");

	/* find font matching pattern */
	match = FcFontMatch(NULL, pattern, &result);
	if (match == NULL || result != FcResultMatch)
		errx(1, "openfont: could not find font matching pattern");

	/* open font */
	font = XftFontOpenPattern(dpy, match);
	if (font == NULL)
		errx(1, "openfont: could not open font");

	/* close patterns */
	FcPatternDestroy(pattern);
	FcPatternDestroy(match);

	return font;
}

/* allocate color for Xft */
static void
alloccolor(const char *colorname, XftColor *color)
{
	if (!XftColorAllocName(dpy, visual, colormap, colorname, color))
		errx(1, "could not allocate color");
}

/* draw text on pixmap */
static void
draw(Drawable pix, XftFont *font, XftColor *color, const char *text)
{
	XGlyphInfo ext;
	XftDraw *draw;
	int x, y;
	size_t len;

	draw = XftDrawCreate(dpy, pix, visual, colormap);

	len = strlen(text);

	/* get text extents */
	XftTextExtentsUtf8(dpy, font, text, len, &ext);
	x = (WIDTH - ext.width) / 2;
	y = (HEIGHT + ext.height) / 2;

	/* draw text */
	XftDrawStringUtf8(draw, color, font, x, y, text, len);

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
	XftColor color;
	XftFont *font;
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
	font = openfont(fontname);
	alloccolor(colorname, &color);

	/* draw argv[1] on the pixmap */
	draw(pix, font, &color, argv[1]);

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
	XftFontClose(dpy, font);

	/* close connection to the server */
	XCloseDisplay(dpy);

	return 0;
}
