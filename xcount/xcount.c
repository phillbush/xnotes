#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>

#define POSX    200
#define POSY    200
#define WIDTH   350
#define HEIGHT  250
#define BORDER  10
#define STRINGX 175
#define STRINGY 125

static Display *dpy;
static Window root;
static int screen;
static GC gc;

/* setup top-level window */
static Window
createwin(int x, int y, int w, int h, int b)
{
	XSetWindowAttributes swa;
	Window win;

	/* create top-level window */
	swa.background_pixel = WhitePixel(dpy, screen);
	swa.border_pixel = BlackPixel(dpy, screen);
	swa.event_mask = ExposureMask | ButtonPressMask | KeyPressMask;
	win = XCreateWindow(dpy, root, x, y, w, h, b,
	                    CopyFromParent, CopyFromParent, CopyFromParent,
	                    CWBackPixel | CWBorderPixel | CWEventMask,
	                    &swa);

	return win;
}

/* draw string on window */
static void
draw(Window win, char *s)
{
	/* first we clear what is already drawn, for we to not overwrite  */
	XSetForeground(dpy, gc, WhitePixel(dpy, screen));
	XFillRectangle(dpy, win, gc, 0, 0, WIDTH, HEIGHT);

	/* now we can draw our string */
	XSetForeground(dpy, gc, BlackPixel(dpy, screen));
	XDrawString(dpy, win, gc, STRINGX, STRINGY, s, strlen(s));
}

/* enter the event loop */
static void
run(void)
{
	XEvent ev;
	int n = 0;              /* the number we are counting */
	char buf[32] = "0";     /* the string to write in the window */

	while(XNextEvent(dpy, &ev) == 0) {
		switch(ev.type) {
		case Expose:
			if (ev.xexpose.count == 0)
				draw(ev.xexpose.window, buf);
			break;
		case ButtonPress:
			if (ev.xbutton.button == Button1)
				n++;
			else if (ev.xbutton.button == Button2)
				n = 0;
			else if (ev.xbutton.button == Button3)
				n--;
			snprintf(buf, sizeof buf, "%d", n);
			draw(ev.xbutton.window, buf);
			break;
		case KeyPress:
			if (XkbKeycodeToKeysym(dpy, ev.xkey.keycode, 0, 0) == XK_q)
				return;
			break;
		}
	}
}

/* xcounter: count with the mouse */
int
main(void)
{
	Window win;

	/* open connection to the server */
	if ((dpy = XOpenDisplay(NULL)) == NULL)
		errx(1, "could not open display");
	screen = DefaultScreen(dpy);
	root = RootWindow(dpy, screen);

	/* setup resources */
	win = createwin(POSX, POSY, WIDTH, HEIGHT, BORDER);
	gc = XCreateGC(dpy, root, 0, NULL);

	/* map the windows */
	XMapWindow(dpy, win);

	/* run main loop */
	run();

	/* unmap the window */
	XUnmapWindow(dpy, win);

	/* close resources */
	XDestroyWindow(dpy, win);

	/* close connection to the server */
	XCloseDisplay(dpy);

	return 0;
}
