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
#include <string.h>
#include <unistd.h>

#include <list>
#include <map>
#include <set>

Display *dpy;
#define ROOT (DefaultRootWindow(dpy))

bool debug, always_grab;
const char *device_name;

struct XiDevice {
	XDevice *dev;
	XEventClass classes[2];
	int press, release;
	unsigned int num_buttons;
	std::set<unsigned int> status;

	void grab() {
		if (debug)
			printf("Grabbing device %ld\n", dev->device_id);
		int status = XGrabDevice(dpy, dev, ROOT, False, 2, classes,
				GrabModeAsync, GrabModeAsync, CurrentTime);
		switch (status) {
			case GrabSuccess:
				break;
			case AlreadyGrabbed:
				printf("Grab error: Already grabbed\n");
				break;
			case GrabNotViewable:
				printf("Grab error: Not viewable\n");
				break;
			case GrabFrozen:
				printf("Grab error: Frozen\n");
				break;
			case GrabInvalidTime:
				printf("Grab error: Invalid Time\n");
				break;
			default:
				printf("Grab error: Unknown\n");
		}
	}

	void ungrab() {
		if (debug)
			printf("Ungrabbing device %ld\n", dev->device_id);
		XUngrabDevice(dpy, dev, CurrentTime);
	}
};

std::list<XiDevice> devices;

struct Commands {
	const char *press;
	const char *release;
};

std::map<unsigned int, Commands> commands;

void init_xi() {
	int n;
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

		if (device_name && strcasecmp(device_name, devs[i].name))
			continue;

		dev.dev = XOpenDevice(dpy, devs[i].id);
		if (!dev.dev) {
			printf("Opening Device %s failed.\n", devs[i].name);
			continue;
		}

		DeviceButtonPress(dev.dev, dev.press, dev.classes[0]);
		DeviceButtonRelease(dev.dev, dev.release, dev.classes[1]);

		devices.push_back(dev);
	}
	XFreeDeviceList(devs);
	if (devices.size() == 0) {
		printf("Error: No devices found\n");
		exit(EXIT_FAILURE);
	}
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
	debug = !!getenv("DEBUG");
	always_grab = !!getenv("ALWAYS_GRAB");
	device_name = getenv("DEVICE");
}


void grab_buttons() {
	if (always_grab) {
		printf("Grabbing XInput devices...\n");
		for (std::list<XiDevice>::iterator j = devices.begin(); j != devices.end(); j++)
			j->grab();
	}
	for (std::map<unsigned int, Commands>::iterator i = commands.begin(); i != commands.end(); i++) {
		XGrabButton(dpy, i->first, AnyModifier, ROOT, False, ButtonPressMask,
				GrabModeSync, GrabModeAsync, None, None);
		if (always_grab)
			continue;
		for (std::list<XiDevice>::iterator j = devices.begin(); j != devices.end(); j++) {
			if (i->first > j->num_buttons)
				continue;
			XGrabDeviceButton(dpy, j->dev, i->first, AnyModifier, NULL,
					ROOT, False, 2, j->classes, GrabModeAsync, GrabModeAsync);
		}
	}
}

void run_cmd(const char *cmd) {
	if (system(cmd) == -1)
		fprintf(stderr, "Error: system() failed\n");
}

struct Event {
	bool is_press;
	unsigned int button;
	XiDevice *dev;
	bool core;
	Time t;
	bool get();
	void handle();
	bool combine(Event &ev);
};

bool Event::get() {
	XEvent ev;
	XNextEvent(dpy, &ev);

	if (ev.type == ButtonPress) {
		is_press = true;
		button = ev.xbutton.button;
		dev = NULL;
		core = true;
		t = ev.xbutton.time;
		if (debug)
			printf("Button %d pressed (core)\n", button);
		return true;
	}
	for (std::list<XiDevice>::iterator j = devices.begin(); j != devices.end(); j++) {
		if (ev.type == j->press) {
			XDeviceButtonEvent* bev = (XDeviceButtonEvent *)&ev;
			is_press = true;
			button = bev->button;
			dev = &(*j);
			core = false;
			t = bev->time;
			if (debug)
				printf("Button %d pressed (Xi)\n", button);
			return true;
		}
		if (ev.type == j->release) {
			XDeviceButtonEvent* bev = (XDeviceButtonEvent *)&ev;
			is_press = false;
			button = bev->button;
			dev = &(*j);
			core = false;
			t = bev->time;
			if (debug)
				printf("Button %d released (Xi)\n", bev->button);
			return true;
		}
	}
	printf("Unknown event\n");
	return false;
}

void Event::handle() {
	if (core && is_press) {
		if (dev) {
			XTestFakeButtonEvent(dpy, button, False, CurrentTime);
			XAllowEvents(dpy, AsyncBoth, t);
		} else {
			XAllowEvents(dpy, ReplayPointer, t);
		}
	}

	if (!dev)
		return;

	std::map<unsigned int, Commands>::iterator i = commands.find(button);
	if (i != commands.end())
		run_cmd(is_press ? i->second.press : i->second.release);
	if (always_grab)
		return;
	if (is_press) {
		if (!dev->status.size())
			dev->grab();
		dev->status.insert(button);
	} else {
		dev->status.erase(button);
		if (!dev->status.size())
			dev->ungrab();
	}
}

bool Event::combine(Event &ev) {
	if (is_press != ev.is_press)
		return false;
	if (button != ev.button)
		return false;
	if (t != ev.t)
		return false;
	if (core && !dev && !ev.core && ev.dev) {
		dev = ev.dev;
		return true;
	}
	if (!core && dev && ev.core && !ev.dev) {
		core = ev.core;
		return true;
	}
	return false;
}

int main(int argc, char **argv) {
	dpy = XOpenDisplay(NULL);

	parse_args(argc, argv);
	init_xi();
	grab_buttons();

	Event queue[2];
	int queue_size = 0;

	while (1) {
		while (queue_size < 2 && (!queue_size || XPending(dpy))) {
			if (queue[queue_size].get())
				queue_size++;
			else
				continue;
		}
		if (queue_size == 2 && queue[0].combine(queue[1]))
			queue_size = 1;
		for (int i = 0; i < queue_size; i++)
			queue[i].handle();
		queue_size = 0;
	}
}
