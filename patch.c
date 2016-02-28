
/*
*    ybattery version 0.1
*    Copyright (C) 2009 poiut.web.fc2.com
*
*    This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; either version 3 of the License, or (at your option) any later version.
*
*    This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
*
*    You should have received a copy of the GNU General Public License along with this program. If not, see <http://www.gnu.org/licenses/>.
*/
#include<X11/Xlib.h>
#include<X11/Xutil.h>
#include<X11/Xatom.h>
#include<X11/cursorfont.h>
#include<X11/xpm.h>
#include<stdio.h>
#include<unistd.h>
#include<string.h>
#include<pthread.h>
#include "battery.xpm"
#include "ac.xpm"
#include "alarm.xpm"
#define MX 150 /*Display width - MX = ybattery X position*/
#define MY 36  /*Display height - MY = ybattery Y position*/
#define ALARM_MX 400 /*Display width - ALARM_MX = alarm window X position*/
#define ALARM_MY 400 /*Display height - ALARM_MY = alarm window Y position*/
#define WIDTH 24
#define HEIGHT 24
#define ALARM_W 108
#define ALARM_H 36
#define ALARMLINE 2
#define BATTERY_GAUGE_COLOR "dark green"
static Atom del;
static Atom motifhint , wm_state , wm_state_prop[2] , win_type , win_type_nakami;
static Display	*display;
static Window	mainwindow;
static Window	alarmwindow;
static unsigned long	black,white;
Bool ac;
struct hintstruct{
	unsigned long flags;
	unsigned long functions;
	unsigned long decorations;
	long input_mode;
	unsigned long status;
};

Cursor cursor;
Bool alarmflag;
Bool baloon_opened;
Bool procflag ;
int battery_gauge;
int x,y;
pthread_mutex_t mu;
FILE *procinfofp , *procstatefp , *procacfp ;/*thread2*/
FILE *maxfp , *alarmfp , *curfp , *acfp;/*thread2*/
int openproc(void); /*thread2*/
void *thread1(void *);
void thread1_clean(void *);
void mainloop(void);
void drawbattery(void);
void draw_warning(void);

int main(int argc,char **argv)
{
	XTextProperty wname;
	XSizeHints xsh;
	XWMHints xwmh;
	XClassHint xch;
	pthread_t thread;
	pthread_attr_t pat;
	int screen_num;
	Screen *screen;
	Window	rootwindow;
	struct hintstruct hs={2,0,0,0,0};
	/*Open battery information files */
	if(XInitThreads()==0)
	{
		puts("Xlib not supported multi thread. exit.");
		return 2;
	}
	if( (display=XOpenDisplay("")) ==0)
	{
		puts("Cannot open display. exit.");
		return 3;
	}
	pthread_mutex_init(&mu,NULL);
	/*get screen*/
	screen_num=DefaultScreen(display);
	screen=ScreenOfDisplay(display,screen_num);
	rootwindow=DefaultRootWindow(display);

	/* create window */
	white=WhitePixel(display,screen_num);
	black=BlackPixel(display,screen_num);
	mainwindow=XCreateSimpleWindow(display,rootwindow,WidthOfScreen(screen)-MX,HeightOfScreen(screen)-MY,WIDTH,HEIGHT,1,black,white);
	/*set cursor*/
	cursor=XCreateFontCursor(display,XC_fleur);
	XDefineCursor(display,mainwindow,cursor);

	/* Drag event Mask */
	XSelectInput(display,mainwindow,Button1MotionMask|ButtonPressMask|ExposureMask);
	/* set window properties */
	wname.value=(unsigned char*)"ybattery";
	wname.encoding=XA_STRING;
	wname.format=8;
	wname.nitems=8;
	xsh.width=1;
	xsh.height=1;
	xsh.x=WidthOfScreen(screen)-MX;
	xsh.y=HeightOfScreen(screen)-MY;
	xsh.min_width=WIDTH;
	xsh.min_height=HEIGHT;
	xsh.min_aspect.x=xsh.max_aspect.x=(int)(WIDTH/HEIGHT)*16384;
	xsh.min_aspect.y=xsh.max_aspect.y=16384;
	xsh.flags=USPosition|USSize|PMinSize|PAspect;
	xwmh.input=False;
	xwmh.initial_state=NormalState;
	xwmh.flags=InputHint|StateHint;
	xch.res_name="ybattery";
	xch.res_class="Ybattery";
	XSetWMProperties(display,mainwindow,&wname,&wname,argv,argc,&xsh,&xwmh, &xch);

	del=XInternAtom(display,"WM_DELETE",False);
	XSetWMProtocols(display,mainwindow,&del,1);

	wm_state=XInternAtom(display,"_NET_WM_STATE",False);
	wm_state_prop[0]=XInternAtom(display,"_NET_WM_STATE_STICKY",False);
	wm_state_prop[1]=XInternAtom(display,"_NET_WM_STATE_SKIP_TASKBAR",False);
	XChangeProperty(display,mainwindow,wm_state,XA_ATOM,32,PropModeReplace,(unsigned char *)wm_state_prop,2);

	motifhint=XInternAtom(display,"_MOTIF_WM_HINTS",False);
	XChangeProperty(display,mainwindow,motifhint,motifhint,32,PropModeReplace,(unsigned char *)&hs,5);

	win_type=XInternAtom(display,"_NET_WM_WINDOW_TYPE",False);
	win_type_nakami=XInternAtom(display,"_NET_WM_WINDOW_TYPE_DOCK",False);
	XChangeProperty(display,mainwindow,win_type,XA_ATOM,32,PropModeReplace,(unsigned char *)&win_type_nakami,1);
	/*Map window */
	XMapWindow(display,mainwindow);

	baloon_opened=False;
	/**/
	pthread_mutex_init(&mu,NULL);
	/* create file watch thread */
	pthread_attr_init(&pat);
	pthread_create(&thread,&pat,thread1,NULL);
	/*go to event loop */	
	XFlush(display);
	mainloop();
	/*exit this program*/
	pthread_cancel(thread);
	XFreeCursor(display,cursor);
	XFree(&wm_state_prop[0]);XFree(&wm_state_prop[1]);XFree(&wm_state);
	XFree(&motifhint);XFree(&win_type);XFree(&del);
	if(baloon_opened)
	{
		XFreeCursor(display,cursor);
		XDestroyWindow(display,alarmwindow);
	}
	XDestroyWindow(display,mainwindow);
	XCloseDisplay(display);
	return 0;
}

void mainloop(void)
{
	XEvent event;
	Display *dpy;
	Window mw , aw;
	for (;;)
	{
		pthread_mutex_lock(&mu);
		dpy=display;
		mw=mainwindow;
		aw=alarmwindow;
		pthread_mutex_unlock(&mu);
		XNextEvent(dpy,&event);
		switch(event.type)
		{
			case Expose:
				if(event.xany.window==mw)
				{
					drawbattery();
				}
				draw_warning();
			break;
			case ButtonPress:
					x=event.xbutton.x;
					y=event.xbutton.y;
			break;
			case MotionNotify:
				XMoveWindow(event.xany.display,event.xany.window,event.xmotion.x_root-x,event.xmotion.y_root-y);
			break;
			case ClientMessage:
				if(event.xany.window==mw && event.xclient.data.l[0]==del)/*WM_DELETE*/
				{
					return;
				}else if(event.xany.window==aw && event.xclient.data.l[0]==del)
				{
					XDestroyWindow(dpy,alarmwindow);
					pthread_mutex_lock(&mu);
					baloon_opened=False;
					pthread_mutex_unlock(&mu);
				}
				break;
		}
	}
}

void *thread1(void * arg)
{
	char buf[40];
	Display *dpy;
	XEvent xe;
	int alarm , max , cur  ;/*int = 32bit*/
	/*open file*/
	pthread_mutex_lock(&mu);
	dpy=display;
	pthread_mutex_unlock(&mu);
	memset(&xe,0,sizeof(xe));
	pthread_cleanup_push(thread1_clean,NULL);
	for(;;)
	{
		procflag=True;
		if((curfp=fopen("/sys/class/power_supply/BAT/charge_now","r"))==NULL)		{
			if(openproc())
				return "err";
		}else if((alarmfp=fopen("/sys/class/power_supply/BAT/alarm","r"))==NULL)
		{
			fclose(curfp);
			if(openproc())
				return "err";
		}else if ((maxfp=fopen("/sys/class/power_supply/BAT/charge_full","r"))==NULL)
		{
			fclose(alarmfp);
			if(openproc())
				return "err";
		}else if((acfp=fopen("/sys/class/power_supply/ACAD/online","r"))==NULL)
		{
			fclose(maxfp);
			if(openproc())
				return "err";
		}
		else
			procflag=False;

		/* read /proc/acpi information */
		if (procflag)
		{
			while(fgets(buf,sizeof(buf)-1,procinfofp))
			{
				if(5==sscanf(buf,"last full capacity:       %d mAh",&max))
					break;
			}
			while(fgets(buf,sizeof(buf)-1,procinfofp))
			{
				if(5==sscanf(buf,"design capacity warning:       %d mAh",&alarm))
					break;
			}

			while(fgets(buf,sizeof(buf)-1,procstatefp))
			{
				if(4==sscanf(buf,"remaining capacity:      %d mAh",&cur))
					break;
			}
			fgets(buf,sizeof(buf)-1,procacfp);

			pthread_mutex_lock(&mu);
			if(strstr(buf,"on-line"))
				ac=True;
			else
				ac=False;
			pthread_mutex_unlock(&mu);

		}else /*read /sys/class/power_supply information*/
		{
			fscanf(curfp,"%d",&cur);
			fscanf(maxfp,"%d",&max);
			fscanf(alarmfp,"%d",&alarm);
			pthread_mutex_lock(&mu);
			fscanf(acfp,"%d",&ac);
			pthread_mutex_unlock(&mu);
		}
		pthread_mutex_lock(&mu);
		battery_gauge=(int)(cur/(max/20));
		if(alarm)
		{
			if(cur<=alarm)
				alarmflag=True;
			else
				alarmflag=False;
		}else
		{
			if(battery_gauge-ALARMLINE<=0)
				alarmflag=True;
			else
				alarmflag=False;
		}
		pthread_mutex_unlock(&mu);
		/*close files*/
		if (procflag)
		{
			fclose(procinfofp);
			procinfofp=NULL;
			fclose(procstatefp);
			procstatefp=NULL;
			fclose(procacfp);
			procacfp=NULL;
		}else
		{
			fclose(curfp);
			curfp=NULL;
			fclose(maxfp);
			maxfp=NULL;
			fclose(alarmfp);
			alarmfp=NULL;
			fclose(acfp);
			acfp=NULL;
		}
		/*Send Expose Event to thread1*/
		xe.xany.type=Expose;
		xe.xany.window=mainwindow;
		xe.xexpose.display=dpy;
		xe.xexpose.width=WIDTH;
		xe.xexpose.height=HEIGHT;
		xe.xexpose.count=False;
		XSendEvent(dpy,mainwindow,True,ExposureMask,&xe);
		XFlush(dpy);
		/*sleep*/
		sleep(2);

	}
	pthread_cleanup_pop(1);
	return NULL;
}
void thread1_clean(void *arg)
{
	if(procflag)
	{
		if(procinfofp)
			fclose(procinfofp);
		if(procstatefp)
			fclose(procstatefp);
		if(procacfp)
			fclose(procacfp);
	}else
	{
		if(curfp)
			fclose(curfp);
		if(maxfp)
			fclose(maxfp);
		if(alarmfp)
			fclose(alarmfp);
		if(acfp)
			fclose(acfp);
	}
}

int openproc(void)
{
	if((procinfofp=fopen("/proc/acpi/battery/BAT1/info","r"))==NULL)
	{
		puts("Battery infomation not found.");
		return -1;
	}else if((procstatefp=fopen("/proc/acpi/battery/BAT1/state","r"))==NULL)
	{
		fclose(procinfofp);
		puts("Battery information not found.");
		return -1;
	}else if((procacfp=fopen("/proc/acpi/ac_adapter/AC/state","r"))==NULL)
	{
		fclose(procstatefp);
		puts("AC adapter information not found.");
		return -1;
	}
	return 0;
}
void drawbattery(void)
{
	XImage *img , *shapemask;
	int  i , j , bg;
	Bool ac_pluged;
	Display *dpy;
	Colormap cmap;
	XColor color,exact;
	int screen_num;
	pthread_mutex_lock(&mu);
	ac_pluged=ac;
	dpy=display;
	bg=battery_gauge;
	pthread_mutex_unlock(&mu);
	/*Draw battery gauge */
	i=HEIGHT-1-bg;
	screen_num=DefaultScreen(dpy);
	cmap=DefaultColormap(dpy,screen_num);
	XAllocNamedColor(dpy,cmap,BATTERY_GAUGE_COLOR,&color,&exact);
	if(ac_pluged)
	{
		XpmCreateImageFromData(dpy,ac_xpm,&img,&shapemask,NULL);
		for(;i<=(HEIGHT-1);i++)
			for(j=1;j<=14;j++)
				XPutPixel(img,j,i,color.pixel);
	}else
	{
		XpmCreateImageFromData(dpy,battery_xpm,&img,&shapemask,NULL);
		for(;i<=(HEIGHT-1);i++)
			for(j=1;j<=(WIDTH-2);j++)
				XPutPixel(img,j,i,color.pixel);
	}
	XPutImage(dpy,mainwindow,DefaultGC(dpy,screen_num),img,0,0,0,0,24,24);
	XDestroyImage(img);
	XFreeColors(dpy,cmap,&(color.pixel),1,0);
	/*XDestroyImage(shapemask);*/
	/**/
}

void draw_warning(void)
{
	Display *dpy;
	Bool ac_pluged , alarm , wwo;
	Window aw;
	pthread_mutex_lock(&mu);
	ac_pluged=ac;
	alarm=alarmflag;
	wwo=baloon_opened;
	dpy=display;
	aw=alarmwindow;
	pthread_mutex_unlock(&mu);

	if( alarm && !ac_pluged )
	{
		int screen_num;
		Screen *s;
		screen_num=DefaultScreen(dpy);
		s=ScreenOfDisplay(dpy,screen_num);
		if(!wwo)
		{
			/*make "LOW BATTERY"  alarm window*/
			Window root;
			struct hintstruct hs={2,0,0,0,0};
			XTextProperty xtp={(unsigned char*)"Low battery",XA_STRING,8,12};
			root=DefaultRootWindow(dpy);
			pthread_mutex_lock(&mu);
			alarmwindow=XCreateSimpleWindow(dpy,root,s->width-ALARM_MX,s->height-ALARM_MY ,ALARM_W,ALARM_H,1,BlackPixel(dpy,screen_num),WhitePixel(dpy,screen_num));
			aw=alarmwindow;
			pthread_mutex_unlock(&mu);
			XSetWMProtocols(dpy,aw,&del,1);
			XSetWMName(dpy,aw,&xtp);
			XChangeProperty(dpy,alarmwindow,motifhint,motifhint,32,PropModeReplace,(unsigned char *)&hs,5);
			XChangeProperty(dpy,alarmwindow,win_type,XA_ATOM,32,PropModeReplace,(unsigned char *)&win_type_nakami,1);
			XChangeProperty(dpy,alarmwindow,wm_state,XA_ATOM,32,PropModeReplace,(unsigned char *)wm_state_prop,2);
			XSelectInput(dpy,aw,Button1MotionMask|ButtonPressMask|ExposureMask); 
			XDefineCursor(display,alarmwindow,cursor);
			XMapWindow(dpy,aw);
			XFlush(dpy);
			pthread_mutex_lock(&mu);
			baloon_opened=True;
			pthread_mutex_unlock(&mu);

		}else
		{
			XImage *img , *mask;
			/*draw "LOW BATTERY"*/
			XpmCreateImageFromData(dpy,alarm_xpm,&img,&mask,NULL);
			XPutImage(dpy,aw,DefaultGC(dpy,screen_num),img,0,0,0,0,ALARM_W,ALARM_H);
			XDestroyImage(img);
		}
	}else
	{
		if(wwo)
		{
			/*destroy alarm window*/
			XDestroyWindow(dpy,aw);
			pthread_mutex_lock(&mu);
			baloon_opened=False;
			pthread_mutex_unlock(&mu);
		}
	}
}

