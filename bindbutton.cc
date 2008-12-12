/*
 * Copyright (c) 2008, Thomas Jaeger <ThJaeger@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <X11/Xlib.h>
#include <X11/extensions/XInput.h>
#include <X11/extensions/XTest.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <map>

Display *dpy;
#define ROOT (DefaultRootWindow(dpy))

struct XiDevice {
	XDevice *dev;
	XEventClass classes[2];
	int press, release;
	unsigned int num_buttons;
};

int nMajor;
std::map<XID, XiDevice> devices;

struct Commands {
	const char *press;
	const char *release;
};

std::map<unsigned int, Commands> commands;

void init_xi() {
	int nFEV, nFER, n;
	if (!XQueryExtension(dpy,INAME,&nMajor,&nFEV,&nFER))
		exit(EXIT_FAILURE);
	XDeviceInfo *devs = XListInputDevices(dpy, &n);
	if (!devs)
		exit(EXIT_FAILURE);

	for (int i = 0; i < n; i++) {
		if (devs[i].use == IsXKeyboard || devs[i].use == IsXPointer)
			continue;

		XiDevice dev;

		dev.num_buttons = 0;
		XAnyClassPtr any = (XAnyClassPtr) (devs[i].inputclassinfo);
		for (int j = 0; j < devs[i].num_classes; j++) {
			if (any->c_class == ButtonClass) {
				XButtonInfo *info = (XButtonInfo *)any;
				dev.num_buttons = info->num_buttons;
			}
			any = (XAnyClassPtr) ((char *) any + any->length);
		}
		if (!dev.num_buttons)
			continue;

		dev.dev = XOpenDevice(dpy, devs[i].id);
		if (!dev.dev) {
			printf("Opening Device %s failed.\n", devs[i].name);
			continue;
		}

		DeviceButtonPress(dev.dev, dev.press, dev.classes[0]);
		DeviceButtonRelease(dev.dev, dev.release, dev.classes[1]);

		devices[devs[i].id] = dev;
	}
	XFreeDeviceList(devs);
}

void usage(const char *cmd) {
	printf("Usage: %s <button 1> <press command 1> <release command 1>\n", cmd);
	printf("          [<button 2> <press command 2> <release command 2>]...\n");
}

void parse_args(int argc, char **argv) {
	if ((argc % 3) != 1 || argc <= 3) {
		usage(argv[0]);
		exit(EXIT_SUCCESS);
	}
	for (int i = 0; 3*i+3 < argc; i++) {
		Commands cmds;
		cmds.press = argv[3*i+2];
		cmds.release = argv[3*i+3];
		int button = atoi(argv[3*i+1]);
		if (!button) {
			usage(argv[0]);
			exit(EXIT_FAILURE);
		}
		commands[button] = cmds;
	}
}

void grab_buttons() {
	for (std::map<unsigned int, Commands>::iterator i = commands.begin(); i != commands.end(); i++) {
		XGrabButton(dpy, i->first, AnyModifier, ROOT, False, ButtonPressMask,
				GrabModeAsync, GrabModeAsync, None, None);
		for (std::map<XID, XiDevice>::iterator j = devices.begin(); j != devices.end(); j++) {
			if (i->first > j->second.num_buttons)
				continue;
			XGrabDeviceButton(dpy, j->second.dev, i->first, AnyModifier, NULL,
					ROOT, False, 2, j->second.classes, GrabModeAsync, GrabModeAsync);
		}
	}
}

bool run_cmd(const char *cmd) {
	switch (fork()) {
		case 0:
			execlp("/bin/sh", "sh", "-c", cmd, NULL);
			exit(EXIT_FAILURE);
		case -1:
			printf("Error: can't execute command %s: fork failed\n", cmd);
	}
	return true;

}

int main(int argc, char **argv) {
	dpy = XOpenDisplay(NULL);

	parse_args(argc, argv);
	init_xi();
	grab_buttons();

	while (1) {
		XEvent ev;
		XNextEvent(dpy, &ev);
		if (ev.type == ButtonPress) {
			XTestFakeButtonEvent(dpy, ev.xbutton.button, False, CurrentTime);
			continue;
		}
		for (std::map<XID, XiDevice>::iterator j = devices.begin(); j != devices.end(); j++) {
			if (ev.type == j->second.press) {
				XDeviceButtonEvent* bev = (XDeviceButtonEvent *)&ev;
				std::map<unsigned int, Commands>::iterator i = commands.find(bev->button);
				if (i != commands.end())
					run_cmd(i->second.press);
				goto cont;
			}
			if (ev.type == j->second.release) {
				XDeviceButtonEvent* bev = (XDeviceButtonEvent *)&ev;
				std::map<unsigned int, Commands>::iterator i = commands.find(bev->button);
				if (i != commands.end())
					run_cmd(i->second.release);
				goto cont;
			}
		}
		printf("Unknown event\n");
cont:
		;
	}
}
