#include <err.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#define POSX    200
#define POSY    200
#define WIDTH   350
#define HEIGHT  250
#define BORDER  10
#define FONT    "-*-dejavu sans mono-medium-r-normal--*-80-*-*-*-*-iso10646-1"

static Display *dpy;
static Window root;
static int screen;
static int depth;

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

/* create pixmap */
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

/* create fontset */
static XFontSet
createfontset(const char *fontname)
{
	XFontSet fontset;
	char **mc;      /* dummy variable; allocated array of missing charsets */
	int nmc;        /* dummy variable; number of missing charsets */
	char *ds;       /* dummy variable; default string drawn in place of unknown chars */

	if ((fontset = XCreateFontSet(dpy, fontname, &mc, &nmc, &ds)) == NULL)
		errx(1, "XCreateFontSet: could not create fontset");
	XFreeStringList(mc);
	return fontset;
}

/* draw text on pixmap using given graphics context and fontset */
static void
draw(Drawable pix, GC gc, XFontSet fontset, const char *text)
{
	XRectangle box, dummy;
	int x, y;
	int len;

	len = (int)strlen(text);
	XmbTextExtents(fontset, text, len, &dummy, &box);
	x = (WIDTH - box.width) / 2 - box.x;
	y = (HEIGHT - box.height) / 2 - box.y;
	XmbDrawString(dpy, pix, fontset, gc, x, y, text, len);
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
	Window win;
	Pixmap pix;
	XFontSet fontset;
	GC gc;

	if (argc != 2) {        /* we need one argument */
		fprintf(stderr, "usage: xoutput string\n");
		return 1;
	}

	/* set locale */
	if (!setlocale(LC_ALL, "") || !XSupportsLocale())
		warnx("warning: no locale support");

	/* open connection to the server */
	if ((dpy = XOpenDisplay(NULL)) == NULL)
		errx(1, "could not open display");
	screen = DefaultScreen(dpy);
	depth = DefaultDepth(dpy, screen);
	root = RootWindow(dpy, screen);

	/* setup resources */
	win = createwin(POSX, POSY, WIDTH, HEIGHT, BORDER);
	gc = XCreateGC(dpy, root, 0, NULL);
	pix = createpix(gc, WIDTH, HEIGHT);
	fontset = createfontset(FONT);

	/* draw argv[1] on the pixmap */
	draw(pix, gc, fontset, argv[1]);

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
	XFreeFontSet(dpy, fontset);

	/* close connection to the server */
	XCloseDisplay(dpy);

	return 0;
}
