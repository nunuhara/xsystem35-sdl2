/*
 * sdl_darw.c  SDL event handler
 *
 * Copyright (C) 2000-     Fumihiko Murata       <fmurata@p1.tcnet.ne.jp>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
*/
/* $Id: sdl_event.c,v 1.5 2001/12/16 17:12:56 chikama Exp $ */

#include "config.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <SDL.h>

#include "portab.h"
#include "system.h"
#include "counter.h"
#include "nact.h"
#include "sdl_private.h"
#include "key.h"
#include "menu.h"
#include "imput.h"
#include "joystick.h"
#include "sdl_input.c"

static void sdl_getEvent(void);
static void keyEventProsess(SDL_KeyboardEvent *e, boolean bool);
static int  check_button(void);


/* pointer の状態 */
static int mousex, mousey, mouseb;
boolean RawKeyInfo[256];
/* SDL Joystick */
static int joyinfo=0;

static int mouse_to_rawkey(int button) {
	switch(button) {
	case SDL_BUTTON_LEFT:
		return KEY_MOUSE_LEFT;
	case SDL_BUTTON_MIDDLE:
		return KEY_MOUSE_MIDDLE;
	case SDL_BUTTON_RIGHT:
		return KEY_MOUSE_RIGHT;
	}
	return 0;
}

/* Event処理 */
static void sdl_getEvent(void) {
	static int cmd_count_of_prev_input = -1;
	SDL_Event e;
	boolean m2b = FALSE, msg_skip = FALSE;
	int i;
	boolean had_input = false;

	while (SDL_PollEvent(&e)) {
		had_input = true;
		switch (e.type) {
#ifndef __EMSCRIPTEN__
		case SDL_QUIT:
			nact->is_quit = TRUE;
			nact->wait_vsync = TRUE;
			break;
#endif
		case SDL_WINDOWEVENT:
			switch (e.window.event) {
			case SDL_WINDOWEVENT_ENTER:
				ms_active = true;
#if 0
				if (sdl_fs_on)
					SDL_WM_GrabInput(SDL_GRAB_ON);
#endif
				break;
			case SDL_WINDOWEVENT_LEAVE:
				ms_active = false;
#if 0
				if (sdl_fs_on)
					SDL_WM_GrabInput(SDL_GRAB_OFF);
#endif
				break;
			}
			break;
		case SDL_KEYDOWN:
			keyEventProsess(&e.key, TRUE);
			break;
		case SDL_KEYUP:
			keyEventProsess(&e.key, FALSE);
			if (e.key.keysym.sym == SDLK_F1) msg_skip = TRUE;
			if (e.key.keysym.sym == SDLK_F4) {
				sdl_fs_on = !sdl_fs_on;
				SDL_SetWindowFullscreen(sdl_window, sdl_fs_on ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
			}
			break;
		case SDL_MOUSEMOTION:
			mousex = e.motion.x;
			mousey = e.motion.y;
			break;
		case SDL_MOUSEBUTTONDOWN:
			mouseb |= (1 << e.button.button);
			RawKeyInfo[mouse_to_rawkey(e.button.button)] = TRUE;
#if 0
			if (e.button.button == 2) {
				keywait_flag=TRUE;
			}
#endif
			break;
		case SDL_MOUSEBUTTONUP:
			mouseb &= (0xffffffff ^ (1 << e.button.button));
			RawKeyInfo[mouse_to_rawkey(e.button.button)] = FALSE;
			if (e.button.button == 2) {
				m2b = TRUE;
			}
			break;

		case SDL_FINGERDOWN:
			if (SDL_GetNumTouchFingers(SDL_GetTouchDevice(0)) >= 2) {
				mouseb &= ~(1 << SDL_BUTTON_LEFT);
				mouseb |= 1 << SDL_BUTTON_RIGHT;
				RawKeyInfo[mouse_to_rawkey(SDL_BUTTON_LEFT)] = FALSE;
				RawKeyInfo[mouse_to_rawkey(SDL_BUTTON_RIGHT)] = TRUE;
			} else {
				mouseb |= 1 << SDL_BUTTON_LEFT;
				RawKeyInfo[mouse_to_rawkey(SDL_BUTTON_LEFT)] = TRUE;
				mousex = e.tfinger.x * view_w;
				mousey = e.tfinger.y * view_h;
			}
			break;

		case SDL_FINGERUP:
			if (SDL_GetNumTouchFingers(SDL_GetTouchDevice(0)) == 0) {
				mouseb &= ~(1 << SDL_BUTTON_LEFT | 1 << SDL_BUTTON_RIGHT);
				RawKeyInfo[mouse_to_rawkey(SDL_BUTTON_LEFT)] = FALSE;
				RawKeyInfo[mouse_to_rawkey(SDL_BUTTON_RIGHT)] = FALSE;
				mousex = e.tfinger.x * view_w;
				mousey = e.tfinger.y * view_h;
			}
			break;

		case SDL_FINGERMOTION:
			mousex = e.tfinger.x * view_w;
			mousey = e.tfinger.y * view_h;
			break;

#if HAVE_SDLJOY
		case SDL_JOYAXISMOTION:
			if (abs(e.jaxis.value) < 0x4000) {
				joyinfo &= e.jaxis.axis == 0 ? ~0xc : ~3;
			} else {
				i = (e.jaxis.axis == 0 ? 2 : 0) + 
					(e.jaxis.value > 0 ? 1 : 0);
				joyinfo |= 1 << i;
			}
			break;
		case SDL_JOYBALLMOTION:
			break;
		case SDL_JOYHATMOTION:
			break;
		case SDL_JOYBUTTONDOWN:
		case SDL_JOYBUTTONUP:
			if (e.jbutton.button < 4) {
				i = 1 << (e.jbutton.button+4);
				if (e.jbutton.state == SDL_PRESSED)
					joyinfo |= i;
				else
					joyinfo &= ~i;
			} else {
				if (e.jbutton.state == SDL_RELEASED) {
				}
			}
			break;
#endif
		default:
			printf("ev %x\n", e.type);
			break;
		}
	}
	if (had_input) {
		cmd_count_of_prev_input = nact->cmd_count;
	} else if (nact->cmd_count != cmd_count_of_prev_input) {
		nact->wait_vsync = TRUE;
	}
	
	if (m2b) {
		menu_open();
	}
	
	if (msg_skip) set_skipMode(!get_skipMode());
}

int sdl_keywait(int msec, boolean cancel) {
	int key=0, n;
	int end = msec == INT_MAX ? INT_MAX : get_high_counter(SYSTEMCOUNTER_MSEC) + msec;
	
	while ((n = end - get_high_counter(SYSTEMCOUNTER_MSEC)) > 0) {
		if (n <= 16)
			sdl_sleep(n);
		else
			sdl_wait_vsync();
		nact->callback();
		sdl_getEvent();
		key = check_button() | sdl_getKeyInfo() | joy_getinfo();
		nact->wait_vsync = FALSE;  // We just waited!
		if (cancel && key) break;
	}
	
	return key;
}

/* キー情報の取得 */
static void keyEventProsess(SDL_KeyboardEvent *e, boolean bool) {
	RawKeyInfo[sdl_keytable[e->keysym.scancode]] = bool;
}

int sdl_getKeyInfo() {
	int rt;
	
	sdl_getEvent();
	
	rt = ((RawKeyInfo[KEY_UP]     || RawKeyInfo[KEY_PAD_8])       |
	      ((RawKeyInfo[KEY_DOWN]  || RawKeyInfo[KEY_PAD_2]) << 1) |
	      ((RawKeyInfo[KEY_LEFT]  || RawKeyInfo[KEY_PAD_4]) << 2) |
	      ((RawKeyInfo[KEY_RIGHT] || RawKeyInfo[KEY_PAD_6]) << 3) |
	      (RawKeyInfo[KEY_RETURN] << 4) |
	      (RawKeyInfo[KEY_SPACE ] << 5) |
	      (RawKeyInfo[KEY_ESC]    << 6) |
	      (RawKeyInfo[KEY_TAB]    << 7));
	
	return rt;
}

static int check_button(void) {
	int m1, m2;
	
	m1 = mouseb & (1 << 1) ? SYS35KEY_RET : 0;
	m2 = mouseb & (1 << 3) ? SYS35KEY_SPC : 0;
	
	return m1 | m2;
}

int sdl_getMouseInfo(MyPoint *p) {
	sdl_getEvent();
	
	if (!ms_active) {
		if (p) {
			p->x = 0; p->y = 0;
		}
		return 0;
	}
	
	if (p) {
		p->x = mousex - winoffset_x;
		p->y = mousey - winoffset_y;
	}
	return check_button();
}

#ifdef HAVE_SDLJOY
int sdl_getjoyinfo(void) {
	sdl_getEvent();
	return joyinfo;
}
#endif
