#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>

#define POSX    200
#define POSY    200
#define WIDTH   350
#define HEIGHT  250
#define BORDER  10
#define LINE    2

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
	swa.event_mask = ExposureMask | ButtonPressMask | Button1MotionMask | ButtonReleaseMask | KeyPressMask;
	win = XCreateWindow(dpy, root, x, y, w, h, b,
	                    CopyFromParent, CopyFromParent, CopyFromParent,
	                    CWBackPixel | CWBorderPixel | CWEventMask,
	                    &swa);

	return win;
}

/* create a graphics context */
static GC
creategc(int linewidth)
{
	GC gc;
	XGCValues values;
	unsigned long valuemask;

	/* setup the values for our graphics context */
	values.foreground = BlackPixel(dpy, screen);
	values.background = WhitePixel(dpy, screen);
	values.cap_style = CapButt;     /* line's end-point is straight */
	values.join_style = JoinBevel;  /* join lines in "bevelian" style */
	values.fill_style = FillSolid;
	values.line_style = LineSolid;
	values.line_width = linewidth;  /* line width in pixels */

	/* set valuemask to the values we have set */
	valuemask = GCForeground | GCBackground | GCCapStyle | GCJoinStyle |
	            GCFillStyle | GCLineStyle | GCLineWidth;

	/* create our graphics context */
	gc = XCreateGC(dpy, root, valuemask, &values);

	return gc;
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

/* enter the event loop */
static void
run(GC gc, Pixmap pix)
{
	XEvent ev;
	int init = 0;   /* whether we are drawing a line */
	int lx, ly;     /* last x and y positions */

	while(XNextEvent(dpy, &ev) == 0) {
		switch(ev.type) {
		case Expose:
			XCopyArea(dpy, pix, ev.xexpose.window, gc,
			          ev.xexpose.x, ev.xexpose.y,
			          ev.xexpose.width, ev.xexpose.height,
			          ev.xexpose.x, ev.xexpose.y);
			break;
		case ButtonPress:
			XDrawPoint(dpy, pix, gc, ev.xbutton.x, ev.xbutton.y);
			XDrawPoint(dpy, pix, gc, ev.xbutton.x, ev.xbutton.y);
			XCopyArea(dpy, pix, ev.xbutton.window, gc, 0, 0, WIDTH, HEIGHT, 0, 0);
			break;
		case MotionNotify:
			if (init) {
				XDrawLine(dpy, pix, gc, lx, ly, ev.xbutton.x, ev.xbutton.y);
			} else {
				XDrawPoint(dpy, pix, gc, ev.xbutton.x, ev.xbutton.y);
				init = 1;
			}
			XCopyArea(dpy, pix, ev.xbutton.window, gc, 0, 0, WIDTH, HEIGHT, 0, 0);
			lx = ev.xbutton.x;
			ly = ev.xbutton.y;
			break;
		case ButtonRelease:
			init = 0;
			break;
		case KeyPress:
			if (XkbKeycodeToKeysym(dpy, ev.xkey.keycode, 0, 0) == XK_q)
				return;
			break;
		}
	}
}

/* xpaint: paint a line with the mouse */
int
main(void)
{
	Pixmap pix;
	Window win;
	GC gc;

	/* open connection to the server */
	if ((dpy = XOpenDisplay(NULL)) == NULL)
		errx(1, "could not open display");
	screen = DefaultScreen(dpy);
	depth = DefaultDepth(dpy, screen);
	root = RootWindow(dpy, screen);

	/* setup resources */
	win = createwin(POSX, POSY, WIDTH, HEIGHT, BORDER);
	gc = creategc(LINE);
	pix = createpix(gc, WIDTH, HEIGHT);

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

	/* close connection to the server */
	XCloseDisplay(dpy);

	return 0;
}
