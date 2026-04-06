# Programmer's Guide to MultiFinder

Apple Computer, Inc. — APDA — June 3, 1988 (revised June 3, 1989)

Extracted from OCR'd PDF. Some formatting artifacts may remain.

---

Macintosh®  Programmer's 

Guide to 
MultiFinder 

APDA™ 

June 3, 1988 

C Apple Computer, Inc. 1988 


---
\ 

\ 

trademark,  of AM  International, 
Inc. 

Simultaneously published in the 
United States and Canada. 

ti APPLE COMPUI'ER,  INC. 

This manual is copyrighted by 
Apple or by Apple's suppliers, 
with all rights reserved. Under 
the copyright laws, this manual 
may not be copied, in whole or 
in part, without the written 
consent of Apple Computer, 
Inc. This exception does not 
allow copies to be made for 
others, whether or not sold, but 
alf of the material purchased 
may be sold, given, or lent to 
another person. Under the law, 
copying includes translating into 
another language. 

C Apple Computer, Inc.,  1988 
20525 Mariani Avenue 
Cupertino, CA  95014 
(408) 9%-1010 

Apple,  the Apple logo, 
HyperCard,  LaserWriter,  and 
Macintosh are  registered 
trademarks of Apple Computer, 
Inc. 

APDA,  Finder,  MPW,  and 
MultiFinder are trademarks of 
Apple Computer, Inc. 

ITC Avant Garde Gothic, ITC 
Garamond, and ITC Zapf 
Dingbats are registered 
trademarks of International 
Typeface Corporation. 

MacPaint is a registered 
trademark of Claris Corporation. 

Microsoft is a  registered 
trademark of Miaosoft 
Corporation. 

POSTSCRIPT is a  registered 
trademark of Adobe Systems 
Incorporated. 

Varityper is a  registered 
trademark, and VT6oo is a 


---
Contents 

Pref ace  Welcome  to  MultlRnder  xx 

Organization of this manual  xx 
About the Apple Programmer's and Developer's Association  xx 
Some conventions used in this manual  xx 

Chapter 1 

Introduction  to  MultlFlnder  xx 

The traditional Macintosh user's model  xx 
The MultiFinder user's model  xx 

The desktop metaphor remains  xx 
Applications, windows, and the menu bar  xx 

The MultiFinder programmer's model  xx 

Cooperative multitasking  xx 
Background processing and event calls  xx 
Background notification  xx 

The three types of Macintosh applications  xx 

Well-behaved applications  xx 
MultiFinder-aware applications  xx 

Faster switching  xx 
A better dti2Jen  xx 
More flexible memory management  xx 

Special-purpose applications  xx 
Embedded services  xx 
Facel~ background tasks  xx 
Desk accessories  xx 

Chapter 2  Well-Behaved  AppllcaHons  xx 
Windows, menus, and screens  xx 

Don't modify Window Manager data structures directly  xx 
Don't manipulate Menu Manager data structures directly  xx 
Don't draw on the desktop  xx 

Iii 


---
WMgrPort and GrayRgn  xx 
Don't write directly to the screen  xx 
Don't save the contents of windows  xx 
Handle update events  xx 
Use null events properly  xx 
Support keyboard commands for editing  xx 

Memory management  xx 

Don't depend on the relative positions of the system and 

application heaps  xx 
Free space and heap size  xx 
Stack si2le  xx 
The application heap  xx 

The A5 world xx 

Trap patches and global data  xx 
Completion routines  xx 
VBL tasks  xx 
Tune Manager tasks  xx 
Defprocs  xx 

Miscellaneous guidelines  xx 

Low memory  xx 
.Asynchronous system calls  xx 
Exit time restrictions  xx 
System resources  xx 

Chapf• 3  MuUIFlnder·AWGM  Appllcatlon1  xx 

Suspend and resume events  xx 

Handling activate and deaaivate  xx 
Take care if masking out app4Evts  xx 
AC example of how to handle suspend and resume events  xx 

The SIZE resource  xx 

The SIZE resource flags  xx 
Preferred memory si7.e  xx 
Minimum memory si7.e  xx 
How to create your own SIZE resource  xx 
How can I tell if my application is running in the 

background?  xx 

Null events  xx 
WaitNextEvent  xx 

The mouseRgn parameter  xx 
The sleep parameter  xx 
Yielding time gracefully  xx 
Using unused null event time  xx 
Don't call SystemTask  xx 

iv 

Contents 


---
( 

When exactly are applications moved between the foreground 

and the background?  xx 

How can I tell if WaitNextEvent is implemented?  xx 

Temporary memory allocation calls  xx 

How can I tell if the temporary memory allocation calls are 

implemented?  xx 
Launching and sublaunching  xx 
Working directories  xx 

Chapter 4  Special-Purpose  Appllcatlons  xx 

Embedded services  xx 
Faceless background tasks  xx 
Desk acressories  xx 

Self-sufficient desk accessories  xx 
Dependent desk accessories  xx 
Error checking  xx 

Appendix A  AC Example of a  MultlFlnder·Aware AppllcaHon  xx 

Appendix B  A  Pascal  Example of a  MultlRnder·Aware Application  xx 

Appendix C  Resource  Descriptions tor the  Example  MultlFlnder-Aware 

Appllcatlon  xx 

Appendix 0  The  NotlflcaHon  Manager  xx 

How a notification happens  xx 
Creating a notification request  xx 
Notification Manager routines  xx 

NMinstall  xx 
NMRemove  xx 

Appenclx E  A Summary  of the  Multlflnder Traps  xx 

Temporary memory allocation calls  xx 
WaitNextEvent  xx 

Index  xx 

Contents 

v 


---

---
Preface 

Welcome  to  MultiFinder 

Programmer's Guide to Multi.Finder introduces MultiFinderTM,  a new set of operating 
system functions designed to increase the efficiency and functionality of the 
Macintosh®  family of computers.  In addition,  it outlines specific programming 
guidelines intended to help software developers write MultiFinder-compatible 
applications. 

This book is directed toward the proficient Macintosh application developer.  For an 
introductory discussion on how to write a Macintosh application, consult the 
introductory volumes of the Macintosh  Technical Library. 

This is not a dedicated reference manual  While the Macintosh programming 
paradigm remains largely unchanged, MultiFinder does reflect a departure from the 
original one-application-open-at-a-time desktop environment.  To appreciate  the 
nuances of how MultiFinder works and to achieve the highest degree of compatibility 
with MultiFinder-present and future versions-you should read this entire book.  Take 
a moment to scan the section •some Conventions Used in This Manual"; this section 
is  particularly important in this book.  Programmer's Guide to MultiFinderincludes 

o  a description of the traditional Macintosh user's model,  the MultiFinder user's 

model, and the MultiFinder programmer's model 

o  definitions of the three types of MultiFinder-friendly applications: well-behaved, 

MultiFinder-aware,  and special-purpose 

o  detailed descriptions of the new Macintosh programming features:  the SIZE 

resource, the event call WaitNextEvent,  the Notification Manager, suspend and 
resume events, and the new temporary memory allocation calls 

o  descriptions of cooperative multitasking and background notification 

o  programming guidelines for ensuring MultiFinder compatibility 
o  guidelines for designing well-behaved, MultiFinder-aware,  and special-purpose 

applications 

vii 


---
o  code examples for handling the new suspend and resume events, saving A5 and 

CurrentA5,  writing completion routines for asynchronous Device Manager calls, 
determining whether WaitNextEvent and the temporary memory allocation calls 
are implemented, and writing MultiFinder-aware applications 

Programmer's Guide to Mult1Finder does not include information about 
o  programming in general 
o  getting started as a developer 

To use this book, you should already be familiar with the information that's in Inside 
Macintosh and have experience writing Macintosh applications. 

For information about becoming a developer or obtaining developer support, write to 

Developer Programs 
Mail Stop 51-T 
Apple  Computer,  Inc. 
20525 Mariani Avenue 
Cupertino, CA 95014 

Organization of this manual 
This manual is organized as follows: 

o  Chapter  1,  "Introduction to MultiFinder,"  describes  the  traditional  Macintosh 

user's model,  the MultiFinder user's model, the MultiFinder programmer's model, 
and the different types of MultiFinder-friendly applications. 

o  Chapter 2,  "Well-Behaved Applications,•  describes the characteristics of 

applications that will work well in the MultiFinder environment.  Included here are 
general programming guidelines for the user interface and memory management, 
as well as a detailed discussion of the A5 world and some miscellaneous 
programming hints. 

o  Chapter 3,  •MultiFinder-Aware Applications,• introduces the  concept of an 

application that takes maximum advantage of the background processing time now 
available under MultiFinder.  Also included in this chapter are descriptions of the 
SIZE resource; the two new types of app4Evts, suspend and resume; WaitNextEvent; 
the new temporary memory allocation calls; and launching and sublaunching. 
o  Chapter 4,  •special-Purpose Applications,•  describes  three  different special 

applications: embedded services, faceless background tasks, and desk accessories. 

o  Appendix A,  •A C Example of a MultiFinder-Aware Application,• gives a C 

example of an application that is MultiFinder-aware. 

o  Appendix B,  •A Pascal Example of a MultiFinder-Aware Application,• gives a 

Pascal example of an application that is MultiFinder-aware. 

viii 

Preface: Welcome to MultlAnder 


---
o  Appendix C,  •Resource Descriptions for the Example MultiFinder-Aware 

Application,• lists the necessary resource descriptions for the MPwn' Rez tool used 
in the C and Pascal program examples of a MultiFinder-aware application. 

o  Appendix D,  •nie Notification Manager,•  provides a  detailed description of this 

new manager. 

o  Appendix E,  •A Summary of the MultiFinder Traps,• summarizes the new traps 

available to applications under MultiFinder. 

About  the  Apple  Programmer's  and  Developer's 
Association 
The Apple Programmer's and Developer's Association (APDA TM)  is an excellent 
source of technical information for anyone interested in developing Apple 
compattble products. Membership in the association allows you to purchase Apple 
technical documentation,  programming tools,  and utilities.  For information on 
membership fees,  available products, and prices, please contact 

APDA 
290 SW 43rd Street 
Renton, WA  980SS 
(2o6) 2S 1-6548 
AppleLink: APDA 
MCI: 312-7449 
CompuServe# 73527,27 

Some conventions used in this manual 
The following conventions have been adopted for use in this book: 
•  The AS world consists of an application's own global variables and its private set of 
QuickDraw globals (both are accessed through A5), the set of low-memory globals 
associated with the application by MultiFinder, and the application's heap and 
stack.  The single-FinderTM environment only allowed one AS world at any given 
time.  With MultiFinder active, there is an AS world for each open application. 
•  Cooperative multitasking is the result of a foreground application and one or 
more applications concurrently running in the background interactively and 
sharing a limited amount of resources. 

•  An embedded service is a special-purpose application that runs only in the 

background 

Some  conventions used In  this  manual 

Ix 


---
•  Event calls refer to GetNextEvent, WaitNextEvent (see Chapter 3 for a detailed 

desaiption of this new event call), and EventAvail. 

•  A faceless background task is another special-purpose application that is almost 
invisible.  It is minimal in siz.e and has no user interface-no icon will appear in the 
Apple menu and no windows will be displayed. 

•  A MultiFfncler-aware applic:adon is one that calls WaitNextEvent, handles 

suspend/resume events, specifies a SIZE resource, and optionally performs some 
background work without significantly affecting the responsive nature of any 
application running in the foreground. 

•  Null event processing time is the time when most applications sit idle because 

there are no events initiated by the user or no windows to be redrawn. 

•  A well-behaved application is one that follows the programming guidelines 

outlined in Chapter 2. 

x 

Preface: Welcome to MultlAnder 


---
Chapter  1 

Introduction  to  MultiFinder 

This chapter introduces the new set of multitasking operating system functions for 
Macintosh® computers, MultiFinderTM.  The name •MultiFinder" is actually 
misleading-MultiFinder is  not part of the FinderTM.  Instead, it is a set of additional 
operating system functions designed to allow an increased level of functionality with a 
minimal impact on the  Macintosh programming model.  MultiFinder is compatible 
with the Macintosh Plus, Macintosh SE,  and Macintosh II computers and resides in the 
System Folder. 

The additional functionality MultiFinder provides is essentially the ability to run 
multiple applications on the Macintosh, all of them sharing the available desktop. 
Great care has been taken to introduce this new functionality without noticeably 
altering the familiar Macintosh desktop. 

You should take note of a couple of important issues here.  First, at present, a user 
retains the ability to turn MultiFinder off or not run it at all--£eserving single 
application capability if desired  Second, MultiFinder will eventually represent the 
exclusive Macintosh desktop environment 

This chapter provides an overview of the traditional Macintosh desktop and the new 
MultiFinder desktop environments,  and explains cooperative multitasking and its role 
in the new MultiFinder programming model.  Finally, this chapter ends with a 
discussion of the different types of Macintosh applications. 


---
The traditional Macintosh user's model 
The original Macintosh user interface presented a powerful metaphor of the everyday 
desktop to both the Macintosh customer and software developer.  The Macintosh user 
has an enormous amount of freedom to custom.ire a personal desktop work space and 
clearly benefits from the high degree of compatibility among Macintosh applications. 

Each application running in the Macintosh single-Finder environment has control of 
the entire desktop because it knows the state of the entire Macintosh machine.  As part 
of  this original implementation, the desktop disappears while an application runs, 
and only one application can run during each work session. 

The Multifinder user's model 
MultiFinder introduces some welcome changes to the Macintosh desktop 
environment  Users can have any number of applications open at the same time, 
including the Finder, and can easily switch between them. 

For example, a user could have a word processing program, an accounting package, a 
communications program, and HyperCard®  all open simultaneously. 

When running under MultiFinder,  the Finder is not closed when another application is 
opened.  The user can activate the Finder or any open application in one of three 
ways: by clicking the appropriate name and icon in the Apple menu, by clicking the 
small icon in the upper-right corner of the menu bar (until the correct icon appears), 
or by clicking the desired window on the desktop. 

The  desktop metaphor remains 

MultiFinder has not restricted or limited the familiar desktop metaphor.  Since many 
applications can now be open at the same time, their windows can overlap each other 
and users can quickly switch between them-MultiFinder actually models a real 
working desktop better than the single-Finder environment does. 

Applications, windows, and the menu bar 

When a user opens an application under MultiFinder, that application becomes active 
and begins running in the foreground  The menu bar will contain the active 
application's menu titles and a small icon that represents the application in the 
rightmost corner of the menu bar.  Also, all the active application's windows will come 
to the frontmost layer of the desktop. 

2 

Chapter 1:  Introduction to  MultlFinder 


---
( 

For example, a user could have a word-processing application running in the 
foreground with three windows open for three different documents, as well as a 
number of other applications open simultaneously.  If the user brings one of the other 
applications to the foreground, performs some work, and then once again brings the 
word-processing application to the foreground, all three of its windows will come to 
the frontmost layer of the desktop. 

The MultiFinder programmer's model 
The user interface is the most important element of the Macintosh environment 
Although the MultiFinder engineers have taken great care to preserve the look and feel 
of the Macintosh interface, a similar responsibility also rests with developers who will 
write-MultiFinder-compatible applications.  While MultiFinder has dramatically 
extended the user interface of the Macintosh, the programming model remains 
basically intaCL  MultiFinder supports a new concept for the Macintosh-background 
processing-that allows applications not currently running in the foreground to make 
use of processing time that is unused by the current foreground application. 

MultiFinder uses this null event processing time-the time when most applications 
sit idle because there are no events initiated by the user or no windows to be 
redrawn-to allow other applications not running in the foreground to perform useful 
work.  The Finder, for example, now uses this time to keep the view within its windows 
and on the desktop consistent (disk insertions, files being added or removed, and so 
on).  Since this time is essentially wasted in the single-Finder environment, 
MultiFinder does not inhibit or noticeably slow the response time of the application 
running in the foreground 

In addition, if an application wants to take advantage of this unused null event 
processing time and perform some work in the background, it must take on some 
additional responsibility.  Applications running in the background must know how to 
coexist with foreground applications because the user is interacting with the 
foreground application and expects it to respond immediately. 

Cooperative multitasking 

Cooperative muldtasklng is the result of a foreground application and one or mote 
applications concurrently running in the background interactively sharing a limited 
amount of resources-the sort of kindness you might expect among individuals with 
good manners. 

The MUltlFlnder programmer's model 

3 


---
While most operating systems regulate this •sharing• by having the system parcel out 
contro~ MultiFinder relies on the willingness of foreground and background 
applications to share the available resources.  (MultiFinder does regulate the sharing 
of most resources, including microprocessor time; however, the application is 
allowed to decide when it will give up control of the miaoprocessor.)  This •kindness 
of strangers• philosophy between applications running in the foreground and those 
running in the background fonns the basis for MultiFinder's friendly cooperation. 

The burden of responsibility for this sharing of resources lies with all applications. 
Due to the cooperative nature of MultiFinder, if one application holds on to the 
microprocessor too long, the other applications will appear unresponsive. 

Background processing and event calls 

Applications running in the background receive processing time when the foreground 
application releases the microprocessor with a call to an event call and no events for 
the foreground application are pending.  Exceptions to this rule are mouse 
movements outside a predefined area, and after a predefined sleep value set by the 
foreground application has expired (see the desaiption of WaitNextEvent in 
Chapter 3,  •MuJtiFinder-Aware Applications,• for more information on how your 
application can take advantage of these new programmer-defined parameters). 

Background notification 

Applications running in the background cannot use the standard methods of 
communicating with the user, such as alert or dialog boxes, since such windows 
woUldn't necessarily be visible under the windows of the foreground application. 
Also, in the single-Finder environment, some applications used nonstandard and 
unsupported notification techniques.  Now, under MultiFinder, if something occurs 
that requires the user's immediate attention, applications should use the Notiftcadon 
Manager (see Appendix D) to notify the user. 

It is suggested that your application adopt a three-level notiftcadon hierarchy for 
communicating with the user as a user interface standard: 

1 . A diamond displayed next to the application's name in the Apple menu. 

2. An icon for the application alternating with the Apple icon menu title in the menu 
bar (and any other background-resident application icons that need attention), 
and a diamond displayed next to the application's name in the Apple menu. 
3. Both the application's icon and the diamond are displayed, and an alert box is 
displayed on the frontmost layer to notify the user that something needs to be 
done. 

4 

Chopter 1:  Introduction to  MultlRnder 


---
The user should be allowed to set the desired level of notification in a dialog window 
(for example, PrintMonitor's Preferences dialog window).  The default should be level 
two (display the background application's icon in the menu bar and display a 
diamond next to the application's name in the Apple menu).  Sound can also be used 
for levels two and three; however, the user should have the option of turning it off. 
Sound, alert boxes, and icon use should all be optional.  1he most important idea 
here is that the user should have the final say in how the notification process will work. 

Users should also be able to tum background notification off altogether, except in 
cases where damage would oca.u or data would be lost (for example, a file server going 
down in two minutes).  Background-resident applications should not do anything that 
might affect the foreground application, such as changing the pointer or altering the 
menu bar. 

The three types of Macintosh applications 

Applications generally fit into three categories while running under MultiFinder. 
Well-behaved applications work fine--they just don't take advantage of the new 
expanded functionality.  MultiFinder-aware applications take advantage of some or all 
of MultiFinder's new specific capabiliti~perhaps doing some work in the 
background.  Finally, special-purpose applications perform most or all of their work 
in the background.  While desk accessories are not applications, they represent a 
special case and will be disrussed at the end of Chapter 4,  •special-Purpose 
Applications.• 

(. 

Well-behaved applications 

A well-behaved application is one that generally follows the standard Macintosh 
programming procedures outlined in Instde Macintosh. 

Applications that write directly to the screen, directly modify Window Manager or 
Menu Manager data structures, rely on the contents of low memory, or use other 
shortcuts to save time are not compatible with MultiFinder. 

A well-behaved application regularly makes an event call (GetNextEvent, 
WaitNextEvent, or EventAvail) to provide frequent times when the application may be 
suspended, follows the standard Macintosh notification procedures, and can function 
properly anywhere in memory. 

See Chapter 2 for further information on well-behaved applications. 

The  fhree  types of Macintosh opplicattons 

5 


---
MultlFlnder-aware applications 
A MultiFincler-aware application is one that handles suspend and resume events, 
calls WaitNextEvent, includes a SIZE resource (see Chapter 3 for a detailed description 
of these three new programming features), and uses normally unused null events for 
effective background work, while not significantly affecting the responsive nature of the 
application running in the foreground. 

Faster switching 

· .. 

A MultiFindet-aware application can speed up the switching process by being 
responsible for converting its own private Clipboard when a user clicks on another 
application.  By following the new programming guidelines outlined in Chapter 3, 
"MultiFinder-Aware Applications,• applications can ensure that switching between 
open applications will be even faster. 

A better citizen 

To be considered tNly MultiFinder-aware, applications should call WaitNextEvent to 
allow other applications to use any unused processing time.  Since MultiFinder allows 
many applications to share the available resources, if you don't need them, allow 
someone else the opportunity.  Because certain MultiFinder-unfriendly applications 
may not call WaitNextEvent, applications running in the background cannot be 
guaranteed any microprocessor time. 

Supporting the responsive nature of the foreground application is an important issue 
for applications running in the background  The foreground application, however, is 
not required to provide the same service to other applications.  The idea here is not to 
slow down the responsive nature of the foreground application, but rather to allow 
other applications the opportunity to make use of time that would normally be wasted 

More flexible memory management 

Applications should not depend on running in a particular area of memory and 
should not require large amounts of memory to function properly just because they 
were given control of the entire machine in the single-Finder environment. 

MultiFindet does provide a means for applications to get additional memory; 
however, this memory should only be used for very short-term needs and should be 
returned as soon as possible.  It is not to be used for long-term storage (see Chapter 3, 
•MultiFinder-Aware Applications,• for more information on these temporary 
memory allocation calls). 

6 

Chapter 1:  Introduction to  MultlAnder 


---
Special-purpose applications 

Embedded services, faceless background tasks, and desk accessories (for the purpose 
of this manual) each represent different types of special applications that will run with 
MultiFinder.  See Chapter 4,  •special-Purpose Applications,~ for more information 
on these applications. 

Embedded services 

An embedded service runs only in the background  This type of application is 
normally not visible and interacts heavily with the Notification Manager (see 
Appendix D). 

One example of an embedded service is PrintMonitor-a background printing utility 
supplied with MultiFinder. 

Faceless background tasks 

A faceless background task is almost invisible.  It is minimal in size and has no user 
interface-no icon will appear in the Apple menu, no windows will be displayed, and 
no port exists.  If any user interaction is required, it uses the Notification Manager. 

A faceless background task sets the canBackground and backgroundOnly bits in the 
SIZE resource (see Chapter 3 for more information on the SIZE resource) and can't 
display a user interface. 

A good example of a faceless background task that looks for printer spool files is 
BackGrounder. 

Desk accessories 

MultiFinder has eliminated the unique advantages that gave desk accessories increased 
functionality over applications.  While it's true that MultiFinder continues to support 
the standard desk accessory model, you are encouraged to write small applications in 
the future. 

It's important to note here that desk accessories are now loaded into the system heap 
instead of the application heap (except when the Option key is held down). 
Therefore, desk accessories that rely on being loaded in a specific application's heap 
will not function properly under MultiFinder. 

The  three  types of Macintosh appllcatlons 

7 


---

---
Chapter  2 

Well-Behaved  Applications 

{ 

If you have been following the programming guidelines specified in Inside 
Macintosh, your application will probably work as expected under MultiFinder.  This 
chapter outlines a number of programming guidelines that applications should follow 
to ensure future compaubility with MultiFinder. 

A well-behaved application regularly makes an event call (GetNextEvent, 
WaitNextEvent,  or EventAvail) allowing for frequent suspension times, follows the 
standard Macintosh notification procedures,  can function properly anywhere in 
memory, and follows the other guidelines specified in this chapter. 

Be aware that MultiFinder allows special types of applications: applications that 
perform part of their work in the background while another application is running in 
the foreground, and applications that do all their work in the background.  A well 
behaved application should make event calls to ensure that such applications will be 
able to use any null event processing time. 

Windows, menus, and screens 
Save your window positions; there is nothing more irritating to the Macintosh user who 
sets up a MultiFinder work space on the desktop than to have to reposition the windows 
of applications every time the Macintosh is turned on. 

Applications that lay out their control panels and palettes in separate windows need to 
be careful of •gaps• in the layout  If a user accidentally clicks in one of these gaps, 
another application could be switched to the foreground unintentionally. 

9 


---
The user should maintain control over the initial positioning of free-floating palettes 
(for example, the MacPaintGD 2.0 Command-T option that allows the user to position 
the Tool palette right at the present location of the cursor).  Otherwise, the initial 
position of these palettes should be within a few pixels of the window.  Mini-windows 
and tear-off menus should disappear when an application is switched out and reappear 
when the application returns to the foreground 
+ Note: MultiFinder suspends a well-behaved application (which isn't aware of 

MultiFinder) by creating a situation similiar to that which occurs when a user opem 
a desk accessory; that is, the application receives a deactivate event for its front 
window.  Similarly, MultiFinder causes a well-behaved application to resume by 
·sending the application an activate event 

There have always been a number of suggested •don•ts• connected with data structure 
manipulation-now, under MultiFinder, these suggestiom are no longer optional. 

Don't modify Window Manager data structures directly 
Don't let your application modify Window Manager data structures directly.  1be 
Window Manager owm the Window Manager data structures.  These include all the 
low-memory values defined by the Window Manager, in addition to any of the fields 
(including the grafPort fields) contained in the window record itself.  Because the 
procedural interface to the Window Manager is so effective, direct data structure 
modification is rarely done (one exception is windowKind), but beware nonetheless. 

Because MultiFinder provides a shared environment, it is particularly important to 
avoid circumventing the Wmdow Manager. 
+ Note: Don't modify the visRgn field of the GrafPort in the window record; 

MultiFinder relies on this field 

Don't manipulate Menu Manager data structures directly 

Much of MultiFinder's functionality depends on using the Apple menu in novel ways. 
This includes controlling the menu data structures of the Apple menu.  For this reason, 
items should be enabled and disabled through traps provided for that pwpose; direct 
manipulation of data structures should be avoided completely. 

10 

Chapter 2:  Well-Behoved  Applications 


---
( 

Don't draw on  the desktop 
Your application no longer •owns• the desktop under MultiFinder, so don't draw on 
the desktop.  This means on the menu bar, desktop, or windows that belong to other 
applications.  To remain MultiFinder-compatible with future systems, draw only in 
response to an update event or as part of the feedback for a user action (for example, 
while tracking the mouse). 
+  Note: DeskHook, a low-memory vector that allowed applications to draw on the 

desktop, is no longer called by the Window Manager. 

WMgrPort  and GrayRgn 

WMgrPort has its visRgn set to include all active screens.  Its clipRgn is initially set to 
"wide open• (the rectangle -32767, -32767, 32767, 32767), although Window Manager 
routines like Clip.Above will change it.  Consider this grafPort to be read-only.  The 
global variable GrayRgn is a region that is equal to the WMgrPort's visRgn minus the 
menu bar area. 

You should use GrayRgn to find out the shape, size, and coordinates of the screens. 
You will never have to use the WMgrPort directly, and should not call GetWMgrPort 
under any circumstances. 

Don't write directly to the screen 

Drawing to the screen should only be done within windows via QuickDraw.  Off-screen 
bitmaps should be copied to the screen via CopyBits. 

Don't save the contents of windows 

Don't save the bitmap contents of windows to save time when displaying dialog boxes 
or pop-up menus; the window you save might not be yours and it might change while it 
is being covered up. 

Handle update events 

Remember that update events are very important; applications running under 
MultiFinder must pay close attention to them.  All applications will receive update 
events, not just the application currently running in the foreground.  When an 
application receives an update event, it must update the appropriate window without 
doing anything else. 

Windows.  menus.  and  screens 

11 


---
MultiFinder feeds update events to the application when the application makes an 
event call and continues to feed update events to the application until it actually 
processes them.  Applications should respond (that is, draw) as a direct response to 
receiving the update event. 

In general, if you are using an event call, you should be prepared to receive and 
respond to update events immediately.  Do not defer update processing to a later 
time. 

Use null events properly 

Null events have a different meaning under MultiFmder.  Originally, an application 
would receive a null event when no other event occurred  Under MultiFinder, 
however, a well-behaved application receives null events when it is in the foreground 
and no background task is pending. 

Periodic garbage collection and similar time-consuming actions should not be 
perfonned on every null event received  Use absolute time rather than the number of 
events. 

Support keyboard commands for editing 

Support the appropriate keyboard equivalents for menu editing commands; for 
example,  Undo (Command-Z),  Cut (Command-X),  Copy (Command-C),  and Paste 
(Command-V). 

Memory management 
Applications that do not supply a SIZE resource (see Chapter 3,  •MultiFinder-Aware 
Applications,• for more information on this new resource) are launched into the 
defauk partition sii.e of 384K.  Thus, you will encounter trouble if your application 
requires more than this amount of memory.  You may want to include a SIZE resource 
in your application to inform MultiFinder that you require a particular partition sii.e. 

To ensure that your application remains compatible under MultiFinder, follow these 
memory management guidelines. 

12 

Chapter 2:  Well-Behaved  Appllcatlons 


---
( 

( . •. 

Don't depend on the relative positions of the system  and 
application  heaps 

Don't make assumptions about the memory model concerning the relative positions 
of the various heaps.  The system heap is not necessarily adjacent to the application 
heap. 

Free space and heap size 

Heap size is not as important as the amount of free space.  Use FreeMem, PurgeSpace, 
and MaxBlock to verify how much free space is available. 

The size of your heap is given only by the bkLim field of the heap zone header.  You 
can find this by dereferencing the ApplZone pointer in low memory (or calling the 
ApplicZone in either C or PascaO.  For instance, in C: 

*(unsigned  long*)  ApplicZone () - (unsigned  long) ApplicZone () 

Stack size 

If your application has unusual stack requirements you can check the size of your stack 
by calling Stack:Size. 

If you must resize your stack, call SetAppJLimit immediately before or after initializing 
the various Toolbox managers.  This will indirectly change your stack size. 
SetApplLimit sets the limit of the application's heap size.  Only the original 
application zone can be expanded.  See Inside Macintosh, Volume Il for more 
information. 

The  application heap 

The only permanent memory available to your application is your application heap 
and your stack.  If you need to allocate additional heaps, they must exist within this 
area. 

Memory management 

13 


---
The AS world 
The AS world consists of an application's own global variables and its private set of 
QuickDraw globals (both are accessed through A5), the set of low-memory globals 
associated with the application by MultiFinder, and the application's heap and stack. 
The single-Finder environment only allowed one A5 world at any given time.  With 
MultiFinder active, each open application has an A5 world 

Most applications don't need to worry about their AS world since MultiFinder 
automatically ensures that the application's A5 world is set up whenever the 
application is given processor time.  1bere are, however, circumstances where some 
portion of an application will need to make certain that it is operating in its own A5 
world.  Before the specific details and guidelines for these circumstances are 
described, comider the three possible A5 situations that can occur under MultiFinder: 
•  AS  and low memory both valid-register AS points to the application's globals 
and low memory contains the set of Toolbox and OS globals appropriate for the 
application (including the global CurrentAS).  This is the normal situation when an 
application is running. 

•  AS  invallcl, low memory valid-register A5 points nowhere in particular; 

however, low memory contains the set of Toolbox and OS globals appropriate for 
the application (including the global CurrentAS).  This situation can sometimes 
occur in trap patches where the A5 register is temporarily used to store a value other 
than the A5 world pointer. 

•  AS  and low memory both imallcl-register AS points to another application's 

globals and low memory contains the wrong set of Toolbox and OS globals 
(including the global CurrentA.5).  This situation can occur for routines run at 
interrupt time-such as completion routines, VBL tasks, Time Manager tasks-and 
in application-specific window definition procedures (WDEF's). 

Trap  patches and global data 

If you are patching traps, use the SetrrapAddress calls rather than writing into the 
dispatch table in low memory.  To ensure that MultiFmder will only run your trap 
patches while your application is running in the foreground, place your patch 
receiving routines in your application heap and not in the system heap. 

Remove all your patched traps before exiting from your application.  Under 
MultiFinder, patches are local to your application and no longer exist after the 
application quits.  However, to remain compauble with the single-Finder 
environment, your application must remove them. 

14 

Chapter 2:  Well-Behaved  Appllcotlons 


---
( 

For those traps that cannot be called from interrupt routines (such as calls to 
QuickDraw), you cannot assume that the value of A5 points to your application's 
globals when a trap is made.  This means that if you patch a trap and the code you 
install references your application's global data, you must manually save A5, set it to 
CurrentA5, do your work, and restore the original A5 when you are finished 

CompleHon routines 
Many VO completion routines (for asynchronous I/O) run at interrupt level; this 
implies that any A5 world could be active.  Applications cannot rely on A5 or 
CurrentA5 to contain the correct value when the VO completion routine is called 

Place the value of CurrentA5 that "belongs• to your partition in a place where you can 
find irfrom within your completion routine.  Since it is guaranteed that AO will be 
pointing to your parameter block when your completion routine is called, you can put 
the value of CurrentA5 at a known offset from the beginning of your parameter block 
and then reference it from AO.  1be following section on VBL tasks gives a simple 
example of how to do this. 

VBL tasks 

VBL tasks (tasks performed during the vertical retrace interrupt) in the application 
heap only run if the creating application is frontmost.  VBL tasks in the system heap 
run all the time, and as in the case of interrupt routines, absolutely no A5 world 
context can be guaranteed. 

As with VO completion, the A5 value can be prefixed to the VBL queue element-this 
should be done whether the VBL tasks are in the application or the system heap. 

The following short MPW examples show how to do this using INLIN&.  Please note 
that this technique does not involve writing into your code segmenL  1be value of 
CurrentA5 is placed in a position where the application can find it from within the VBL 
task.  These examples rely on the fact that at the time your VBL task is run, register AO 
points to the VBLTask structure associated with your VBL task.  Since you store your 
CurrentA5 into the 4 bytes before the VBLTask, you can get the correct CurrentA5 
from -4(AO). 

This example also serves to demonstrate how you might write a completion routine for 
an asynchronous Device Manager call. It is not intended to be a complete program, 
nor to demonstrate optimal techniques for displaying information. Jn MPWTM  Pascal: 

PROGRAM  InlineVBL; 

USES 

{$PUSH} 
{$LOAD  PasDump.dump} 

save  compiler  options 
load  symbol  table  dump 

lhe AS world 

15 


---
Memtypes,QuickOraw,OSintf,Toolintf,Packintf,MacPrint,WLW,JimLib; 
{$LOAD} 
{$POP} 
{$D+) 

{  turn  off  LOAD 
{  restore  compiler  options 
{  debug  symbols 

CONST 

Interval 

K  6; 

CurrentAS 

$904; 

TYPE 

MyVBLType  -

RECORD 

how  often  you  want  your  VBL 

called,  in  ticks 
low-memory  global 

CurAS:  Longint; 
MyVBL:  VBLTask; 

put  CurAS  where  you  can  find  it} 
the  actual  VBLTask  } 

END;  {MyVBLType} 

VAR 

Err 
MyVBLRec 
Counter 
MyEvent 

Integer; 
MyVBLType; 
Integer; 
EventRecord; 

PROCEDURE  _Datainit; 

EXTERNAL; 

PROCEDURE  PushAS; 
INLINE  $2FOD; 

PROCEDURE  PopAS; 
INLINE  $2ASF; 

{  MOVE.L  AS,-(SP)}  {push  AS  onto  the  stack} 

{  MOVE. L  (SP)+, AS}  {pop  the  stack  into  AS} 

PROCEDURE  GetMyAS; 

INLINE  $2A68,$FFFC; 

{  MOVE.L  -4(A0),AS 

Get  the  value  of  AS  you've  stored  before  the  parameter  block  and) 
) 
put  it  in  AS.  Since  you  know  that  when  a  VBL  task  is  called,  AO 
will  point  to  your  parameter  block,  you  also  know  that  the  value l 
of  CurrentAS  that  you  stored  will  be  at  -4 (AO)  • l 

PROCEDURE  DoVBL; 

{  your  VBL  task  } 

BEGIN 

DoVBL  } 

First,  make  sure  you  have  the  AS  that  you  stored  before  your 
parameter  block. 

) 

PushAS; 
GetMyAS; 

push  the  value  of  AS  onto  the  stack 
get  your  AS  from  right  before  the 

parameter  block 

now  you  can  access  your  globals: 

16 

Chapter 2:  Well-Behaved  Appllc:atlons 


---
( 

MyVBLRec.MyVBL.vblCount  ·- Interval; 
Counter  ·=  Counter  +  l; 

run  again  to  show  that  you  can 

set  a  global 

PopAS; 
END;  ( DoVBL} 

put  back  the  original  AS 

!------------------------------Main  Program-------------------------------} 

BEGIN 

(main  PROGRAM} 

MaxApplZone; 
UnloadSeg (@_Datainit); 
InitMac; 
InitWW (NIL) ; 

(  grow  the  heap  to  ApplLimit 
(  unload  data  init  code  before  any  allocations 
(  initialize  Macintosh  managers 
(  initialize  WritelnWindow  with  default  window 

Counter  :=  O; 

initialize  this 

WITH  MyVBLRec,MyVBL  DO  BEGIN 

CurAS 
vblAddr 
vblCount 

:- LongPtr(CurrentAS)"; 
·=  @DoVBL; 
• •  Interval; 

get  current  value  of  CurrentAS 
point  to  your  task 
set  up  the  interval  where  you' 11 

qType 
vblPhase 

: =  ORD (VType) ; 
·=  O; 

END; 

(With} 

be  called 

this  is  also  necessary 

·=  VInstall(@MyVBLRec.MyVBL); 

Err 
writeln ( 'VInstall  err  =  ',Err); 

(  install  your  VBLTask  } 

REPEAT 

writeln(Counter); 

UNTIL  GetNextEvent(mDownMask,MyEvent); 

write  out  counter 
this  allows  a  switch 

:=  VRemove (@MyVBLRec.MyVBL); 

Err 
writeln ( 'VRemove  err  • 

',Err); 

beep; 

END. 

you're  finished,  remove  the  task 

(  show  the  user  you•re  finished 

· Now for the MPW C example-first, the assembly routines: 

CASE  ON 

;  for  C 

PushAS PROC 

EXPORT; 

MOVE. L  (SP) +,Al 
MOVE. L AS, - (SP) 
(Al) 
JMP 

ENDP 

pushes  AS  onto·ti.e---tack  -- BE  CAREFUL  NOT  TO 
DISTURB  AO  here,  since  GetMyAS  relies  on  it 
get  return  address  off  the  stack 
push  AS 
return  to  caller 

The AS wortd 

17 


---
EXPORT 

PopAS  PROC 
MOVE.  L  (SP) +,Al 
MOVE.L  (SP)+,AS 
JMP  (All 

ENDP 

get  return  address  off  the  stack 

;  pop  into  AS 

return  to  caller 

EXPORT 

GetMyAS  PROC 
MOVE.L  (SP)+,Al 
MOVE.  L -4 (AO)  ,AS 
JMP 

(Al) 

get  return  address  off  the  stack 

;  get  saved  value  of  AS  and  put  it  in  AS 

return  to  caller 

ENDP 

END 

Now the MPW C program: 

tinclude  <types.h> 
tinclude  <quickdraw.h> 
tinclude  <resources. h> 
tinclude  <fonts.h> 
t include  <windows. h> 
tinclude  <menus.h> 
tinclude  <textedit.h> 
tinclude  <events.h> 
tinclude  <retrace.h> 
tinclude  <packaqes.h> 

extern  void  PushA5 ()  ; 
extern  void  PopA5 (); 
extern  void  GetMyA5 ()  ; 

I*  MOVE.L  A5,-(SP) 
*/ 
I*  MOVE.L 
(SP) +,A5  */ 
/*  MOVE.L  -4(AO),A5  */ 

/*  push  A5  onto  the  stack  */ 
I*  pop  the  stack  into  A5 
*/ 

I*  Get  the  value  of  A5  you've  stored  before  the  parameter  block  and  put  it  in  *I 
/*  A5. 
*/ 
I*  parameter  block,  you  also  know  that  the  value  of  CurrentA5  that  you  stored */ 
*I 
/*  will  be  at  -4(A0). 

Since  you  know  that  when  a  VBL  task  is  called,  AO  will  point  to  your 

void  DoVBL () ; 

typedef  struct  MyVBLType 

long 
VBLTask 

CurA5; 
MyVBL; 

MyVBLType; 

MyVBLType  MyVBLRec; 
short  Counter; 

/*  put  CurA5  where  you  can  find  it 
/*  the  actual  VBLTask 

*/ 
*/ 

/*  a  variable  of  the  above  type 
/*  this  needs  to  be  global  so  the 
/* 
VBL  task  can  get  to  it 

*I 
*/ 
*I 

18 

Chapter 2:  Well-Behaved  Applications 


---
( 

main() 
{ 

#define  Interval 

6 

#define  CurrentAS 

Ox904 

/*  how  often  you  want  your  VBL  called,  *I 
I* 
in  ticks 
*/ 
/*  low-memory  qlobal 
*/ 

WindowPtr 
Rect 
OSErr 
EventRecord  MyEvent; 
char 

MyWindow; 
myWRect,rectToErase; 
err; 

my5tr[40]; 

/*  this  should  be  enouqh  room  */ 

InitGraf(&qd.thePort); 
InitFonts (); 
FlushEvents(everyEvent,  0); 
InitWindows(); 
InitMenus (); 
TEinit () ;. 

SetRect(&myWRect,50,260,150,340); 
MyWindow  =  NewWindow (nil, &myWRect, "\pVBL", true, O, 

(WindowPtr) -1, false, 0); 

SetPort(MyWindow); 

counter  =  O; 

MyVBLRec.CurA5  =  * (lonq  *)  (CurrentA5); 

MyVBLRec. MyVBL. vblAddr  - DoVBL; 
MyVBLRec.MyVBL.vblCount 

Interval; 

/*  initialize  this 

/*  qet  the  current  value 
of  CurrentA5 
/* 
/*  point  to  your  task 

*/ 

*I 
*I 
*/ 

/*  set  up  the  interval  at  which  you• ll  be  called  *I 

MyVBLRec.MyVBL.qType  =  vType; 
MyVBLRec. MyVBL. vblPhase  - 0; 

/*  this  is  also  necessary  */ 

err  •  VInstall ( &MyVBLRec. MyVBL)  ; 

/*  install  your  VBLTask 

PenMode(patXor); 

SetRect(&rectToErase,60,20,100,50); 
MoveTo(l0,76); 
Drawstrinq("\pClick  to  quit•); 

/*  so  you  can  see  the 
I* 

drawinq  flicker 

while 
{ 

(!GetNextEvent(mDownMask,&MyEvent)) 

/*  this  allows  a  switch 

MoveTo(20,20); 

/*  draw  a  box 

*/ 

*/ 
*/ 

*I 

*I 

LineTo(20,50);LineTo(50,50);LineTo(50,20); 
LineTo(20,20);LineTo(50,50); 
MoveTo(20,50) ;LineTo(S0,20·1 ;MoveTo(60,43); 

EraseRect(&rectToErase); 

/*  erase  the  last  number 

*/ 

(~\ 

"'/ 

lhe AS  wortd 

19 


---
NumToString(Counter,myStr); 
DrawString(myStr); 

/*  draw  the  current  value  *I 
*I 
/* 

of  Counter 

err  =  VRemove (&MyVBLRec.MyVBL); 

/*  you' re  finished, 
I* 

task 

remove  */ 
*/ 

if  (err  !=  noErr)  debugger(); 
/*  wait  around  until  the  user  clicks  before  exiting  */ 

while  (!Button()); 
while  (Button () ) ; 

/*main*/ 

void  DoVBL () 
{ 

/*  your  VBL  task 
I*  DoVBL 

/*  First,  make  sure  you  have  the  AS  that  you  stored  before  your 
/* 

parameter  block. 

*/ 

*I 
*I 

*I 

PushAS(); 

GetMyAS(); 

onto  the  stack 

/*  push  the  value  of  AS 
*/ 
*I 
/* 
/*  get  your  AS  from  right  */ 
*I 
/*  before  the  parameter 
*/ 
/*  block 

/*  now  you  can  access  your  globals:  */ 

MyVBLRec. MyVBL. vblCount  • 

Interval; 

/*  to  run  again 

*/ 

Counter  +=  1; 

PopAS (); 

I*  END  DoVBL*/ 

Time Manager tasks 

*I 
/*  to  show  you  can  set  a 
/*  global 
*/ 
/*  return  the  original  AS  */ 

Again, no A5 world context is guaranteed; however, unlike VBL tasks (and completion 
routines), a Time Manager task is not called with AO pointing to the task block (AO 
points to the task's routine instead). So, if you need to get at your application's globals 
from your Time Manager task, you'll have to write the value of CurrentA5 into your 
code segment at a time when you know that CurrentA.5 is valid, and then use that value 
to set up A5 when your Tune Manager task is called. 

There may be some circumstances when your application will have to change the value 
in AS; just make sure that you restore A5's previous value when you are finished. 

20 

Chapter 2:  Well-Behoved  Applications 


---
( 

( · .. 

.. 

Defprocs 

Since window defprocs are used by the layer Manager (a new manager called only by 
the Window Manager), the A5 world present when the defproc is called might belong 
to any application.  For example, a window frame may need to be redrawn while 
another application is running in the foreground (the foreground application has a 
window in front of another application's window and the frontmost window is moved). 
In this case, the Window Manager calls the WDEF to draw the frame and posts an 
update event for the application that owns the window to redraw the window contents. 
However, if the WDEF is part of your application, it may be called to draw a window 
frame while another application is active.  Suddenly, A5 does not point to your 
application's globals. 

Therefore, window defprocs cannot depend on A5 being valid.  If your application 
instafls a window defproc that neech to access global variables from your application, 
store a copy of your A5 safely by using the technique described in the previous section 
(VBL tasks) or place it in the refCon (reserved) field of the window. 

Miscellaneous guidelines 
Someday, the Macintosh will expect applications to run in the 68o:XO user mode (as 
opposed to today's supervisor mode), so in preparation for that day avoid using any 
of the 68oXO privileged instructions.  Also avoid making 68o:XO TRAP or TRAPV calls . 

All types of utilities, as well as applications, need to be aware of MultiFinder's shared 
environment  For example, screen savers should make sure that background 
processing continues. 

Remember that applications should avoid direct manipulation of the Apple menu. 

low memory 

The less your application accesses low memory, the better.  Writing to low memory, 
however, is much more objectionable than reading, and should be avoided.  In the 
long run, low memory will disappear, so try not to depend on it 

Interact with the global scrap by using the Scrap Manager whenever possible; avoid 
direct manipulation of the low-memory Scrap Manager data structures or the 
Clipboard file itself. 

Try not to use the low-memory notification procedures (for example: IAZNotify, 
EjectNotify, DeskHook, and so on) unless absolutely necessary. 

Miscellaneous  guldellnes 

21 


---
Asynchronous system calls 

MultiFinder will wait until all currently active fde system requests are completed before 
it brings another application to the foreground.  This means that during any pending 
asynchronous fde system request, MultiFinder will not allow activation of a different 
application. 
+ Note: This is not the last word on this issue.  future releases of MultiFinder will 

examine the compatibility of switching while asynchronous Fde Manager calls are 
still pending.  Currently, in MultiFinder, Device Manager calls do not delay 
application switching. 

Exit time restrictions 

Do not assume that at exit time you can clear the screen to save code. 

Do not destroy fields in system data structures, such as the window list, before you exit. 

Remember to e:x:tt gracefidly-clean up, call ExitToShell (don't assume that you can 
do anything you want before you call ExitToShell; even though your application is 
exiting, there may be other applications running), don't call InitWmdows again, and 
soon. 

System resources 

As stated earlier, resources from the System fde that were formerly loaded into the 
application heap are now loaded into the system heap for use by all applications.  If a 
resource came from the System fde, it will be loaded into the system heap even if the 
resSysHeap bit isn't seL 

Your applications should not make assumptions about where resources other than 
those in your own resource files have been loaded (the system or application heap). 
The best way to get your own copy of a system resource is to use HandToHand rather 
than DetachResource. 

Since other applications may need to use system resources, applications should not 
call ReleaseResource or DetachResource for system resources such as pointers and 
fonts-nor should they change resource attributes or modify the resource data 
directly. 

22 

Chapter 2:  Well-Behaved  Appllcotlons 


---
(" 

Chapter 3 

MultlFinder-Aware 
Applications 

A MultJFincler-aware application is one that handles suspend and resume events, 
includes a SIZE resource, calls WaitNextEvent, and uses normally unused null events 
for effective background work, while not significantly affecting the responsive nature of 
the application currently running in the foreground.  This chapter will desaibe each 
of these important aspects of the MultiFtnder-aware application. 

When an application stops executing in the background and begins running in the 
foreground,  it has all the rights and responsibilities of any foreground application. 
These include being a good citizen while running in the foreground by calling 
WaitNextEvent (see information on this new event call later in this chapter) to allow 
other applications the opportunity to perform some work while running in the 
background. 

After an application begins running in the foreground, it can receive user events and 
use any Toolbox service (File Manager, QuickDraw, Window Manager, and so on). 
Keep in mind that the same application executing in the background can also use any 
Toolbox or OS service, but won't see user events until it begins running in the 
foreground 

When a user attempts to bring a second application to the foreground, the Event 
Manager checks to see if the applications involved can handle suspend/resume 
events.  Here, the user is trying to switch between two layers.  If your application 
doesn't handle suspend/resume events, the operation of the Macintosh will appear 
sluggish. 

23 


---
Suspend and resume events 
Two new types of app4Evts (type 4 application events) have been aeated within the 
Event Manager: suspend and resume.  Their primary function is an optimization to 
tell the application when it should process the saap.  A secondary function is to tell 
the application whether it is in the foreground or background. 
+ Programming ttp: It's a good idea to have a variable (for example, InForeground, 
initialized to TRUE) that keeps track of whether the application is in the foreground 
or background.  When your application is launched, asNme that you are in the 
foreground.  If you receive a suspend event, you're going to the background; if you 
receive a resume event, you're going to the foreground. 

If you intend to have your application perform work in the foreground and the 
background, you need to process these two events. 

Table 3-1 lists the meanings of the bits in the suspend/resume event message field. 

Table 3·1 
The suspend/resume message fteld 

Bit 

Meaning 

0 

1 

0 =suspend event 
1 = resume event 

0  ==  Clipboard conversion not required on resume 
1 •  Clipboard conversion required on resume 

2-23 
24-31 

reserved 
high-byte value of $01 indicates a suspend or resume event 
high-byte value of $FA indicates a mouse-moved event 

Handling activate and deactivate 
Suspend/resume events have to be handled whenever you get them; usually, this 
happens in the main event loop.  By supporting suspend/resume events, the 
application is taking responsibility for activating or deactivating its front window at 
suspend/resume time. 

24 

Chapter 3:  MultlFlnder-Aware Appllco11ons 


---
( 

If the multiFinderAware bit has been set in the application's SIZE resource (see the 
following seaion for more information on this new resource), then the application 
must take responsibility for performing a deaaivate after receiving a suspend event 
and an activate after receiving a resume event 

Take care If masking out app4Evts 

Suspend/resume events are not queued, so be careful when masking out app4Evts. 
You will still get switched out; however, all that will happen if you mask out app4Evts is 
that your application won't know when it is going to be switched out (your application 
will still be switched out when you call WaitNextEvenr).  If your application sets a 
boolean to tell whether or not it's in the foreground or the background, you definitely 
don't want to mask out app4Evts. 

A C example of how to handle suspend and resume events 

If an application doesn't support the suspend/resume events, MultiFinder has to trick 
the application into performing scrap coercion to ensure that the contents of the 
Clipboard can be transferred from one application to another.  This process adds to 
the time it takes to move the foreground application to the background and vice versa 
and makes the user interface appear cumbersome. 

An application responds to a suspend event by moving its private scrap into the 
Clipboard and then returning to the main event loop.  When the application receives 
a resume event, and if the Clipboard has been altered, the application copies the 
Clipboard and converts it back to its private saap.  After this transformation, the 
application resumes executing. 
+ Note: Applications should hide their Clipboard window when not running in the 
foreground.  The contents of the Clipboard window are not valid unless the 
application is frontmosl 

MultiFinder sets bit 1 of the EventRecord of resume events if the scrap has changed 
while the application was suspended. 

Here is a C example of how to handle the suspend/resume events (this code example 
also uses the new MultiFinder call WaltNextEvea.t; see the seaion on this call 
presented later in this chapter): 

/*  --- Useful  macros  for  determining- specifics  of  suspend/resume  events  ----*I 

tdefine  App4Selector (eventPtr) 
/*  top  byte  of  messaqe  field  is  the  selector  */ 

(* ( (unsiqned  char  *I 

'(eventPtr)->messaqe) I 

tdefine  SOSPEND_RESOME_SELECTOR 

OxOl 

SUspend  and resume  events 

25 


---
I*  selector  of  this  value  is  suspend/resume  */ 

#define  SuspResisResume(evtMessaqe) 
/*  low  bit  on  signifies  resume  */ 

( (evtMessaqe)  &  OxOOOOOOOl) 

#define  SuspResisSuspend(evtMessaqe) 
/*  low  bit  off  signifies  suspend  */ 

(!SuspResisResume(evtMessage)) 

#define  ScrapDataHasChanged(e.vtMessage) 
/*  only  valid  for  suspend/resume  messaqes  */ 

( (evtMessage)  &  Ox00000002) 

I*  ----------------------

Necessary  qlobal  variables 

----------------------- */ 

Boolean 

wneisimplemented; 

/*  Is  _WaitNextEvent  implemented? 

*/ 

Boolean 

inForeground; 

. /*  Is  this  application  in  the  foreground  under  *I 
*I 

/*  MultiFinder? 

WindowPtr 

clipboardWindow; 

/*  This  is  a  pointer  to  the  Clipboard  window,  */ 
/*  which  is  only  made  invisible  when  the  user  *I 
*I 
I*  closes  it.  The  next  time  it  is  opened 
*I 
/*  make  it  visible  aqain  to  speed  up  the 
I*  process. 
*/ 

/*  --------------------------

The  main  event  loop 

-------------------------- *I 

/*  pick  the  biggest  possible  timeout  for  _WaitNextEvent  */ 

#define  BIG_TIMEOUT 

OxFFFFFFFF 

void 
EventLoop () 
{ 

Event Record 
void 
void 
void 
void 
void 

my Event; 
HandleMouseDownEvent(EventRecord  *pEvent); 
HandleKeyDownEvent(EventRecord  *pEvent); 
HandleUpdateEvent(EventRecord  *pEvent); 
HandleActivateEvent(EventRecord  *pEvent); 
HandleApp4Event(EventRecord  *pEvent); 

for  (;;) 
{ 
/*  check  the  followinq  each  time  throuqh  loop  */ 

/*  standard  method  for  looping  forever  */ 

CheckClipboardWindow(); 

if.  (wneisimplemented) 
{ 

/*  qet  an  event  *I 

if  ( !WaitNextEvent(everyEvent,  &myEvent,  BIG_TIMEOUT,  nil)) 

continue; 

else 
{ 

/*  keep  loopinq  until  you  qet  a  valid 
/* 

event 

*/ 
*/ 

SystemTask(); 

/*  the  system  will  call  this  itself  if  */ 

26 

Chapter 3:  Multlflnder-Awore Applications 


---
if  (!GetNextEvent(everyEvent, 

continue; 

I* 
'myEvent)) 

_WaitNextEvent  is  used 

*I 

switch  (myEvent.what) 
{ 

case  mouseDown: 

HandleMouseOownEvent(,myEvent); 
break; 

case  keyDown: 

HandleKeyDownEvent(,myEvent); 
break; 
case  updateEvt: 

HandleUpdateEvent(,myEvent); 
break; 

case  activateEvt: 

HandleActivateEvent(,myEvent); 
break; 

case  app4Evt: 

HandleApp4Event(,myEvent); 
break; 

default: 

break; 

/*  ----------------------------ConvertPrivateScrapToDesk------------------------- */ 
void 
ConvertPrivatescrapToDesk() 

/*  If  the  application  uses  a  private  scrap  for  the  Clipboard  contents,  then  this*/ 
/*  is  the  place  to  make  it  public.  This  would  normally  be  called  after 
*/ 
/*  receiving  a  suspend  event  (so  that  it  gets  to  a  location  from  which  it  can  be*/ 
/*  sent  to  other  applications),  when  the  application  quits,  or  when  a  desk 
*I 
/*  accessory  is  activated.*/ 

{ 
} 

/*  ---------------------------convertDeskScrapToPrivate-------------------------- */ 
void 
ConvertDeskScrapToPrivate() 

/*  The  complement  to  ConvertPrivateScrapToDesk (),  this  converts  the  public 
/*  (desk)  scrap  to  the  application's  private  scrap,  if  it  exists  (one  example 
/*  is  the  textedit  scrap).  This  would  normally  be  called  after  receiving  a 
/*  resume  event,  when  the  application  starts  up,  or  when  a  desk  accessory  is 
/*  deactivated.  */ 

*/ 
*/ 
*/ 
*( 

{ 
} 

/*  ------------------------------HandleApp4Event--------------------------------- */ 
void 

SUspend and resune events 

27 

( \ 

/ 


---
HandleApp4Event(pEvent) 
Event Record 

*pEvent; 

/*  Handle  the  app4Evt  (as  determined  by  pEvent->what  -=  15)  if  and  only  if  it• s  * / 
I*  a  suspend/resume  event. 
* / 

I*  NOTE: 
/*  application's  SIZE  resource. 

This  code  only  applies  if  the  multiFinderAware  flag  is  set  in  the 

*I 
* / 

void 
void 
.void 
void 

MyDeactivateWindow(WindowPtr  pWindow); 
MyActivateWindow(WindowPtr  pWindow); 
HideClipboard(); 
ShowClipboard(); 

if  (App4Selector (pEvent)  ==  SUSPEND_RESUME_SELECTOR) 
/*  if  it's  not  suspend/resume  then  ignore  it  */ 
{ 

register  WindowPtr  frontWindow  =  FrontWindow (); 
static  Boolean  clipboardVisinFG; 

/*  If  visible  in  the  foreground,  you  have  to  hide  it  before  going  to 
/*  the  background,  but  then  show  it  later.  This  is  important  because  the 
/*  Clipboard  contents  are  not  valid  unless  the  application  is  in  the 
/*  foreground  (i.e.,  in  the  frontmost  layer).  *I 

*I 
*I 
*I 

/*  It's  either  suspend  or  resume,  based  on  the  low  bit  of  the  message  field.*/ 
/*  You  have  to  treat  suspend  as  a  deactivate  on  the  front  window  and  resume  *I 
*I 
/*  as  an  activate  on  it  because  you  have  the  multiFinderAware  flag  set 
/*  in  the  SIZE  resource. 
*/ 

if  (SuspResisSuspend(pEvent->message)) 
{ 

/*  --------------------------
inForeground  =  false; 
/*  suspend  event  signifies  you  are  moving  to  the  background  */ 

Suspend  Event 

--------------------------- *I 

if  (ScrapDataHasChanged(pEvent->message)) 
/*  on  a  suspend,  this  signifies  whether  the  user  has  changed  the  Clipboard  *I 

ConvertPrivateScrapToDesk(); 
if  (frontWindow  !•  nil) 

MyDeactivateWindow(frontWindow); 
/*  treat  the  suspend  event  as  you  would  a  deactivate  event  */ 

if  (clipboardWindow  !•  nil  ' '  

((WindowPeek)clipboardWindow)->visible) 

HideClipboard () ; 
clipboardVisinFG  • 

/*  hide  the  Clipboard  when  you' re  in  the  background  *I 

true; 

else 

clipboardVisinFG  =  false; 

28 

Chapter 3:  MultlFlnder-Aware  Applications 


---
( 

else 

/*  ----------------------------

Resume  Event 

---------------------------*I 

inForeground  =  true; 
/* 

resume  event  signifies  that  you  are  returning  to  the  foreground 

*/ 

if  (ScrapDataHasChanged(pEvent->message)) 

/*  if  new  scrap,  then 
*/ 
/*  reset  your  private  one  */ 

ConvertDeskScrapToPrivate(); 

if  (frontWindow 

!=  nil) 

MyActivateWindow(frontwindow); 

/*  have  to  treat  the  resume  event  as  you  would  an  activate  event*/ 
if  (clipboardVisinFG) 
ShowClipboard(); 

The SIZE resource 
The SIZE resource (see Table 3-2) is used to communicate information from the 
application to MultiFinder.  You are responsible for creating and maintaining the 
information for this  resource. 

When an application is launched under MultiFinder, it is placed into a  memory 
partition that cannot change in size.  It is the application's responsibility to inform 
MultiFinder just how large a memory partition it will require. 

The SIZE resource consists of a 16-bit flap field, used to communicate to MultiFinder 
the level of responsibility an application will handle, directly followed by a 32-bit 
minimum size field and a 32-bit preferred size field,  which indicate the minimum 
and preferred sizes the application will operate within.  The minimum sire is the actual 
limit below which your application will not run.  The preferred sire is the memory sire 
at which your application can run effectively. 

Table 3-2 
The  SIZE  resource 

Bit 

0-8 

9 
10 

11 

Meaning 

reserved 

getFrontClicks 

only Background 

multiFinderAware 

The  SIZE  resource 

29 


---
12 
13 
14 
15 

canBackground 

reserved 

acceptSuspendResumeEvents 

reserved 

16-48 
49-81 

preferred si7.e 

minimum si7.e 

The SIZE  resource flags 

Here are the SIZE resource ~: 
•  acceptSuspendResumeEvents-when set, this bit signifies that the application 
knows how to process suspend/resume events.  When true, MultiFinder notifies the 
application before making it inactive and after reactivating it  In this way, the 
application knows when to process the global saap. 

Failure to support this optimization requires MultiFinder to trick the application 
into performing saap coercion to ensure that the contents of the Clipboard can be 
transferred from one application to another.  'Ibis process adds to the time it takes 
to move the foreground application to the background and vice versa.  MultiFinder 
will also create a false window to cause the foreground application's window to be 
deactivated unless the multiFinderA ware bit is set. 
Whenever an application calls one of the event calls, MultiFinder can return a 
suspend event  After receiving a suspend event, an application does not actually 
become inactive until the next event call.  At this time, the application should 
convert any local scrap into the global scrap and hide mini-windows, selectiom, 
and soon. 

When control returns to the application, MultiFinder returns a  resume event.  1be 
application may now convert the global saap back into its own private scrap, if 
necessary.  As part of the resume event, MultiFinder also lets the application know if 
the Clipboard has changed since the application was suspended by setting bit 1 of 
the message field of the EventRecord of resume events. 
+ Programming tip: If you set the acc:eptSuspendResumeEvents bit, set the 

multiFinderAware bit as well. 

•  canBackgrouncl-when set, this bit means that the application wants to receive 
null events while in the background.  If your application has nothing to do while in 
the background, don't set this bit 

30 

Chapter 3:  MultlRnder-Aware AppUcattons 


---
( 

•  multiFinderAware-when set, this bit means that an application takes 

responsibility for activating and deactivating any windows in response to a 
suspend/resume event.  This means that if the application was suspended and the 
acceptSuspend.ResumeEvents flag was set and the multiFinderAware flag was not set, 
then the application would still receive an activate event.  If you set the 
multiFinderAware flag,  the application won't receive activate events--you must take 
care of activation and deactivation yourself when you receive the corresponding 
suspend or resume event. 

Because you have taken responsibility for deactivation, if the application's window 
is on top, the suspend event should also be treated as though a deactivate event were 
received as well (if both the multiFinderAware and acceptSuspend.ResumeEvents 
fla~ were set).  For example, scroll bars should be inactivated, blinking insertion 
points should be hidden, selected text should be deselected if your application 
moves to the background, and so forth.  If you don't set this bit, MultiFinder has to 
create a window to force the activate/deactivate events to occur. 
+ Remember: Your application cannot take full advantage of the speed 
increases obtained from the suspend/resume events unless you set the 
multiFinderAware bit. 

•  onlyBackground-set this flag if your application does not have a user interface 

and will not run in the foreground 

•  getFrontClicks-set this flag if you want to receive the mouse-down and mouse-up 
events used to bring your application to the foreground when the user clicks in one 
of your application's windows while it is suspended  Ordinarily, the mouse-down 
and mouse-up events that trigger such a switch are not sent to the application. 

Preferred  memory size 

The preferred size is an amount of memory in which an application will run effectively, 
and which MultiFinder will attempt to secure upon launch of the application.  If this 
amount of memory is unavailable, the application is placed into the largest contiguous 
block available providing that it is larger than the specified minimum size.  Users can 
modify the preferred size through the Finder's Get Info window. 
+ Note: If the amount of available memory is between the minimum and preferred 
sizes, MultiFinder will display a dialog box asking if the user wants to run the 
application. 

Minimum memory size 

The minimum size is an actual limit below which the application will not run.  The only 
way users can see the minimum size is if they try to create a partition smaller than the 
minimum size or open the Finder's Get Info window. 

The  SIZE  resource 

31 


---
How to create your own SIZE  resource 

There is no simple formula for determining the appropriate size requirement for all 
applications.  Since there are so many factors that affect memory requirements, only 
general guidelines are applicable. 

An application's memory requirement depends on a number of factors-the static 
heap size, dynamic heap, A5 world, and the stack.  The static heap size includes 
objects that are always present during the course of execution of the application (these 
could be code segments, Toolbox data structures for window records, and so on). 
Dynamic heap requirements come from various heap objects created on a  per 
document basis (which may vary in size proportionately with the document itself) and 
objects that are required for specific commands or functions.  The size of the A5 world 
depends on the amount of global data and intersegment jumps the application 
contains.  The stack contains variables, return addresses, and temporary information. 

How much memory will an application require?  Ma.csbug and its heap-exploring 
commands can be helpful in empirically determining the application's appetite for 
memory.  Checking to see what resides in the application's heap at key times while 
performing all the application's functions would be quite worthwhile. 

The preferred size should be chosen to allow the application to perform almost all of 
its functions without problems.  On the other hand, the application shouldn't be too 
greedy.  Remember that in the MultiFinder environment, multiple applications are 
sharing the machine. 

The minimum size should be chosen such that the application would never cause a 
system error if required to run within that amount of memory. 

The SIZE resource, specified by the application, is of type 'SIZE' with ID (-1).  This will 
tell MultiFinder what you suggest as the preferred and minimum sizes.  The user has the 
option of changing the application's preferred size, but not below the minimum size. 
Rather than lose the original preferred size, a second SIZE resource (SIZE, 0) is created 
to show the user's specified preferred size.  When MultiFinder prepares to launch an 
application, it first checks the (SIZE,  0) resource.  If this doesn't exist, MultiFinder 
then looks for the (SIZE, -1) resource. 

Applications should not modify the preferred or minimum memory requirements of 
the SIZE resource; however, if this is absolutely necessary, you must change both the 
(SIZE, -1) and the (SIZE, 0) resources to affect the attributes mentioned above. 

How can I tell If my application Is running In the background? 

An application can tell if it is running in the background if it has received a suspend 
event but not the corresponding resume event. 

32 

Chapter 3:  MultlFinder-Aware Applications 


---
To run in the background under MultiFinder, an application must have set the 
canBackground bit (bit 12 of the FLAGS word) in the SIZE resource.  In addition, the 
acceptSuspendResumeEvents bit (bit 1-0 should be set. 

Null events 

As stated in Chapter 2,  null events have a different meaning under MultiFinder.  A 
MultiFinder-aware application receives null events when it is in the foreground and no 
background task is pending, or if the application is in the background and the 
canBackground bit is set in the SIZE resource. 

Also  remember that periodic garbage collection and similar time-consuming actions 
shoul~ not be performed on every null event received 

WaitNextEvent 
In MultiFinder, there is a new Event Manager call-WaitNext:Event-that will allow 
the system to run more efficiently.  There are two important differences between 
WaitNextEvent and GetNextEvent.  WaitNextEvent allows the caller to specify, in 
addition to an event record and mask, a time (sleep) during which the application 
relinquishes the processor if no events are pending; and it also allows the caller to 
specify a region (mouseRgn) from which control will not be returned until the mouse is 
moved outside that region. 
+ Note: GetNextEvent is equivalent to WaitNextEvent with a sleep value of 0 and a 

mouseRgn value of 0.  Also, an application will now receive suspend/resume events 
when calling GetNextEvent 

The interface for WaitNextEvent is: 

Function  WaitNextEvent 

(eventMask 

VAR  theEvent 
sleep 
mouseRqn 

INTEGER; 
EventRecord; 
Lonqint; 
RqnHandle  ) 

tick  units 

:  BOOLEAN; 

The  mouseRgn  parameter 

By taking advantage of the •automatic• mouse-tracking feature (mouseRgn) of 
WaitNextEvent,  you can considerably simplify the application's cursor tracking.  The 
application will receive a mouse-moved event only when the mouse strays outside the 
specified region. 

WaltNextEvent 

33 


---
The application can compute the region where the pointer shape should remain the 
same.  When the mouse moves outside this region, the application receives the 
mouse-moved event and can change the pointer, recompute the new region for this 
pointer, and call WaitNextEvent again.  The region is given in global coordinates.  If 
you pass an empty region or a nil region handle (0), mouse-moved events are not 
generated 

The  sleep parameter 

The sleep parameter (specified in ticks) allows an application to •sleep• until an event 
occurs or the specified time has elapsed  Passing a 0 in the sleep parameter for 
WaitNextEvent means that yow application wants to regain control as soon as 
possible.  This will still yield a minimal amount of time to other applications. 

An application running in the background will not receive null events~ the 
canBackground bit is set in its SIZE resowce.  If an application needs to perform some 
work in the background, you can specify how often it nee<k to receive null events by 
adjusting the sleep parameter (for example, if yow application only needs to receive 
one null event per second, set the sleep parameter to 6o). 
+ Programming tip: Currently, MultiFinder will not suspend yow application when 
the frontmost window is a modal dialog box with a window of type dboxProc--so if 
you want your application to perform work while in the background, don't display a 
dbox:Proc window. 

Vle~ding time gracefully 

In general, you should use WaitNextEvent instead of GetNextEvent.  Any foreground 
application that uses WaitNextEvent with the appropriate sleep and mouseRgn values 
will give the maximum amount of time to any applications running in the background. 
Each application running in the background should also use WaitNextEvent as a means 
to sleep between succeMive invocations. 

Because MultiFinder doesn't support preemptive scheduling, any application running 
in the background must call WaitNextEvent at regular intervals to retain the responsive 
nature of the application currently operating in the foreground  Poor response time is 
a sign that your application is not calling WaitNextEvent often enough while running in 
the background 

When an application using background time has control, user events destined for the 
frontmost application will not be handled until the application running in the 
background calls WaitNextEvent. 

34 

Chapter 3:  Multlflnder-Awore Applications 


---
Using unused null event time 

Only a user can activate an application to run in the foreground  Each time any 
application is scheduled, it runs until it makes an event call.  MultiFinder schedules an 
application ready to perform work in the background when the application running in 
the foreground has no rurrent events pending and no window updates are needed  As 
long as the application running in the background periodically calls WaitNextEvent, 
the foreground application will continue to get null events at regular intervals so that 
pointer tracking and imertion point blinking can continue. 

Don't call SystemTask 

If you call WaitNextEvent, MultiFinder will be responstble for giving time to drivers 
(that is, the system will call SystemTask).  The important point here is that since 
applications running in the background are not guaranteed processing time and may 
be in a sleep state at any time, they cannot call SystemTask a sufficient number of 
times. 

When  exactly are applications  moved between the 
foreground  and the  background? 

Applications are moved between the foreground and background when you make an 
event call.  If you have the acceptSuspend.ResumeEvents bit set in the SIZE resoiJrce, 
you will receive suspend/resume events.  When you receive a suspend event from an 
event call you will be moved from the foreground to the background the next time you 
make an event call.  When an application receives a suspend event, it is going to be 
switched, so don't do anything to try to retain control. 
+ Programming tip: Masking out the suspend event is not a good Macintosh 

programming technique.  This is particularly important if you are setting a flag to 
tell if your application is in the foreground or background. 

How can I tell If WaltNextEvent Is Implemented? 
WaitNextEvent is part of Finder version 6.0.  Most applications should not need to 
know if MultiFinder is running since furure systems might include WaitNextEvent 
whether or not MultiFinder is running.  Most of the time, the application really needs 
to know something like: •How can I tell if WaitNextEvent is implemented?• 

c 

WaltNextEvent 

35 


---
The following Pascal and C code fragments are included here to demonstrate how to 
check whether WaitNextEvent is implemented (this code compares the trap for 
WaitNextEvent with the unimplemented trap).  Common to both of these code 
examples is a useful routine, called TrapAvailable, to check if a particular trap is 
available.  Here is the Pascal code for TrapAvailable: 

FUNCTION  TrapAvailable(tNumber:  INTEGER; 

tType:  TrapType):  BOOLEAN; 

CONST 

UnimplementedTrapNumber 

BEGIN  (TrapAvailable} 

..  $A89F; 

(trap  number  of  "unimplemented  trap"} 

{Check  and  see  if  the  trap  exists.  On  64K  ROM  machines,  tType  will  be  iqnored.} 

TrapAvailable  :- (  NGetTrapAddress(tNumber, 

tType)  <> 

GetTrapAddress(UnimplementedTrapNumber)  ); 

END; 

{TrapAvailable} 

Here is the C code for TrapAvailable: 

Boolean 
TrapAvailable(tNumber, 
short 
TrapType  tType 
{ 

tNumber 

tType) 

tifndef  _Unimplemented 
fdefine  _Unimplemented  OxA89F 
#endif 

/*  define  trap  number  for  old  MPW  or  non-MPW  c  */ 

/*  Check  and  see  if  the  trap  exists.  On  64K  ROM  machines,  tType  will  be  iqnored.  */ 

return (  NGetTrapAddress (tNumber, 

tType)  !•  GetTrapAddress (_Unimplemented) 

) ; 

} 

Here's the Pascal code segment that shows how you should set up the call to the 
function that will actually check to see if WaitNextEvent is implemented, followed by 
the skeleton for calling either WaitNextEvent or GetNextEvent and 
SystemTask-depending on the outcome: 

{Note  that  you  call  both  GetNextEvent  and  SystemTask  if  WaitNextEvent  isn't 
available.} 

hasWNE  :•  WNEisimplemented; 

IF  hasWNE  THEN  BEGIN 

{  call  WaitNextEvent  } 

36 

Chapter 3:  MultlFlncler-Aware Applicattons 

~.· 


---
( 

END  ELSE  BEGIN 

{  call  SystemTask  and  GetNextEvent 

END; 

Here's the Pascal code segment that checks to see ifWaitNextEvent is implemented: 

FUNCTION  WNEisimplemented:  BOOLEAN; 

CONST 

WNETrapNumber  -

$A860; 

{trap  number  of  WaitNextEvent 

VAR 

theWorld 
discardError 

:  SysEnvRec; 
:  OSErr; 

BEGIN  {WNEisimplemented} 

{used  to  check  if  machine  has  new  traps 
{used  to  ignore  OSErr  return  from 
( 

SysEnvirons 

Since  WaitNextEvent  and  HFSDispatch  both  have  the  same  trap  number 
can  only  call  TrapAvailable  for  WaitNextEvent  if  you  are  on  a  machine  that 
supports  separate  OS  and  Toolbox  trap  tables.  Here,  call  SysEnvirons  and 
check  if  machineType  <  O.} 

( $60),  you 

discardError  :=  SysEnvirons(l,  theWorld); 

Even  if  you  get  an  error  from  SysEnvirons,  the  SysEnvirons  glue  has  set  } 
up  machineType. } 

IF  theWorld.machineType  <  0  THEN 

WNEisimplemented  ·- FALSE 

ELSE 

this  ROM  doesn •t  have  separate  trap 
tables  or  WaitNextEvent 
check  for  WaitNextEvent 

WNEisimplemented 

:=  TrapAvailable(WNETrapNumber,  ToolTrap); 

END; 

{  WNEisimplemented 

Here's the same example in C: 

/*  Note  that  you  call  both  GetNextEvent  and  SystemTask  if  WaitNextEvent  isn't 
I*  available.  */ 

*I 

hasWNE  - WNEisimplemented (); 

i f   (hasWNE) 

/*  call  WaitNextEvent 

else 

/*  call  SystemTask  and  GetNextEvent 

*I 

*I 

WaltNextEvent 

37 


---
Boolean 
WNEisimplemented() 
{ 
I*  define  trap  number  for  old  MPW  or  non-MPW  C  */ 

tifndef  WaitNextEvent 
#define  ::=waitNextEvent  OxA860 
tendif 

SysEnvRec  theWorld; 

/*  used  to  check  if  machine  has  new  traps  */ 

/*  Since  WaitNextEvent  and  HFSDispatch  both  have  the  same  trap  number  ($60),  you  */ 
* / 
I*  can  only  call  TrapAvailable  for  WaitNextEvent  if  you  are  on  a  machine  that 
I*  supports  separate  OS  and  Toolbox  trap  tables.  Call  SysEnvirons  and  check  if 
*/ 
* / 
I*  machineType  <  0 • 

SysEnvirons(l,  &theWorld); 

/*  Even  if  you  qet  an  error  from  SysEnvirons,  the  SysEnvirons  qlue  has  set  up 
/*  machineType.  */ 

*/ 

if  (theWorld.machineType  <  0)  { 

return (false) 

/*  this  ROM  doesn •t  have  separate  trap  tables  or  WaitNextEvent  */ 

else  { 

return(TrapAvailable(_WaitNextEvent,  ToolTrap)); 

/*  check  for  WaitNextEvent  */ 

+ Note: WaitNextEvent does not conflict with any OS trap, so the above test is valid on 

64KROMs. 

Temporary memory allocation calls 
To reduce the memory requirements of an application's heap, MultiFinder provides a 
set of temporary memory allocation services that can be used for large transient 
memory requirements. 

An application now has the option of using MultiFinder's temporary memory 
allocation calls to get additional memory; however, don't rely on always getting it 
because this additional memory may not be available.  The application should still 
work if there is no additional memory available when you need it  Also, this memory 
is meant to be transitory; the application should use the memory for a limited time 
and then return it to the system for other applications to use. 

This temporary memory should be released before you call WaitNextEvent again. 
Make the call, use the memory, and then release it 

38 

Chapter 3:  MultlFlnder-Aware Applications 


---
It is important to remember a number of thin~ when using this memory.  First, you 
must use the temporary memory allocation calls when referencing these relocatable 
blocks because of the different handle requirements.  Second, be sure to release the 
blocks of memory as soon as possible to allow other applications to use them, and to 
allow the user to launch new applications.  Finally, never structure your application 
such that it depends on the availability of any of this temporary memory.  1bis means 
having a backup plan in place should no temporary memory be available (most likely 
reserving an emergency amount of memory within your heap zone to complete the 
common procedures). 
+  Note: Do not treat these calls as Memory Manager blocks.  For example, don't call 
GetHandleSize or SetHandleSize.  Also,  don't call Toolbox routines that will call 
GetHandleSize or SetHandleSize. 

For example, the Finder now uses these temporary memory calls to secure copy buffer 
space to be used during me copy operations.  Any available memory (unused by 
running applications) is dedicated to this purpose.  The Finder releases the memory as 
soon as the copy is completed, thus making the memory available again to other 
applications, or to MultiFinder for launching new applications. 

If the Finder cannot allocate this large temporary copy buffer, it will perform the copy 
using a reserved small copy buffer from within its own heap :zone.  1bis is dearly more 
desirable than refusing to copy (or worse yet, crashing) because no temporary 
memory was available. 

( 

There are several temporary memory allocation calls: 

•  FUNCTION  MFFreeMem 

: LONG INT 

MFFreeMem returns the total amount of free  memory available for temporary 
allocation, in bytes. 

•  FUNCTION  MFMaxMemCVAR  grow:Size) 

:  Size 

MFMaxMem compacts the MultiFinder heap :zone and returns the number of bytes 
of the largest contiguous free block for temporary allocation. 

•  FUNCTION  MFTempNewHandle(logicalSize:Size;VAR 

resultCode:OSErr) :Handle 

MF'fempNewHandle attempts to allocate a new relocatable block oflogicalSize 
bytes for temporary usage and return a handle to it. The new block will be unlocked 
and unpurgeable. If an error occurs, MFfempNewHandle will return nil. 

Result codes: noErr 

memFullErr 

No error 
Not enough room 

•  FUNCTION  MFTopMem:  Ptr 

MF'fopMem returns a pointer to the top of the addressable RAM space. 
+ Note: Do not use this call to calculate the size of your application's memory 

partition.  This call provides the total amount of useable machine memory-.not 
the amount of memory available to your application. 

Temporary memory allocation calls 

39 


---
•  PROCEDURE  MFTempDisposHandle (h:Handle;  VAR  resultCode :OSErr) 

MFI'empDisposHandle releases the memoiy ocrupied by the relocatable block 
whose handle is h. 

Result codes: noErr 

memWZErr 

No error 
Attempt to operate on a free block 

•  PROCEDURE  MFTempHLock (h:Handle;  VAR  resultCode:OSErr) 

MFI'empHLock locks the specified relocatable block, preventing it from being 
moved within the MultiFinder heap zone. 

Result codes: noErr 

nilHandleErr 
memWZErr 

No error 
Nil master pointer 
Attempt to operate on a free block 

•  PROCEDURE  MFTempHUnlock (h:Handle;  VAR  resultCode:OSErr) 

MFI'empHUnlock unlocks the specified relocatable block, allowing it to move. 

Result codes: noErr 

nilHandleErr 
memWZErr 

No error 
Nil master pointer 
Attempt to operate on a free block 

How can I tell  if the temporary memory allocation calls are 
·implemented? 

The technique that's used to determine this is similar to the technique for determining 
if WaitNextEvent is implemented.  In Pascal: 

FUNCTION  TempMemCallsAvailable:  BOOLEAN; 

CONST 

OsDispatchTrapNumber  •  $ABBF; 

(  trap  number  of  temporary  memory  calls  } 

BEGIN 

(  TempMemCallsAvailable  } 

Since  OSDispatch  has  a  trap  number  that  was  always  defined  to  be  a  Toolbox 
trap  ($8F),  you  can  always  call  TrapAvailable.  If  you  are  on  a  machine  that 
does  not  have  separate  Os  and  Toolbox  trap  tables,  you'll  still  qet  the  riqht 
trap  address. 

(  check  for  OSDispatch 

TempMemCallsAvailable  :•  TrapAvailable (OSDispatchTrapNumber,  Tool Trap); 

END; 

(TempMemCallsAvailable} 

Now, the same example in C: 

40 

Chapter 3:  MultlAnder-Aware Applications 

"-·. 


---
( 

Boolean 
TempMemCallsAvailable() 
{ 

/*  define  trap  number  for  old  MPW  or  non-MPW  c  */ 

iifndef  _OSDispatch 
tdefine  _OSDispatch  OxABBF 
tendif 

/*  Since  OSDispatch  has  a  trap  number  that  was  always  defined  to  be  a  Toolbox 
* / 
I*  trap  ($BF).  you  can  always  call  TrapAvailable.  If  you  are  on  a  machine  that  */ 
*/ 
/*  does  not  have  separate  OS  and  Toolbox  trap  tables,  you' 11  still  get  the 
*/ 
/*  right  trap  address. 

return(TrapAvailable(_OSDispatch,  ToolTrap)); 

I*  check  for  OSDispatch 

*I 

Launching and sublaunching 
Certain types of applications, such as development systems, need to launch other 
applications.  MultiFinder provides a new platform for applications to interactively 
communicate with such applications.  The application launched by your application 
will become the foreground application.  A sublaunch is the mechanism for allowing 
your application to call another application.  Unlike the single-Finder environment, 
under MultiFinder when the user quits the application that you sublaunched, control 
does not necessarily retum to your application, but rather to the next frontmost layer. 

To launch another application and keep your rurrendy active application open, set 
both high bits of LaunchFlags, that is 

LaunchFlags:•  $COOOOOOO; 

Here is the Launch parameter block description: 

typedef  struct  LaunchBlock  { 

StringPtr 
unsigned  short 
unsigned  short 

name; 
soundBuffers; 
launchBlockID; 

idefine EXTENDED  BLOCK  ID 

((unsigned  short) 'LC') 

unsigned  -long  -

extendedBlockLen; 

tdefine 
( (pLaunchBlock'i->launchBlockID  ••  EXTENDED_BLOCK_ID 
' '   (pLaunchBlock)->extendedBlockLen  >•  4) 

IS  EXTENDED  BLOCK(pLaunchBlock) 

\ 

unsigned  short 

finderFileFlags; 

Launching  and sublaunchlng 

41 


---
idefine 

FINDER_FILE_FLAG_MULTILAUNCH  ((short) (1<<6)) 
launchFlaqs; 

unsiqned  short 

tdefine 
tdefine 
tdefine 

LAUNCH_FLAG_SUBLAUNCH 
LAUNCH_FLAG_TWITCHLAUNCH 
IS_TWITCH_LAUNCH(pLaunchBlock) 

((short) (1<<15)) 

((short) (1<<14)) 

(pLaunchBlock->launchFlaqs 

(IS_EXTENDED_BLOCK(pLaunchBlock) 
'  LAUNCH_FLAG_TWITCHLAUNCH) l 

\ 

U 
)  LaunchBlock; 

Unlike the single-Fmder model, if you set both high bits of LaunchPlap, your 
application will continue to execute after calling Launch, so be prepared.  Calling 
Launch with both high bits of LaunchPlap set can be thcught of as a request to launch 
an application.  The actual execution of that application's code (and hence suspend of 
your application) won't happen in the Launch trap, but at a later time (after a call or 
two to WaitNextEvent). 
+ Wamtng:· The interface to the Launch trap will eventually change.  Unless you are 
implementing an integrated development system, your application should not 
launch other applications. 

Launch under MultiFinder will currently return an error if there isn't enough memory 
to launch the desired application, if the desired application can't be located, or if the 
desired application is already open.  In the latter case, that application will not be 
made active. 

If you sublaunched, control will return to your application; if not, your application will 
be terminated and the next frontmost layer will become active.  If you didn't 
sublaunch and an error occurred, MultiFinder will do a SysBeep since your 
application will be terminated  If you sublaunched and an error occurred, 
MultiFinder will not beep and your application will have to report the error to the user. 

Launch returns the error in register DO if you are sublaunching.  You can check for 
DO < 0 after the sublaunch to see if the launch failed  If DO >- 0, then the application 
will be launched  The following Pascal code segment will return an error if launch 
fails: 

FUNCTION  Launchit (pLnch:  pLaunchStruct) :OSErr; 

INLINE 

(SP) +,AO  ;pointer  to  parameters  in  AO) 

$205F,  {MOVE.L 
$A9F2,  {  Launch) 
$3E80;  {MOVE.W  DO,  (A7) 

;qet  function  result  from  DO) 

42 

Chapter 3:  MultlFlnder-Aware Applications 


---
Working directories 

A new Working Directory Control Block (WDCB) must be created and set as the 
current directory when your application is run under MultiFinder (unless the current 
application represents the root or exists on an MPS volume). 

Under MultiFinder, when you call PBOpenWD, the ioWDProcID that you pass in is 
ignored.  MultiFinder overrides your ioWDProcID with a unique process ID for your 
application so that it can deallocate all working directories that you allocated when 
your application terminates.  Thus, you cannot use the ioWDProcID to identify your 
working directories when running under MultiFinder. 

Therefore, whenever you open a working directory with PBOpenWD, you should pass 
your application's signature as the ioWDProcID and close the working directory as 
soon as possible with PBCloseWD.  Also,  remember to deallocate each WDCB, since 
the sublaunching process is recursive and there is a limit to the number of WDCB's 
that can be created  A good programming practice is to check for errors after calling 
PBOpenWD.  A tMWDOErr (-121) error indicates that all available WDCB's have 
been allocated. 

Launching  and  sublaunching 

43 


---

---
( 

(··.~· ~ 

Chapter 4 

Special-Purpose  Applications 

Three special types of applications are described in this chapter: embedded services, 
faceless background tasks,  and desk accessories. 

Embedded services and faceless background tasks are applications that perform 
almost all their work in the background.  Desk accessories represent another special 
type of application that must follow certain programming guidelines to remain 
compatible with MultiFinder. 

Embedded services 
An embedded service is a special-purpose application that runs only in the 
background.  Tilis type of application is normally not visible and interacts heavily with 
the Notification Manager (see Appendix D for a detailed description). 

A good example of an embedded service that prints and uses heavy amounts of 
background time is PrintMonitor. 

PrintMonitor allows the user to interactively monitor what is being printed.  While 
PrintMonitor is running in the background, a user can bring it to the foreground to see 
which jobs are being held in the print spooler, alter the document printing order, 
cancel or suspend any or all waiting documents, or set times for particular documents 
to be printed  Tilis allows the user to print out large doruments during times when the 
LaserWriter® might be idle or rarely used. 

45 


---
Faceless background tasks 
A faceless background task is almost invisible.  It is minimal in size and has no user 
interface-no icon will appear in the Apple menu, no windows will be displayed, and 
no port exists.  If any user interaction is required, it uses the Notification Manager. 

A faceless background task sets the canBackground and backgroundOnly bits in the 
SIZE resource and should not significantly affect the responsive nature of the 
application running in the foreground 

An example of a faceless background task is Backgrounder, a continually active but 
user-transparent program.  Its main function in life is to seek out and identify the 
creation of a printer spool file.  Spool files are created under MultiFinder when a user 
wants to print a document in the background  When Backgrounder sees a spool file, it 
sets the printing process in motion by launching PrintMonitor. 

Desk accessories 
Desk accessories were originally designed for the Macintosh environment because 
they offered two distinct advantages over applications.  First, they incorporated a 
limited degree of multitasking.  Second, by using the Oipboard, they offered a 
primitive type of interprocess communication.  MultiFinder now makes these 
advantages available to applications as well as desk accessories. 

Since MultiFinder will eventually represent the sole Macintosh desktop environment, 
you will be better served in the future if you design and write a small application rather 
thari a desk accessory.  This does not mean that desk accessories are not compatible 
with MultiFinder.  While small applications are now preferable to desk accessories, 
MultiFinder does support the standard desk accessory model; however, there have 
been some changes. 

The major change is that desk accessories in the System file are now loaded into the 
system heap rather than the application heap (except when the Option key is held 
down).  This means that certain desk accessories that rely on being opened in the 
application heap of specific applications may not work under MultiFinder.  Also, when 
a user opens a desk accessory or clicks on one already open, MultiFinder brinp all 
open desk accessories to the foreground 

Desk accessories can be divided into two different categories: self-sufficient and 
dependent  Self sufficient desk accessories will continue to work as intended under 
MultiFinder. 

46 

Chapter 4:  Special-Purpose Applications 


---
( 

( 

Self-sufficient desk accessories 

Self-sufficient desk accessories do not rely on the presence of specific applications to 
function-that is, they don't need to be in a partirular application's heap in order to 
work correctly.  The standard desk accessories from Apple, such as the Scrapbook and 
Notepad, are examples of self-sufficient desk accessories.  A self-sufficient desk 
accessory also doesn't care about the rest of the world while it's running.  Under 
MultiFinder, a desk accessory has no way of knowing which application was active when 
the user opened it 

Dependent desk  accessories 

A dependent desk accessory relies on an information exchange with a specific 
application that allows it to perform its particular function.  However, under 
MultiFinder, this exchange breaks down because in general, the desk accessory does 
not load into the application heap and has no way of determining which application 
opened it. 

An example of a dependent desk accessory is a spelling checker that only works with 
certain word processing applications.  This sort of desk accessory won't work under 
MultiFinder.  Such desk accessories usually use the scrap to keep the text they're going 
to check and rely on posting events to tell the word processing application to save the 
text to the scrap.  Unfortunately, at accRun time, the desk accessory doesn't know 
which MultiFinder partition (specific application heap) called it.  This means that 
spelling checker desk accessories can no longer post events to begin the text retrieval 
process. 

Error checking 
While it is true that MultiFinder will enlarge the system heap to make room for desk 
accessories if possible, all desk accessories need to contain thorough error checking to 
see if they have enough memory to load 

A desk accessory will not have any indication that MultiFinder has loaded it, or that 
there is additional room in the system heap.  To prevent possible memory problems, 
desk accessories can check to see if there is enough memory to load by trying to 
allocate all the memory they need and exiting gracefully if there is not enough 
available. 

Desk  accessories 

47 


---

---
{ 

Appendix  A 

A  C  Example  of  a  MultiFinder 
Aware  Application 

The following C program is an example of a MultiFinder-aware application. 

MultiFinder-Aware  Sample  Application 

Copyright  C  1988  Apple  Computer,  Inc. 
All  rights  reserved. 

I*  -----------------------------------------------------------------------------*/ 
*I 
/* 
*/ 
/* 
*/ 
/* 
/* 
*/ 
I* 
*/ 
/* 
*/ 
*/ 
/* 
*I 
/* 
*I 
/* 
*/ 
/* 
/*------------------------------------------------------------------------------*/ 

This  sample  application  was  written  by  Macintosh  Developer  Technical 
Support. 
can  enter  and  edit  text. 

It  displays  a  single,  fixed-size  window  in  which  the  user 

/*  Segmentation  strategy: 

This  program  consists  of  three  segments.  Main  contains  most  of  the  code, 
including  the  MPW  libraries,  and  the  main  program. 
code  that  is  only  used  once,  during  startup,  and  can  be  unloaded  after  the 
program  starts. 
globals  for  the  MPW  libraries  and  is  unloaded  right  away.  */ 

•ASinit  is  automatically  created  by  the  Linker  to  initialize 

Initialize  contains 

/*  SetPort  strategy: 

Toolbox  routines  do  not  change  the  current  port.  However,  this  program  uses 
a  strategy  of  callinq  SetPort  whenever  you  want  to  draw  or  make  calls  that 
depend  on  the  current  port.  This  makes  you  less  vulnerable  to  bugs  in  other 
software  that  might  alter  the  current  port  (such  as  the  buq  (feature?)  in 
many  desk  accessories  that  change  the  port  on  OpenDeskAcc). 

This  strategy 

49 


---
also  makes  the  routines  from  this  proqram  more  self-contained, 
since  they  don't  depend  on  the  current  port  settinq.  */ 

I*  Clipboard  strateqy: 

This  proqram  does  not  maintain  a  private  scrap.  Whenever  a  cut,  copy,  or  paste 
occurs,  you  import/export  from  the  public  scrap  to  TextEdit • s  scrap  riqht  away, 
usinq  the  TEToScrap  and  TEFromScrap  routines. 
lf  you  did  use  a  private  scrap, 
the  import/export  would  be  in  the  activate/deactivate  event  and  suspend/resume 
event  routines.  */ 

tinclude  <Values.h> 
tinclude  <Types.h> 
tinclude  <QuickDraw.h> 
tinclude  <Fonts.h> 
tinclude  <Events.h> 
tinclude  <Controls.h> 
tinclude  <Windows.h> 
tinclude  <Menus.h> 
tinclude  <TextEdit.h> 
tinclude  <Dialoqs.h> 
#include  <Desk.h> 
tin elude  <Scrap.h> 
tinclude  <ToolUtils.h> 
#include  <Memory.h> 
tinclude  <SeqLoad.h> 
tinclude  <Files.h> 
tinelude  <OSUtils.h> 
tinclude  <Traps.h> 

/*  MPP  2.0.2  Traps.h  is  missinq  an  tendif 

*/ 

/*  MaxOpenDocuments  is  used  to  determine  whether  a  new  document  can  be  opened 
*/ 
/*  or  created.  You  keep  track  of  the  number  of  open  documents,  and  disable  the*/ 
If  the  *I 
/*  menu  items  that  create  a  new  document  when  the  maximum  is  reached. 
I* 
*/ 

number  of  documents  falls  below  the  maximum,  the  items  are  enabled  aqain. 

idefine 

maxOpenDocuments 

1 

/*  SysEnvironsVersion  is  passed  to  SysEnvirons  to  tell  it  which  version  of  the  */ 
I* 
*/ 

SysEnvRec  is  understood. 

#define 

sysEnvironsVersion 

1 

I*  OSEvent  is  the  event  number  of  the  suspend/resume  and  mouse-moved  events 
sent  by  MultiFinder.  Once  you  determine  that  an  event  is  an  osEvent, 
/* 
I* 
look  at  the  hiqh  byte  of  the  messaqe  sent  with  the  event  to  determine 
/*  which  kind  of  osEvent  it  is.  To  differentiate  suspend  and  resume  events, 
I* 

check  the  resumeMask  bit. 

tdefine 

osEvent 

app4Evt 

idefine 

suspendResumeMessaqe 

1 

/*  event  used  by 
/*  MultiFinder 
/*  hiqh  byte  of  suspend/ 

*I 
*/ 
*I 
*/ 
*I 

*/ 
*I 
*I 

50 

Appendix A:  A  C  Example of a  MultlRnder-Aware Application 


---
( 

(~ 

tdefine 

resumeMask 

tdef ine 

mouseMovedMessage 

1 

OxFA 

*I 
I*  resume  event  message 
I*  bit  of  message  field 
*I 
I*  for  resume  vs.  suspend  *I 
I*  high  byte  of  mouse-
*I 
I*  moved  event  message 
*I 

I*  The  following  constants  are  all  resource  IDs.  They  correspond  to  resources 
/* 

See  Appendix  c. 

in  Sample.r. 

*I 
*I 

fdefine 
tdefine 
tdefine 

rMenuBar 
rAboutAlert 
rDocWindow 

128 
128 
128 

I*  application's  menu  bar  *I 
*I 
I*  about  alert 
I*  application's  window 
*I 

/*  The  following  constants  are  used  to  identify  menus  and  their  items.  The  menu  *I 
*I 
I* 
constants  are  menu  IDs,  and  the  individual  item  constants  are  item  numbers 
*I 
/*  within  the  menus. 

fdef ine 
tdefine 

tdef ine 
#define 
tdefine 
#define 

fdefine 
fdefine 
fdefine 
#define 
tdefine 
fdefine 

mApple 
iAbout 

mFile 
iNew 
iClose 
iQuit 

mEdit 
iUndo 
iCut 
iCopy 
iPaste 
iClear 

I*  Apple  menu  *I 

I*  File  menu  */ 

I*  Edit  menu  *I 

128 
1 

129 
1 
4 
12 

130 
1 
3 
4 
5 
6 

I*  A  Document Record  contains  the  WindowRecord  for  one  of  the  document  windows,  *I 
*I 
as  well  as  the  TEHandle  for  the  text  being  edited.  Other  document  fields 
/* 
I* 
can  be  added  to  this  record  as  needed.  For  a  similar  example,  see  how  the  */ 
*I 
I* 
Window  Manager  and  Dialog  Manager  add  fields  after  the  grafPort. 

typedef  struct  { 
WindowRecord 
TE Handle 

window; 
te; 

DocumentRecord, 

*DocumentPeek; 

/*  GMac  is  used  to  hold  the  result  of  a  SysEnvirons  call.  This  makes 
/* 

it  convenient  for  any  routine  to  check  the  environment. 

SysEnvRec 

gMac; 

I*  set  up  by  Initialize 

I*  GHasWaitNextEvent  is  set  at  startup,  and  tells  whether  the  WaitNextEvent 
I* 

If  it  is  false,  GetNextEvent  must  be  called. 

trap  is  available. 

Boolean 

gHasWaitNextEvent; 

I*  set  up  by  Initialize 

*I 
*I 

*/. 

*I 
*/ 

*I 

AC Example of a  MultlAnder-Aware Application 

51 


---
/*  GinBackground  is  maintained  by  the  osEvent  handling  routines.  Any  part  of 
/* 

* / 
the  program  can  check  it  to  find  out  if  it  is  currently  in  the  background.  * / 

Boolean 

ginBackground; 

/*  maintained  by  Initialize  and  DoEvent  */ 

/*  GNumDocuments  is  used  to  keep  track  of  how  many  open  documents  there  are  at 
It  is  maintained  by  the  routines  that  open  and  close  documents. 
/* 

any  time. 

* / 
*I 

short 

gNumDocuments; 

/*  maintained  by  Initialize, 
/* 

DoCloseWindow 

Do New,  and  *I 
*I 

/*  Here  are  declarations  for  all  the  C  routines. 
/* 

actual  prototypes  for  parameter  type  checking. 

In  MPW  3. O  you  can  use 

*/ 
*I 

/*  EventRecord  *event  */  ); 

void  Event Loop (I; 
void  DoEvent (  /*  EventRecord  *event  */  ) ; 
void  AdjustCursor(  /*  Point  mouse,  RgnHandle  region  */  ) ; 
void  DoOpdate (  /*  WindowPtr  window  *I  ) ; 
void  DoDeactivate (  /*  WindowPtr  window  */  ) ; 
void  DoActivate(  /*  WindowPtr  window*/  ); 
void  DoContentClick (  I*  WindowPtr  window,  EventRecord  *event  *I  ) ; 
void  DoKeyDown( 
long  GetSleep (I ; 
void  Do Idle ( l ; 
void  DrawWindow( 
void  AdjustMenus (I  ; 
void  DoMenuCommand ( 
void  DoNew ( l ; 
void  DoCloseWindow (  I*  WindowPt r  window  *I  ) ; 
void  DoCloseBehind(  /*  WindowPtr  window  */  l; 
void  Terminate ( l ; 
void  Initialize(); 
Boolean  IsAppWindow (  /*  WindowPtr  window  *I  l ; 
Boolean  IsDAWindow( 
/*  WindowPtr  window  */  ); 
Boolean  TrapAvailable (  /*  short  tNumber,  TrapType  tType  */  l; 

/*  long  menuResult  *I  l; 

/*  WindowPtr  window  */  l; 

/*  Define  HiWrd  and  LoWrd  macros  for  efficiency.  *I 

tdefine  HiWrd (aLongl  ( ( (aLong)  >>  16)  &  OxFFFF) 
tdefine  LoWrd (aLong)  ( (aLong)  &  OxFFFFI 

/*  Define  TopLeft  and  BotRight  macros  for  convenience.  Notice  the  implicit 
/* 

dependency  on  the  ordering  of  fields  within  a  Rect. 

*/ 
*/ 

tdefine  TopLeft (aRect) 
tdefine  BotRight (aRect) 

(*  (Point  *l 
(Point  *l 
(* 

& (aRect) .top) 
&(aRect) .bottom) 

extern  void  _Dataini t  Cl  ; 

/*  This  routine  is  automatically  generated  by  the  MPW  Linker.  This  external 
/* 
reference  to  it  is  made  so  that  its  segment,  tASinit,  can  be  unloaded. 

*I 
*I 

52 

Appendix A:  A  C Example of a  MulflFlnder-Aware Applicaflon 


---
( 

idefine  _SEG_  Main 
main() 
{ 

UnloadSeg ( (Ptr)  _Datainit); 
MaxApplZone(); 

/* 
I* 
/* 

NOTE:  _Datainit  must  not  be  in  Main 
expand  the  heap  so  code  segments 

load  at  the  top 

Initialize (); 
UnloadSeg ( (Ptr)  Initialize); 

initialize  the  program 

/* 
/*  NOTE:  Initialize  must  not  be  in  Main 

Event Loop (); 

/*  call  the  main  event  loop 

*I 
*/ 
*/ 

*I 
*/ 

*/ 

I*  Get  events  forever,  and  handle  them  by  calling  DoEvent.  Also  call 
/* 

Adjustcursor  each  time  through  the  loop. 

*/ 
*/ 

idefine  _SEG_  Main 

void  Event Loop () 
( 

RgnHandle  cursorRgn; 
Boolean 
Event Record 

ignoreResult; 
event; 

cursorRgn  - NewRgn (); 
do  { 

if  (  gHasWaitNextEvent 

ignoreResult  - WaitNextEvent (everyEvent,  &event, 

GetSleep(),  cursorRgn); 

else  { 

SystemTask(); 
ignoreResult  - GetNextEvent (everyEvent,  &event); 

AdjustCursor(event.where,  cursorRgn); 
DoEvent(&event); 

while  (  true  ) ; 

I*  loop  forever  *I 

/*EventLoop*/ 

/*  Do  the  right  thing  for  an  event.  Determine  what  kind  of  event  it  is,  and  call* I 
I* 
*/ 

the  appropriate  routines. 

SEG  Main 

fdefine 
void  DoEvent (event) 
EventRecord 

*event; 

part; 

short 
WindowPtr  window; 
char 

key; 

switch 

event->what 

A C  Example of a  MultlRnder-Aware Application 

53 


---
case  nullEvent: 

Doidle (); 
break; 

case  mouseDown : 

part  •  FINDWINDOW (event->where,  &window) ; 
.switch  (  part  l 

( 
inMenuBar: 

case 

AdjustMenus (); 
DoMenuCommand(MENUSELECT(event->where)); 
break; 

case 

inSysWindow: 

SystemClick (event,  window) ; 
break; 
inContent: 

case 

i f   (  window  ! •  FrontWindow () 
SelectWindow(window); 
/*DoEvent(event);*/ 

) 

( 

I*  use  this  line  for 
/* 
"do  first  click" 

*I 
*/ 

else 

break; 

case 

inDraq: 

DoContentClick(window,  event); 

DRAGWINDOW(window,  event->where, 

&qd.screenBits.bounds); 

break; 
inGoAway: 

case 

i f   (  TRACKGOAWAY(window,  event->where) 

DoCloseWindow(window); 

break; 

break; 

case  keyDown: 
case  autoKey: 

key  •  event->messaqe  &  charCodeMask; 
(event->modifiers  &  cmdKey) 
i f   ( 

! •  O 

I*  Command  key  down  */ 
i f   (  event->what  ••  keyDown  ) 
AdjustMenus(); 

( 

/*  enable/disable/check 
/* 

*/ 
menu  items  properly  */ 

DoMenuCommand(MenuKey(key)); 

}  else 

break; 

DoKeyDown(event); 

case  activateEvt: 

window  • 
i f   ( 

(WindowPtr)  event->messaqe; 

(event->modifiers  &  activeFlag)  !•  O  ) 

DoActivate(window); 

DoDeactivate(window); 

else 

break; 

case  updateEvt: 

DoUpdate((WindowPtr)  event->message); 
break; 

54 

Appendix A:  A  C Example of a  MultlAnder-Aware Application 


---
( 

case  osEvent: 

switch  (event->message  >>  24) 

I*  high  byte  of  message 

*I 

case  mouseMovedMes sage: 
Doidle(); 

break; 

case  suspendResumeMessage: 

/*  mouse  moved  is  also  an  *I 
*I 
I*  idle  event 

window  - Front Window() ; 
if  (  event->message  &  resumeMask 

ginBackground  =  false; 
DoActivate(window); 

else  { 

ginBackground  -
true; 
DoDeactivate(window); 

/*  Have  to  treat 
*I 
/*  suspend/resume 
*I 
/*  as  deactivate/ 
*/ 
/*  activate  as  well.*/ 

break; 

break; 

/*DoEvent*/ 

/*  Change  the  cursor• s  shape,  depending  on  its  position.  This  also  calculates 
I* 

a  region  that  includes  the  cursor  for  WaitNextEvent. 

*I 
*I 

tdefine 
SEG  Main 
void  Adjustc,J;"sor (mouse, region) 

Point 
RgnHandle  region; 

mouse; 

WindowPtr  frontmost; 
RgnHandle  arrowRgn; 
RgnHandle  iBeamRgn; 
Re ct 

iBeamRect; 

frontmost  =  Front Window() ; 

/*  only  adjust  the  cursor  when 
/* 

you  are  in  front 

*I 
*I 

if 

( !  ginBackground)  " 

( ! 

IsDAWindow (frontmost)) 

)  { 

I*  calculate  regions  for  different  cursor  shapes  */ 

arrowRgn  =  NewRgn () ; 
iBeamRgn  =  NewRgn () ; 

/*  start  arrowRgn  wide  open  */ 
SetRectRqn(arrowRqn,  -32768,  -32768,  32767,  32767); 

/*  calculate  iBeamRgn  *I 
if  (  IsAppWindow{frontmost) 

iBeamRect  = 
SetPort(frontmost); 

(* ( {DocumentPeek) 

frontmost)->te)->viewRect; 

/*  make  a  global  version  of  the 
/* 

viewRect 

*/ 
*/ 

A  C  Example  of a  MultlAnder-Aware Application 

55 


---
LocalToGlobal(&TopLeft(iBeamRect)); 
LocalToGlobal(&BotRight(iBeamRect)); 
RectRgn (iBeamRgn,  &iBeamRect); 

I*  subtract  other  regions  from  arrowRgn  *I 
DiffRgn (arrowRqn, 

iBeamRqn,  arrowRqn); 

I*  change  the  cursor  and  the  region  parameter  */ 
iBeamRgn) 
i f   (  PTINRGN (mouse, 
SetCursor(*GetCursor(iBeamCursor)); 
CopyRgn(iBeamRgn, 

region); 

)  { 

else  { 

SetCursor(&qd.arrow); 
CopyRgn(arrowRgn, 

region); 

/*  qet  rid  of  local  regions  */ 
DisposeRqn(arrowRgn); 
DisposeRgn(iBeamRgn); 

/*AdjustCursor*/ 

/*  This  is  called  when  an  update  event  is  received  for  a  window. 
/* 

DrawWindow  to  draw  the  contents  of  an  application  window. 

It  calls 

*/ 
*/ 

SEG  Main 

#define 
void  DoUpdatelwindow) 
WindowPtr  window; 

if  (  IsAppWindow(window) 
BeqinOpdate(window); 
if  ( 

!  EmptyRqn (window->visRgn) 

DrawWindow(window); 

EndUpdate(window); 

/*DoOpdate*/ 

/*  this  sets  up  the  visRqn 
/*  draw  if  updating  is  needed 

*I 
*/ 

/*  This  is  called  when  a  window  is  deactivated.  */ 

#define  _SEG_  Main 
void  DoDeactivate (window) 
WindowPtr  window; 

if  ( 

IsAppWindow(window) 

) 

TEDeactivate ( ( (DocumentPeek)  window) ->te); 

/*DoDeactivate*/ 

/*  This  is  called  when  a  window  is  activated.  */ 

tdefine  _SEG_  Main 

56 

Appendix A:  A C Example of a  Mulflflnder-Aware Appllcaflon 


---
( 

void  DoActivate (window) 
WindowPtr  window; 

if  (  IsAppWindow(window) 

TEActivate(((DocumentPeek)  window)->te); 

/*DoActivate*/ 

I*  This  is  called  when  a  mouseDown  occurs  in  the  content  of  a  window.  */ 

tdefine 
void  DoContentClick (window, event I 

SEG  Main 

WindowPtr 
Event Record 

window; 
*event; 

Point 
Boolean 

mouse; 
shiftDown; 

if  (  IsAppWindow(window) 
SetPort (window); 
mouse  -
GlobalToLocal(,mouse); 

event->where; 

I*  extend  if  Shift  is  down  *I 
shiftDown  = 
TECLICK (mouse,  shiftDown, 

(event->modifiers  ' 

( (DocumentPeek)  window) ->tel ; 

/*  qet  the  click  position 
/*  convert  to  local  coordinates 

*I 
*I 

shiftI<ey) 

! ..  O; 

/*DoContentClick*/ 

/*  This  is  called  for  any  keyDown  or  autoI<ey  events,  except  when  the  Command 
I* 
I* 

It  looks  at  the  frontmost  window  to  decide  what  to  do 

key  is  held  down. 
with  the  key  pressed. 

*I 
*I 
*I 

tdefine 
void  DoKeyDown (event) 

SEG.:._  Main 

EventRecord 

*event; 

WindowPtr 
char 

frontmost; 
key;  , 

frontmost  •  FrontWindow(); 
if  (  IsAppWindow(frontmostl 

key  ..  event->messaqe  '  charCodeMask; 
TEKey(key, 

((DocumentPeek)  frontmost)->te); 

/*DoI<eyDown*/ 

/*  Calculate  a  sleep  value  for  WaitNextEvent.  This  takes  into  account  the  thinqs*/ 
I* 
*/ 

that  Doidle  does  with  idle  time. 

tdefine 
lonq  Get Sleep (I 

SEG  Main 

A C  Example of a  MultlRnder-Aware Appllcatlon 

57 


---
sleep; 

lonq 
WindowPtr  trontmost; 
TEHandle  te; 

sleep  •  MAXLONG; 
i f   ( 

!  qinBackqround  )  { 

frontmost  •  Front Window () ; 
if  (  IsAppWindow(frontmost) 
( (DocumentPeek) 

te  • 

(frontmost))->te; 

if  (  (*te) ->selStart  •=  (*te)->selEnd 
sleep  •  GetcaretTime (); 

return  sleep; 

/*GetSleep*/ 

/ 

I 

I*  default  value  for  sleep 
*/ 
I*  if  you  are  in  front  and  the. •  *I 
*I 
I* 
front  window  is  yours. • • 

I* 
I* 
I* 
I* 

I* 
I* 
I* 

and  the 

selection  is 
an  insertion 
point 

*I 
*/ 
•r 
*/ 

you  need  to  make  *I 
*/ 
*I 

the  insertion 
point  blink 

*I 
I*  This  is  called  whenever  you  qet  a  null  event  or  a  mouse-moved  event. 
I*  takes  care  of  necessary  periodic  actions.  For  this  proqram,  it  calls  TEidle.  */ 

It 

tdefine  _SEG_  Main 
_void  Doldle () 
{ 

WindowPtr  frontmost; 

frontmost  - Front Window () ; 
if  (  IsAppWindow ( frontmost) 
TEidle(((OocumentPeek) 

trontmost)->te); 

/.*Doldle*/ 

/*  Draw  the  contents  of  an  application  window.  *I 

tdefine  _SEG_  Main 
void  DrawWindow (window) 
WindowPtr  window; 

SetPort(window); 
TEUpdate(&window->portRect, 

/*DrawWindow*/ 

((DocumentPeek)  window)->te); 

I*  Enable  and  disable  menus  based  on  the  current  state.  This  is  called  just 
*/ 
•i 
/*  before  MenuSelect  or  MenuKey,  so  it  can  set  up  everything  for  the  Menu 
I*  Manaqer.  Since  these  are  the  times  that  the  user  can  see  the  menus  or  choose*/ 
*/ 
/* 

a  menu  item,  you  only  need  to  enable/disable  items  then. 

ldef ine 
void  AdjustMenus o 

SEG  Main 

58 

Appendix A: A  C  Example of a  MulttFlnder-Aware Appllcatton 


---
( 

WindowPtr 
Menu Handle 
Boolean 
Boolean 
Boolean 
TEHandle 

frontmost; 
menu; 
undo; 
cutCopyClear; 
paste; 
te; 

frontmost  =  Front Window () ; 

menu  - GetMHandle (mFile); 
if  (  9NumDocuments  <  maxOpenDocuments 

Enableitem(menu, 

iNew); 

else 

Disableitem(menu, 
if  (  frontmost  !- nil  ) 

iNew); 

Enableitem(menu, 

iClose); 

else 

Disableitem(menu, 

iClose); 

menu  =  GetMHandle (mEdit); 
undo  •  false; 
cutCopyClear  =  false; 
paste  ..  false; 

if  (  IsDAWindow(frontmost) 
true; 

undo  -

/*  New  is  enabled  when  you  can 
I* 
open  more  documents 

I*  Close  is  enabled  when  there 
is  a  window  to  close 
/* 

*I 
*/ 

*I 
*/ 

I*  all  editin9  is  enabled  for 
I* 

DA  windows 

*/ 
*/ 

cutCopyClear  =  true; 
true; 
paste  • 

else  if  (  IsAppWindow(frontmost) 

te 
if 

((DocumentPeek)  frontmost)->te; 
(*te) ->selStart  <  (*te) ->selEnd 

cutCopyClear  •  true; 

I*  Cut,  Copy,  and  Clear  are  enabled  for  application  windows  */ 
I* 
*/ 

with  selections 

paste  -

true;/*  Paste  is  enabled  for  application  windows 

*/ 

} 
if 

undo  ) 

Enableitem(menu, 

iOndo); 

else 

Disableitem(menu, 

iOndo); 

if  (  cutCopyClear  ) 
Enableitem(menu, 
Enableltem(menu, 
Enableitem(menu, 

{ 
iCut); 
iCopy); 
!Clear); 

else  { 

Disableitem(menu, 
Disableitem(menu, 
Disableitem(menu, 

iCut); 
iCopy); 
iClear); 

A  C Example of a  MJltlAnder-Aware Appllcatlon 

59 


---
i f  

paste  ) 

Enable!tem(menu, 

iPaste); 

else 

Disable!tem(menu, 

iPaste); 

/*AdjustMenus*/ 

I*  This  is  called  when  an  item  is  chosen  from  the  menu  bar  (after  calling 
I* 

MenuSelect  or  Menu Key)  •  It  does  the  right  thing  for  each  command. 

*I 
* / 

fdefine  _SEG_  Main 
void  DoMenuCommand(menuResultl 

long 

menuResult; 

short 
short 
short 
Str255 
short 
OSErr 
OS Err 
TE Handle 

menu ID; 
menu Item; 
itemHit; 
daName; 
daRefNum; 
error; 
ignoreResult; 
te; 

menu ID  - Hi Word (menuResult); 
menuitem  -
switch  (  menuID  )  { 
case  mApple: 

LoWord (menuResult); 

switch 

menu Item 
case  iAbout: 

/*use  macros  for  efficiency  to  get  ••  *I 
*I 
menu  item  number  and  menu  number 
/* 

/*  bring  up  alert  box  for  About 

*/ 

itemHit  - Alert (rAboutAlert,  nil); 
break; 

default: 

/*  all  non-About  items  in  this  menu 
/* 

are  DAs 
GETITEM(GetMHandle (mApple),  menuitem,  &daName); 
daRefNum  =  OPENDESKACC (&daName); 
break; 

*/ 
*/ 

break; 

case  mFile: 

switch  (  menuitem  ) 

case  iNew: 

DoNew(); 
break; 
case  iClose: 

DoCloseWindow(FrontWindow()); 
break; 

case  iQuit: 

Terminate () ; 

break; 

break; 

case  mEdit: 

/*  call  SystemEdit  for  DA  editing  and  MultiFinder 

*/ 

60 

Appendix A:  AC Example of a  MultlFlnder·Aware Application 


---
( 

,~ 

if  ( 

!  SystemEdit(menuitem-ll 

te  -
switch  (  menu!tem  )  { 

( (DocumentPeek)  Front Window ()) ->te; 

case  iCut: 

/*  after  cutting, 
I* 
the  TE  scrap 

export 

*/ 
*I 

TECut (te); 

error  - ZeroScrap (); 

i f   (  error  ==  noErr  ) 

ignoreResult  - TEToScrap(); 

break; 

case  iCopy: 

TECopy(te); 

/*  after  copying, 
error  - ZeroScrap(); 
I* 
the  TE  scrap 
i f   (  error  -=  noErr  ) 

ignoreResult  - TEToScrap(); 

export 

/*  import  the  TE 
I* 
error  •  TEFromScrap (); 
if  (  error  ==  noErr  ) 

before  pasting 

scrap 

break; 
case  iPaste: 

*/ 
*/ 

*I 
*/ 

TEPaste(te); 

break; 
case  iClear: 

TEDelete(te); 
break; 

break; 

HiliteMenu(O); 
/*DoMenuCommand*/ 

I*  unhighlight  what  MenuSelect  or  MenuKey  highlighted *I 

/*  Create  a  new  document  and  window.  */ 

#define  _SEG_  Main 
void  DoNew () 
{ 

Boolean 
Ptr 
WindowPtr window; 

good; 
storage; 

storage  •  NewPtr (sizeof (DocumentRecord)); 
if  (  storage  !- nil  )  { 

window  •  GetNewWindow(rDocWindow,  storage, 
( 
i f   (  window  !•  nil  ) 

(WindowPtr)  -1); 

gNumDocuments  +•  l; 

good  •  false; 
SetPort(window); 

/*  this  will  be  decremented  when  * l 
*I 
/* 

you  call  DoCloseWindow 

( (DocumentPeek)  window)->te  •  TENew(&window->portRect, 

&window->portRect); 

A  C  Example of a  MultlAnder-Aware Appllcatlon 

61 


---
if  ( 

( CDocumentPeek)  window)->te  !•  nil  ) 

qood  m  true; 

/*  if  TENew  succeeded,  the 
I* 
document  is  qood 

i f   C  qood  ) 

ShowWindow(window); 

else 

DoCloseWindow(window); 

else 

DisposPtr(storaqe); 

I*  if  the  document  is  qood, 
I* 
the  window  visible 

make 

I*  otherwise 
I* 
created 

reqret  you  ever 
it 

I*  qet  rid  of 
I* 
is  never 

the  storaqe  if 
used 

it 

*I 
*I 

*I 
*I 

*I 
*I 

*I 
*I 

/*DoNew*/ 

tdefine 
void  OoCloseWindow (window) 

SEG  Main 

WindowPtr  window; 

I*  Close  a  window.  This  handles  desk  accessory  and  application  windows.  *I 

TEHandle  te; 
if  (  IsDAWindow (window) 

CloseOeskAcc(((WindowPeek)  window)->windowKind); 

else  if  (  IsAppWindow(window) 

)  { 

te  = 
i f   (  te  ! •  nil  ) 

( (OocumentPeek)  window) ->te; 

TEDispose(te); 

OisposeWindow(window); 
qNumOocuments  -- l; 

/*DoCloseWindow*/ 

I*  dispose  the  TEHandle 

*I 

I*  Close  the  window  that  is  passed  and  all  windows  behind  it.  This  is  used  to  */ 
*I 
I*  close  all  the  windows  when  the  proqram  quits,  so  it  is  in  the  Terminate 
I*  segment.  Note  that  it  closes  windows  from  back  to  front,  by  calling  itself  */ 
I*  recursively,  which  minimizes  window  updating. 
*/ 

tdefine  _SEG_  Terminate 
void  OoCloseBehind (window) 

WindowPtr  window; 

if  (  window  !•  nil  )  { 

DoCloseBehind((WindowPtr) 
DoCloseWindow(window); 

/*DoCloseBehind*/ 

I*  if  you  are  passed  a  window,  close 
I* 

other  windows  behind  it  first 

(((WindowPeek)  window)->nextWindow)); 

/*  now  that  all  the  windows  behind  are 
I* 

closed,  close  this  one 

*I 
*I 

*I 
*I 

62 

Appendix A:  A C Example of a  Multlflnder-Aware Application 


---
( 

/*  Clean  up  the  application  and  exit.  Close  all  of  the  windows  so  they  can 
/* 

update  their  documents. 

*I 
*/ 

tdefine  _SEG_  Terminate 
void  Terminate () 
{ 

DoCloseBehind(FrontWindow()); 
Exit To Shell () ; 
/*Terminate*/ 

/*  close  all  windows  *I 

/*  Set  up  the  whole  world, 
/* 

and  a  single  blank  document. 

including  global  variables,  Toolbox  managers,  menus,  */ 
* / 

fdefine  _SEG_  Initialize 
void  Initialize() 
{ 

OSErr 

ignoreError; 

I*  Ignore  the  error  returned  from  SysEnvirons;  even  if  an  error  occured,  the  */ 
I*  SysEnvirons  glue  will  fill  in  the  SysEnvRec. 
*/ 

ignoreError  =  SysEnvirons (sysEnvironsVersion,  &gMac); 
if  (  gMac. machine Type  <  0 
l 

gHasWaitNextEvent  =  false; 

else 

/*  old  machines  have. • • 
/*  no  separate  trap  table;  no 
I*  WaitNextEvent 

*I 
*I 
*I 

gHasWaitNextEvent  =  TrapAvailable(_WaitNextEvent,  ToolTrap); 

ginBackground  =  false; 

InitGraf ( (Ptr)  &qd. thePort); 
InitFonts (); 
InitWindows(); 
InitMenus (); 
TEinit ()  ; 
InitDialogs(nill; 
Initcursor (); 

SetMenuBar(GetNewMBar(rMenuBar)); 
AddResMenu(GetMHandle(mApple), 
DrawMenuBar(); 

'DRVR'); 

/*  read  menus  into  menu  bar 
/*  add  DA  names  to  Apple  menu 

*/ 
*I 

gNumDocuments  =  O; 

/*  do  other  initialization  here  */ 

DoNew(); 
/*Initialize*/ 

I*  create  a  single  empty  document  *I 

/*  Check  if  a  window  belongs  to  the  application.  */ 

tdefine  _SEG_  Main 
Boolean 

IsAppWindow (window) 

A C Example of a  MultlRnder-Aware Application 

63 


---
WindowPtr  window; 

if  (  window  -- nil 
return  false; 

else 

/*  application  windows  have  non-neqative  windowKinds  */ 

return 

( (WindowPeek)  window)->windowKind  >•  O; 

/*IsAppWindow*/ 

/*  Check  if  a  window  belonqs  to  a  desk  accessory.  *I 

tdefine  _SEG_  Main 
Boolean 

IsDAWindow (window) 

WindowPtr  window; 

if  (  window  ••  nil 
return  false; 

else 

/*  DA  windows  have  neqative  windowKinds  */ 

return 

( (WindowPeek)  window)->windowKind  <  O; 

/*IsDAWindow*/ 

/*  Check  to  see  if  a  qiven  trap  is  implemented.  This  code  is· only  used  by  the 
/* 

Initialize  routine  in  this  proqram,  so  it  is  in  the  Initialize  seqment. 

*/ 
*/ 

tdefine  _SEG_  Initialize 
.Boolean  TrapAvailable (tNumber, tType) 
tNumber; 
tType; 

short 
TrapType 

/*  Check  and  see  if  the  trap  exists.  On  64K  ROM  machines,  tType  will  be  iqnored. */ 

return  NGetTrapAddress (tNumber, 
./*  TrapAvailable  */ 

tType)  !•  GetTrapAddress (_Unimplemented); 

64 

Appendix A:  A C  Example of a  MultlFfnder-Aware Appllcatlon 


---
Appendix  B 

A  Pascal  Example  of  a 
MultiFinder-Aware  Application 

The following Pascal program is an example of a MultiFinder-aware application. 

{-------------------------------------------------------------------------------} 

MultiFinder-Aware  Sample  Application 

Copyright  C  1988  Apple  Computer,  Inc. 
All  rights  reserved. 

This  sample  application  was  written  by  Macintosh  Developer  Technical 
Support. 
enter  and  edit  text. 

It  displays  a  single,  fixed-size  window  in  which  the  user  can 

{-------------------------------------------------------------------------------} 

} 
} 
} 
l 
} 
} 
} 
} 
} 
} 

{ 
{ 
{ 
{ 
{ 
{ 
{ 
{ 
{ 
{ 

PROGRAM  Sample; 

{Segmentation  strategy: 

This  program  consists  of  three  segments.  Main  contains  most  of  the  code, 
including  the  MPW  libraries,  and  the  main  program.  Initialize  contains 
code  that  is  only  used  once,  during  startup,  and  can  be  unloaded  after  the 
program  starts.  tASinit  is  automatically  created  by  the  Linker  to  initialize 
globals  for  the  MPW  libraries  and  is  unloaded  right  away.} 

{SetPort  strategy: 

Toolbox  routines  do  not  change  the  current  port.  However,  this  program  uses  a 
strategy  of  calling  SetPort  whenever  you  want  to  draw  or  make  calls·  that 

65 


---
depend  on  the  current  port.  This  makes  you  less  vulnerable  to  bugs  in  other 
software  that  might  alter  the  current  port  (such  as  the 
desk  accessories  that  change  the  port  on  OpenDeskAcc) •  This  strategy  also  makes 
the  routines  from  this  program  more  self-contained,  since  they  don't  depend  on 
the  current  port  setting. ) 

bug  (feature?)  in  many 

{Clipboard  strategy: 

This  program  does  not  maintain  a  private  scrap.  Whenever  a  cut,  copy,  or  paste 
occurs,  you  import/export  from  the  public  scrap  to  TextEdit • s  scrap  right  away, 
using  the  TEToScrap  and  TEFromScrap  routines.  If  you  did  use  a  private  scrap, 
the  import/export  would  be  in  the  activate/deactivate  event  and  suspend/resume 
event  routines.) 

· 

USES 

CONST 

MemTypes,  QuickDraw,  OSintf,  Toolintf; 

{MPW  3.0  will  include  a  Traps.p  interface  file  that  includes  constants  for 

trap  numbers. 

These  constants  are  from  that  file. ) 

_WaitNextEvent 
_Unimplemented 

•  $A860; 
•  $A89F; 

{MaxOpenDoeuments  is  used  to  determine  whether  a  new  document  can  be  opened 

or  created.  You  keep  track  of  the  number  of  open  documents,  and  disable  the 
menu  items  that  create  a  new  document  when  the  maximum  is  reached.  If  the 
number  of  documents  falls  below  the  maximum,  the  items  are  enabled  again.) 

maxOpenDocuments 

-

l; 

{SysEnvironsVersion  is  passed  to  SysEnvirons  to  tell  it  which  version  of  the 

SysEnvRec  is  understood.) 

sysEnvironsVersion 

l; 

{OSEvent  is  the  event  number  of  the  suspend/resume  and  mouse-moved  events  sent 
by  MultiFinder.  Once  you  determine  that  an  event  is  an  osEvent,  look  at  the 
high  byte  of  the  message  sent  with  the  event  to  determine  which  kind  of 
osEvent  it  is.  To  differentiate  suspend  and  resume  events,  check  the 
resumeMask  bit • ) 

osEvent 
suspendResumeMessaqe•  l; 
l; 
resumeMask 
$FA; 
mouseMovedMessaqe 

app4Evt; 

{event  used  by  MultiFinder) 
{hiqh  byte  of  suspend/resume  event  message} 
{bit  of  message  field  for  resu- vs.suspend} 
{high  byte  of  mouse-moved  event  messaqe) 

{The  following  constants  are  all  resource  IDs.  They  correspond  to  resources 

in  Sample. r. 

see  Appendix  C. 

) 

rMenuBar 
rAboutAlert 

128; 
- 128; 

{application• s  menu  bar) 
(about  alert) 

66 

Appendix B:  A  Pascal  Example of a  MultlRnder-Aware Appllcatlon 


---
( 

rDocWindow 

- 128; 

(application's  window} 

(The  followinq  constants  are  used  to  identify  menus  and  their  items.  The  menu 
constants  are  menu  IDs,  and  the  individual  item  constants  are  item  numbers 
within  the  menus.} 

mApple 
iAbout 

mFile 
iNew 
iClose 
iQuit 

mEdit 
iUndo 
iCut 
iCopy 
iPaste 
iClear 

12; 

129; 

- 128; 
- 1; 
..  1; 
- 4; 
- 130; 
- 4; 
- 5; 
- 6; 

1; 
=  3; 

(Apple  menu} 

(File  menu} 

(Edit  menu} 

TYPE 

VAR 

(A  DocumentRecord  contains  the  WindowRecord  for  one  of  the  document  windows, 
as  well  as  the  TEHandle  for  the  text  beinq  edited.  Other  document  fields 
can  be  added  to  this  record  as  needed.  For  a  similar  example,  see  how  the 
Window  Manaqer  and  Dialoq  Manaqer  add  fields  after  the  qrafPort. } 

DocumentRecord  =  RECORD 

window 
te 

WindowRecord; 
TEHandle; 

END; 

DocumentPeek 

..  "DocumentRecord; 

(GMac  is  used  to  hold  the  result  of  a  SysEnvirons  call.  This  makes 

it  convenient  for  any  routine  to  check  the  environment. } 

gMac 

SysEnvRec; 

(set  up  by  Initialize} 

(GHasWaitNextEvent  is  set  at  startup,  and  tells  whether  the  WaitNextEvent 

trap  is  available.  If  it  is  false,  GetNextEvent  must  be  called.  } 

qHasWaitNextEvent 

:  BOOLEAN; 

(set  up  by  Initialize} 

(GinBackqround  is  maintained  by  the  osEvent  handlinq  routines.  Any  part  of 

the_  proqram  can  check  it  to  find  out  if  it  is  currently  in  the  backqround:} 

ginBackground 

:  BOOLEAN; 

(maintained  by  Initialize  and  DoEvent) 

(GNumDocuments  is  used  to  keep  track  of  how  many  open  documents  there  are 

at  any  time.  It  is  maintained  by  the  routines  that  open  and  close  documents.) 

A  Pascal Example of a  MultlRnder-Aware Application 

67 


---
gNumDocuments 

INTEGER; 

{maintained  by  Initialize,  DoNew,  and 

DoCloseWindow} 

($S  Initialize} 
FUNCTION  TrapAvailable(tNumber: 

INTEGER; 

tType:  TrapType):  BOOLEAN; 

{Check  to  see  if  a  given  trap  is  implemented.  This  is  only  used  by  the 
Initialize  routine  in  this  program,  it  is  in  the  Initialize  segment.} 

BEGIN 
{Check  and  see  if  the  trap  exists.  On  64K  ROM  machines,  tType  will  be  ignored.} 

TrapAvailable 

:=  NGetTrapAddress(tNumber, 

tType}  <> 
GetTrapAddress(_Unimplemented); 

END;  {TrapAvailable} 

{ $S  Main} 
FUNCTION 

IsDAWindow(window:  WindowPtr):  BOOLEAN; 

(Check  i f   a  window  belongs  to  a  desk  accessory.} 

BEGIN 

IF  window  =  NIL  THEN 

ELSE 

IsDAWindow 
{DA  windows  have  negative  windowKinds} 
IsDAWindow 

:=  FALSE 

:=  WindowPeek (window) A. windowKind  <  O; 

END; 

{IsDAWindow} 

{ $S  Main} 
FUNCTION 

IsAppWindow(window:  WindowPtr):  BOOLEAN; 

(Check  if  a  window  belongs  to  the  application.} 

BEGIN 

IF  window  =  NIL  THEN 

ELSE 

IsAppWindow  :•  FALSE 
{application  windows  have  non-negative  windowKinds} 
: •  WindowPeek (window) A .  windowKind  >=  O; 
I sAppWindow 

END;  {IsAppWindow} 

{ $S  Main} 
PROCEDURE  DoCloseWindow(window:  WindowPtr); 

{Close  a  window.  This  handles  desk  accessory  and  application  windows.} 

BEGIN 

IF  IsDAWindow(window)  THEN 

CloseDeskAcc(WindowPeek(window)A.windowKind) 

ELSE  IF  IsAppWindow(window)  THEN  BEGIN 

68 

Appendix B:  A  Pascal  Example of a  MultlFinder-Aware Application 


---
( 

( 

WITH  Document Peek (window) A  DO 
IF  te  <>  NIL  THEN 

TEDispose(te); 

{dispose  the  TEHandle} 

DisposeWindow(window); 
gNumDocuments 

:=  gNumDocuments 

- 1; 

END; 

END; 

{DoCloseWindow) 

{ $S  Main} 
PROCEDURE  Do New; 

{Create  a  new  document  and  window.} 

VAR 

BEGIN 

good 
storage 
window 

BOOLEAN; 
Ptr; 
WindowPtr; 

storage 
IF  storage  <>  NIL  THEN  BEGIN 

: =  NewPtr (SIZEOF (Document Record)); 

window 
IF  window  <>  NIL  THEN  BEGIN 

:=  GetNewWindow {rDocWindow,  storage,  WindowPtr {-1) l; 

gNumDocuments  :•  gNumDocuments  +  1; {  this  will  be  decremented 

when  you  call  DoCloseWindow 

good  :=  FALSE; 
SetPort(window); 

WITH  windowA,  DocumentPeek (window) A  DO  BEGIN 
te  :=  TENew(portRect,  portRect); 
IF  te  <>  NIL  THEN 

good  :=  TRUE; 

{if  TENew  succeeded,  the 
document  is  good  } 

END; 
IF  good  THEN 

ShowWindow(window) 

{if  the  document  is  good,  make 

the  window  visible 

} 

DoCloseWindow(window); 

{otherwise  regret  you  ever 

created  i t }  

ELSE 

END  ELSE 

DisposPtr(storage); 

{get  rid  of  the  storage  i f  i t  

is  never  used} 

END; 
{DoNew} 

END; 

{$S  Initialize} 
PROCEDURE  Initialize; 

{Set  up  the  whole  world, 

including  global  variables,  Toolbox  managers,  menus,  and  a 

single  blank  document.} 

A  Pascal  Example  of a  Mul11Flnder-Aware  Appllca11on 

69 


---
VAR 

BEGIN 

ignoreError 

OSErr; 

{Ignore  the  error  returned  from  SysEnvirons;  even  if  an  error  occurred, 

the  SysEnvirons  glue  will  fill  in  the  SysEnvRec.} 

ignoreError  :•  SysEnvirons (sysEnvironsVersion,  qMac); 
IF  gMac.machineType  <  0  THEN 

gHasWaitNextEvent  :•  FALSE 

ELSE 

(old  machines  have ••• } 
(no  separate  trap  table;  no 

WaitNextEvent} 

gHasWaitNextEvent  :•  TrapAvailable (_WaitNextEvent,  Tool Trap); 

ginBackground  :•  FALSE; 

InitGraf(@thePort); 
InitFonts; 
InitWindows; 
InitMenus; 
TEinit; 
InitDialogs(NIL); 
InitCursor; 

SetMenuBar(GetNewMBar(rMenuBar)); 
AddResMenu(GetMHandle(mApple), 
DrawMenuBar; 

(read  menus  into  menu  bar} 

'DRVR');  (add  DA  names  to  Apple  menu} 

gNumDocuments 

:=  0; 

(do  other  initialization  here} 

DoNew; 

END;  {Initialize} 

{create  a  single  empty  document l 

{ $5  Terminate} 
PROCEDURE  DoCloseBehind(window:  WindowPtr); 

(Close  the  window  that  is  passed  and  all  windows  behind  it.  This  is  used  to  close 
all  the  windows  when  the  program  quits,  so  it  is  in  the  Terminate  segment.  Note 
that  it  closes  windows  from  back  to  front,  by  calling  itself  recursively,  which 
minimizes  window  updatinq.} 

BEGIN 

IF  window  <>  NIL  THEN  BEGIN 

{if  you  are  passed  a  window,  close  other 

DoCloseBehind(WindowPtr(WindowPeek(window)".nextWindow))_; 
DoCloseWindow (window); 

{now  that  all  the  windows  behind  are  closed, 

windows  behind  it  first} 

close  this  one} 

END; 

END;  {DoCloseBehind} 

{ $S  Terminate} 

70 

Appendix 8:  A  Pascal  Example of o  MulflRnder-Aware Application 


---
PROCEDURE  Terminate; 

{Clean  up  the  application  and  exit.  Close  all  the  windows  so  they  can  update  their 
documents.} 

BEGIN 

DoCloseBehind(FrontWindow); 
ExitToShell; 

{close  all  windows} 

END;  {Terminate} 

{$S  Main} 
PROCEDURE  AdjustMenus; 

{Enable  and  disable  menus  based  on  the  current  state.  This  is  called  just 

before  MenuSelect  or  MenuKey,  so  it  can  set  up  everything  for  the  Menu  Manager. 
Since  these  are  the  times  that  the  user  can  see  the  menus  or  choose  a  menu  item, 
you  only  need  to  enable/disable  items  then.} 

VAR 

frontmost 
menu 
undo 
cutCopyClear 
paste 

WindowPtr; 
MenuHandle; 
BOOLEAN; 
BOOLEAN; 
BOOLEAN; 

BEGIN 

frontmost 
menu 

·- FrontWindow; 
·- GetMHandle(mFile); 

IF  gNumDocuments  <  maxOpenDocuments  THEN 

Enable Item (menu, 

iNew); 

{  New  is  enabled  when  you  can  open  more  } 
{  documents  } 

ELSE 

Disableitem(menu, 
IF  frontmost  <>  NIL  THEN 

iNew); 

ELSE 

Enableitem(menu, 

iClose) 

Disableitem(menu, 

iClose); 

Close  is  enabled  when  there  is  a 
window  to  close  } 

menu 
undo 
cutCopyClear 
paste 

:•  GetMHandle(mEditl; 
:•  FALSE; 
:=  FALSE; 
:=  FALSE; 

IF  IsDAWindow ( frontmost)  THEN  BEGIN 

undo 
:•  TRUE; 
cutCopyClear  :- TRUE; 
: =  TRUE; 
paste 

{all  editing  is  enabled  for  DA  windows 

END  ELSE  IF  IsAppWindow(frontmost)  THEN  BEGIN 

WITH  Document Peek ( frontmost I". te"'"  DO 
IF  selStart  <  selEnd  THEN 
cutCopyClear 

: •  TRUE; 

A  Pascal  Example  of a  MultlFlnder-Aware Application 

71 


---
Cut,  Copy,  and  Clear  are  enabled  for  application 
windows  with  selections} 

paste  :=  TRUE; 

{Paste  is  enabled  for  application  windows} 

END; 
IF  undo  THEN 

Enableitem(menu, 

iUndo) 

ELSE 

Disableitem(menu, 

iUndo}; 

IF  cutCopyClear  THEN  BEGIN 

Enableitem(menu, 
Enableitem(menu, 
Enableitem(menu, 

iCut); 
iCopy); 
iClear); 

END  ELSE  BEGIN 

Disableitem(menu, 
Disableitem(menu, 
Disableitem(menu, 

iCut) ; 
iCopy); 
iClear); 

END; 
IF  paste  THEN 

Enableitem(menu, 

iPaste) 

ELSE 

END;  {AdjustMenus} 

Disableitem(menu, 

iPaste); 

I $S  Main} 
PROCEDURE  DoMenuCommand (menuResult:  LONGINT); 

{This  is  called  when  an  item  is  chosen  from  the  menu  bar  (after  calling 

MenuSelect  or  MenuKey)  •  It  does  the  right  thing  for  each  command.} 

VAR 

BEGIN 

menu ID 
menu Item 
itemHit 
daName 
daRefNum 
error 
ignoreResult 
te 

INTEGER; 
INTEGER; 
INTEGER; 
Str255; 
INTEGER; 
OSErr; 
OSErr; 
TEHandle; 

: - HiWrd (menuResult l; 

:=  LoWrd(menuResult); 

menu ID 
menuitem 
CASE  menu ID  OF 
mApple: 

{use  built-ins  (for  efficiency) ••• } 
{to  get  menu  item  number  and  menu  number} 

CASE  menuitem  OF 

iAbout: 
itemHit  :•  Alert (rAboutAlert,  NIL); 
OTHERWISE  BEGIN 

{bring  up  alert  box  for  About} 

{all  non-About  items  in  this 

Getitem(GetMHandle (mApple),  menu Item,  daName); 
daRefNum 

:=  OpenDeskAcc(daName}; 

menu  are  DAs} 

72 

Appendix B:  A  Pascal  Example of a  MultlAnder-Aware Application 


---
END; 

END; 

mFile: 

CASE  menuitem  OF 

iNew: 
DoNew; 
iClose: 
DoCloseWindow(FrontWindow); 
iQuit: 
Terminate; 

END; 

mEdit: 

{call  SystemEdit  for  DA  editing  and  MultiFinder} 

IF  NOT  SystemEdit (menuitem-1)  THEN  BEGIN 

te  :=  DocumentPeek(FrontWindow) A.te; 
CASE  menuitem  OF 

iCut:  BEGIN 

TECut (te); 

{after  cutting,  export  the  TE 

scrap} 

error  : - ZeroScrap; 
IF  error  - noErr  THEN 

ignoreResult 

: =  TEToScrap; 

END; 
iCopy:  BEGIN 

TECopy(te); 

(after  copying,  export  the  TE 

scrap} 

error  :- ZeroScrap; 
IF  error  •  noErr  THEN 
ignoreResult 

: =  TEToScrap; 

END; 
iPaste:  BEGIN 

{import  the  TE  scrap  before 

error  : =  TEFromScrap; 
IF  error  =  noErr  THEN 

TEPaste(te); 

END; 
iClear: 

TEDelete(te); 

pasting) 

END; 

END; 

END; 
HiliteMenu(O); 

END; 

{DoMenuCommand) 

( unhighlight  what  Menu Select  or  MenuKey  highlighted} 

{$S  Main} 
PROCEDURE  DrawWindow (window:  WindowPtr); 

{Draw  the  contents  of  an  application  window.} 

BEGIN 

SetPort(window); 
TEUpdate(windowA.portRect,  DocumentPeek(window)A.te); 

END; 

{DrawWindow} 

A  Pascal  Example of a  MulttFlnder-Aware Appllcatton 

73 


---
{ $5  Main} 
FUNCTION  Get5leep:  LONGINT; 

{Calculate  a  sleep  value  for  WaitNextEvent.  This  takes  into  account  the  thinqs 

that  Doidle  does  with  idle  time.} 

VAR 

BEGIN 

sleep 
frontmost 

LONG INT; 
WindowPtr; 

sleep  :- MAXLONGINT; 
IF  NOT  qinBackqround  THEN  BEGIN 
frontmost  :- FrontWindow; 

{default  value  for  sleep} 
{if  you  are  in  front ••• } 
{and  the  front  window  is  yours ••• } 

IF  IsAppWindow(frontmost)  THEN  BEGIN 

WITH  Document Peek ( frontmost) ". te""  DO 
{and 
{ 
sleep  :•  GetCaretTime; 

IF  sel5tart  -

selEnd  THEN 

the  selection  is 
an  insertion  point ••• 

{you  need  to  make  the 
insertion  point  blink} 

END; 

END; 
Get5leep  ••  sleep; 

END;  {Get5leep} 

{$5  Main} 
PROCEDURE  Doidle; 

{This  is  called  whenever  you  qet  a  null  event  or  a  mouse-moved  event. 
of  necessary  periodic  actions.  For  this  program,  it  calls  TEidle.} 

It  takes  care 

VAR 

BEGIN 

frontmost 

WindowPtr; 

frontmost 
IF  IsAppWindow ( frontmost)  THEN 

:=  FrontNindow; 

TEidle(DocumentPeek(frontmost)".te); 

END; 

(Doidle} 

{ $5  Main} 
PROCEDURE  DoKeyDown (event:  EventRecord); 

{This  is  called  for  any  keyDown  or  autoKey  events,  except  when  the  Command  key  is 

held  down.  It  looks  at  the  frontmost  window  to  decide  what  to  do  with  the  key 
pressed.} 

VAR 

frontmost 
key 

WindowPtr; 
CHAR; 

74 

Appendix B:  A  Pascal  Example of a  MultlRnder·Aware Application 


---
BEGIN 

:=  FrontWindow; 
frontmost 
IF  IsAppWindow(frontmost)  THEN  BEGIN 

key  :•  CHR(BAnd(event.messaqe,  charCodeMask)); 
TEKey(key,  DocumentPeek(frontmost)A.te); 

END; 

END; 

{DoKeyDown} 

{$S  Main} 
PROCEDURE  DoContentClick (window:  WindowPtr i  event:  EventRecord); 

{This  is  called  when  a  mouseDown  occurs  in  the  content  of  a  window.} 

VAR 

BEGIN 

mouse 
shiftDown 

Point; 
BOOLEAN; 

IF  IsAppWindow(window)  THEN  BEGIN 

SetPort(window); 
mouse:=  event.where; 
GlobalToLocal (mouse); 
shiftDown  :•  BAnd(event.modifiers,  shiftKey)  <>  O; 
{extend  i f  Shift  is  down} 

{qet  the  click  position} 
{convert  to  local  coordinates} 

TEClick(mouse,  shiftDown,  DocumentPeek(window)~.te); 

END; 

END; 

{DoContentClick} 

{$5  Main} 
PROCEDURE  DoActivate {window:  WindowPtrl; 

{This  is  called  when  a  window  is  activated.} 

BEGIN 

IF  IsAppWindow(window)  THEN 

TEActivate(DocumentPeek(window)~.te); 

END; 

{DoActivate} 

{$5  Main} 
PROCEDURE  DoDeactivate (window:  WindowPtr); 

{This  is  called  when  a  window  is  deactivated.} 

BEGIN 

IF  IsAppWindow(window)  THEN 

TEDeactivate(DocumentPeek(window)A.te); 

END; 

{DoDeactivate} 

A Pascal Example of a  MultlFlnder-Aware Appllcatlon 

75 


---
( $5  Main} 
PROCEDURE  DoUpdate (window:  WindowPtr); 

(This  is  called  when  an  update  event  is  received  for  a  window. 

It.  calls  DrawWindow 

to  draw  the  contents  of  an  application  window.} 

BEGIN 

IF  IsAppWindow(window)  THEN  BEGIN 
BeginUpdate(window); 
IF  NOT  EmptyRgn(window".visRgn)  THEN 

Drawwindow(window); 

EndUpdate(window); 

END; 

END;  {DoUpdate} 

{this  sets  up  the  visRgn} 
{draw  if  updating  needs  to  be 

done} 

{$5  Main} 
PROCEDURE  AdjustCursor (mouse:  Point;  region:  RgnHandle); 

{Change  the  cursor• s  shape,  depending  on  its  position.  This  also  calculates  a  region 
.. that  includes  the  cursor  for  Wait Next Event. } 

VAR 

BEGIN 

frontmost 
arrowRgn 
iBeamRgn 
iBeamRect 

WindowPtr; 
RgnHandle; 
RgnHandle; 
Rect; 

frontmost 

: •  FrontWindow; 

{  only  adjust  the  cursor  when  you  are  in  front} 

IF 

(NOT  ginBackground)  AND 

(NOT  IsDAWindow(frontmost)}  THEN  BEGIN 

{calculate  regions  for  different  cursor  shapes} 
arrowRgn 
iBeamRgn 

: =  NewRgn; 
:=  NewRgn; 

{start  arrowRgn  wide  open} 
5etRectRgn (arrowRgn,  -32768,  -32768,  32767,  32767); 

{calculate  iBeamRqn} 
IF  IsAppWindow(frontmost)  THEN  BEGIN 

iBeamRect 
5etPort(frontmostli 

: •  DocumentPeek ( frontmost l ". te"". viewRect; 

{make  a  global  version  of  the  viewRect} 

WITH 

iBeamRect  DO  BEGIN 

LocalToGlobal(topLeft); 
LocalToGlobal(botRight); 

END; 
RectRgn(iBeamRqn, 

iBeamRect); 

END; 

{subtract  other  regions  from  arrowRgn} 
iBeamRqn,  arrowRgn); 
DiffRgn (arrowRgn, 

76 

Appendix B:  A  Pascal  Example of a  MuHIRnder-Aware Appllcation 


---
( 

{change  the  cursor  and  the  region  parameter} 
IF  PtinRgn (mouse, 

iBeamRgn)  THEN  BEGIN 

SetCursor(GetCursor(iBeamCursor)AA); 
CopyRgn(iBeamRgn, 

region); 

END  ELSE  BEGIN 

SetCursor(arrow); 
CopyRgn(arrowRgn, 

region); 

END; 

{get  rid  of  local  regions} 
DisposeRgn(arrowRgn); 
DisposeRgn(iBeamRgn); 

END; 

END;  {AdjustCursor} 

{SS  Main) 
PROCEDURE  DoEvent (event:  EventRecordl ; 

{Do  the  right  thing  for  an  event.  Determine  what  kind  of  event  it  is,  and  call 

the  appropriate  routines.} 

VAR 

BEGIN 

part 
window 
key 

INTEGER; 
WindowPtr; 
CHAR; 

CASE  event. what  OF 

nullEvent: 

Do Idle; 

mouseDown:  BEGIN 
part 
CASE  part  OF 

: =  FindWindow (event. where,  window); 

inMenuBar:  BEGIN 

AdjustMenus; 
DoMenuCommand{MenuSelect(event.where)); 

END; 
in Sys Window: 

SystemClick (event,  window); 

inContent: 

IF  window  <>  FrontWindow  THEN  BEGIN 
SelectWindow(window); 

{DoEvent(event);} 
{use  this  line  for  "do  first  click") 

END  ELSE 

DoContentClick(window,  event); 

inDrag: 

DragWindow(window,  event.where,  screenBits.bounds); 

inGoAway: 

IF  TrackGoAway(window,  event.where)  THEN 

A Pascal  Example  of a  MultlFlnder-Aware  Application 

77 


---
END; 

END; 
keyDown,  autoKey:  BEGIN 

DoCloseWindow(window); 

key  :- CHR(BAnd(event.message,  charCodeMask)); 
IF  BAnd(event.modifiers,  cmdKey)  <>  0  THEN  BEGIN 
{Command  key  down} 

IF  event. what  -

keyDown  THEN  BEGIN 

AdjustMenus; 

{enable/disable/check  menu 

DoMenuCommand(MenuKey(key)); 

items  properly} 

END; 

END  ELSE 

DoKeyDown(event); 

END; 
activateEvt:  BEGIN 
window 
IF  BAnd(event .modifiers,  activeFlag)  <>  O  THEN 

:=  WindowPtr(event.message); 

DoActivate(window) 

DoDeactivate(window); 

ELSE 

END; 
updateEvt: 

DoUpdate(WindowPtr(event.message)); 

osEvent: 

CASE  BSR(event.message,  24)  OF 
mouseMovedMessage: 

{high  byte  of  message} 

Doidle; 

{mouse  moved  is  also  an  idle  event} 

suspendResumeMessage:  BEGIN 

:=  FrontWindow; 
window 
IF  BAnd (event .message, 

resumeMask)  <>  O  THEN  BEGIN 

ginBackground  :=  FALSE; 
DoActivate(window); 
{  Have  to  treat  suspend/resume 
{  as  deactivate/activate  as  well 

END  ELSE  BEGIN 

ginBackground  :=  TRUE; 
DoDeactivate(window); 

END; 

END; 

END; 

END; 

{DoEvent} 

END; 

{ $S  Main} 
PROCEDURE  EventLoop; 

{Get  events  forever,  and  handle  them  by  calling  DoEvent. 

Also  call  AdjustCursor 

each  time  through  the  loop.} 

VAR 

cursorRgn 

RgnHandle; 

78 

Appendix  B:  A  Pascal  Example  of a  MulttAnder-Aware Appllcatton 


---
ignoreResult 
event 

BOOLEAN; 
EventRecord; 

BEGIN 

cursorRgn 
REPEAT 

:=  NewRgn; 

IF  gHasWaitNextEvent  THEN 

ignoreResult  :•  WaitNextEvent (everyEvent,  event,  GetSleep, 

cursorRgn) 

ELSE  BEGIN 

SystemTask; 
ignoreResult  :•  GetNextEvent (everyEvent,  event); 

END; 
AdjustCursor(event.where,  cursorRgn); 
DoEvent(event); 
UNTIL  FALSE;  {loop  forever} 

END;  {EventLoop} 

PROCEDURE  _Datainit;  EXTERNAL; 

{This  routine  is  automatically  generated  by  the  MPW  Linker.  This  external  reference 

to  it  is  made  so  that  its  segment, 

'ASinit,  can  be  unloaded.} 

{ $S  Main} 
BEGIN 

UnloadSeg(@_Datainit); 
MaxApplZone; 

{note  that  _Datainit  must  not  be  in  Main! } 
{expand  the  heap  so  code  segments  load  at  the  top} 

Initialize; 
UnloadSeg(@Initialize); 

{initialize  the  program} 
{note  that  Initialize  must  not  be  in  Main!} 

Event Loop; 

{call  the  main  event  loop} 

END. 

A Pascal  Example  of a  Mu111Flnder-Aware  Appllcaflon 

79 


---

---
( 

(~ 

(~ 

Appendix  C 

Resource  Descriptions  for  the 
Example  Multifinder-Aware 
Application 

Here are the resource descriptions for the MPW Rez tool used in 
Appendixes A and B. 

Resources  for  the  MultiFinder-Aware  Sample  Application 

/*------------------------------------------------------------------------------*/ 
*/ 
I* 
*I 
/* 
I* 
*/ 
I* 
*/ 
*/ 
/* 
/* 
*/ 
/*------------------------------------------------------------------------------*/ 

Copyright  IG>  1988  Apple  Computer,  Inc. 
All  rights  reserved. 

if include  "Types.r" 

I*  these  ifdefines  correspond  to  values  in  the  Pascal  and  c  source  code 

*/ 

if define 
ifdefine 
ifdefine 

ifdefine 
#define 
tdefine 

rMenuBar 
rAboutAlert 
rDocWindow 

mApple 
mFile 
mEdit 

128 
128 
128 

128 
129 
130 

/*  application's 
/*  about  alert  *I 
/*  application's 

window 

menu  bar  */ 

/*  Apple  menu 
I*  File  menu 
/*  Edit  menu 

/*  we  use  an  MBAR  resource  to  load  all  the  menus  conveniently  *I 

resource 

'MBAR' 
mApple,  mFile,  mEdit  }; 

( rMenuBar,  preload) 

/*  three  menus  */ 

*/ 

*/ 
*/ 
*I 

81 


---
} ; 

resource 

'MENU' 

(mApple,  preload) 

mApple, 
OblllllllllllllllllllllllllllllOl, 

textMenuProc, 

I*  disable  dashed  line,  enable  About 

and  DAs  */ 

enabled,  apple, 
{ 

"About  Sample-", 

"-", 

noicon,  nokey,  nomark,  plain; 

noicon,  nokey,  nomark,  plain 

resource 

'MENU' 

(mFile,  preload) 

mFile, 
ObOOOOOOOOOOOOOOOOOOOlOOOOOOOOOOO, 

textMenuProc, 

enabled,  "File", 
{ 

"New", 

/*  enable  Quit  only,  program  enables 

others  */ 

noicon,  "N",  nomark,  plain; 

"Open", 

"-" , 

noicon,  "0",  nomark,  plain; 

noicon,  nokey,  nomark,  plain; 

"Close", 

noicon,  "W",  nomark,  plain; 

"Save", 

noicon,  "S",  nomark,  plain; 

"Save  As ... ", 

noicon,  nokey,  nomark,  plain; 

"Revert", 

noicon,  nokey,  nomark,  plain; 

"-", 

noicon,  nokey,  nomark,  plain; 

"Page  Setup-", 

noicon,  nokey,  nomark,  plain; 

"Print-", 

"-" , 

noicon,  nokey,  nomarl<,  plain; 

noicon,  nokey,  nomarl<,  plain; 

"Quit", 

noicon,  •Q 11 , 

nomarl<,  plain 

} ; 

resource 

'MENU' 

(mEdit,  preload) 

mEdit, 
ObOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO, 

textMenuProc, 

enabled,  "Edit", 

{ 

/*  disable  everythinq,  program  does 

the  enabling  *I 

82 

Appendix C:  Resource  Descriptions for fhe Example  MultlFlnder-Aware Appllcotlon 


---
( 

( 

"Undo", 

noicon,  "Z",  nomark,  plain; 

noicon,  nokey,  nomark,  plain; 

"-" , 

"Cut", 

noicon,  "X",  nomark,  plain; 

"Copy", 

noicon,  "C",  nomark,  plain; 

"Paste", 

noicon,  "V",  nomark,  plain; 

"Clear", 

noicon,  nokey,  nomark,  plain 

) ; 

I*  this  ALRT  and  DITL  are  used  as  an  About  screen  */ 
resource 

(rAboutAlert)  { 

'ALRT' 

{40,  20,  160,  292},  rAboutAlert, 

OK,  visible, 
OK,  visible, 
OK,  visible, 
OK,  visible, 

silent; 
silent; 
silent; 
silent 

} ; 

} ; 

resource 

'DITL' 

(rAboutAlert) 

{88,  180,  108,  260), 
Button  { 

enabled, 

"OK" 

} ; 
(8,  8,  24,  214}, 
StaticText 

disabled, 

"MultiFinder-Aware  Application" 

} ; 
{32,  8,  48,  237}, 
StaticText 

disabled, 

"Copyright  @  1988  Apple  Computer" 

} ; 
(56,  8,  72,  136), 
StaticText  { 

disabled, 

"Brought  to  you  by:" 

} ; 
(80,  24,  112,  167}, 
StaticText 

disabled, 

"Macintosh  Developer  Technical  Support" 

}; 

resource 

'WIND' 

(rDocWindow) 

{64,  60,  314,  460), 
noGrowDocProc, 

invisible,  goAway,  OxO,  "untitled" 

} ; 

Resource  Descriptions  for  the  Example  MultlFlnder-Aware  Appllcatlon 

83 


---
I*  put  the  latest  SIZE  template  here  to  rez  with  MPW  2. O  * / 

type 

'SIZE' 

boolean 

boolean 

boolean 

boolean 

boolean 

boolean 

boolean 

dontSaveScreen, 
savescreen; 
iqnoreSuspendResumeEvents, 
acceptSuspendResumeEvents; 
enableOptionSwitch, 
disableOptionSwitch; 
cannotBackqround, 
canBackqround; 
notMultiFinderAware, 
multiFinderAware; 
notOnlyBackqround, 
onlyBackqround; 
dontGetFrontClicks, 
qetFrontClicks; 

unsiqned  bitstrinq[9]  =  O; 
unsiqned  longint; 
unsiqned  lonqint; 

/*  preferred  memory  size  in  bytes  */ 
/*  minimum  memory  size  in  bytes  */ 

l; 

/*  iqnore  the  warninq  caused  by  redefininq  SIZE  */ 

/*  here  is  the  quintessential  MultiFinder  friendliness  device,  the  SIZE  resource  */ 
resource 

'SIZE' 

(-1)  { 
dontSaveScreen, 
acceptSuspendResumeEvents, 
enableOptionSwitch, 
canBackqround, 

/*  You  can  backqround,  althouqh  not 
/* 
*/ 
currently;  your  sleep  value 
I*  quarantees  that  you  don •t  hog  the 
/*  Macintosh  while  you  are  in  the 
I* 

backqround. 

/*  This  says  that  you  do  your  own 
/*  activate/deactivate.  MultiFinder 
/* 

does  not  trick  the  application. 

*I 

*I 
*/ 
*I 

*/ 
*/ 
*I 

/*  This  is  definitely  not  a  background- */ 
*I 
I*  only  application! 

/*  Chanqe  this  is  if  you  wan:t  "do  first  */ 
*I 
I* 
click"  behavior  as  in  the  Finder. 

/*  This  (preferred)  size  is  biqqer  than  */ 
/* 
/* 

the  minimum  size  so  you  can  *I 
have  more  text  and  scraps. 

*I 

/*  A  heap  dump  was  viewed  while  the 
I* 
I* 
I* 

program  was  running;  it  was  usinq 
about  27K,  so  13K  was  added  for 
stack,  text,  and  scraps. 

*I 
*/ 
*I 
*l 

multiFinderAware, 

notOnlyBackqround, 

dontGetFrontClicks, 

60  *  1024, 

40  *  1024 

}; 

84 

Appendix C:  Resource  Descriptions for 1he Example  MultlFlnder-Aware Appllcatlon 


---
Appendix  D 

The  Notification  Manager 

( 

The Notiflcatloo Manager, in System version 6.0 and later, provides the user with an 
asynchronous •notification• service.  It is especially useful for background 
applications running under MultiFinder that need to communicate with the 
user-since windows can easily be obscured by other applications.  However, the 
Notification Manager can be used by any application; it is not limited to th~ 
applications that take advantage of the new MultiFinder environment 

Each application, desk accessory, or device driver can queue any number of 
notifications.  For this reason, you should try to avoid posting multiple notifications, 
since each one will be presented separately to the user ("you have mail,• •you have 
man; ... ). 

Information descnbing each notification request is contained in the Notification 
Manager queue; you supply a pointer to a queue element describing the type of 
notification you desire.  The Notification Manager queue is a standard Macintosh 
queue, as described in the Operating System Utilities chapter of Inside Macintosh, 
Volumell. 

The Notification Manager provides a one-way communication path from the 
application to the user.  There is no path from the user to the application.  If you 
require this secondary communication link, do not use the Notification Manager.  If, 
however, the Notification Manager provides what you want, but not exactly how you 
would J.ike-6ay you wanted the application's "icon to exhibit some special effea-th.en 
you should use the Notification Manager because in the future, such features may be 
poSS1ble. 

Each entry in the Notification Manager queue is a static and nonrelocatable record of 
type N'.Mllec with the following structure: 

TYPE  NMRec  •  RECORD 

qLink: 
qType: 

QElemPtr; 
INTEGER; 

{next  queue  entry} 
{queue  type  -- ORO(nmType) 

•  8} 

85 


---
nmFlaqs: 
nmPrivate: 
nmReserved: 
nmMark: 
nmSicon: 
nmsound: 
nmStr: 
nmResp: 
nmRefCon: 

INTEGER; 
LONGINT; 
INTEGER; 
INTEGER; 
Handle; 
Handle; 
StrinqPtr; 
ProcPtr; 
LONGINT; 

{reserved} 
{reserved} 
{reserved} 
{item  to  mark  in  Apple  menu} 
{handle  to  small  icon} 
{handle  to  sound  record} 
{string  to  appear  in  alert  box} 
{pointer  to  response  routine} 
{for  application  use} 

END; 

If you want to use the Notification Manager, you must also use SysEnvirons to test the 
System version.  If the System is too old, put up an alert message to tell the user that 
System 6.0 or later is needed to run your application, and then exit gracefully. 

How a  notification happens 

When a notification is handled, one or more of the following occurs (in this order): 

1 . the mark is put next to the application (or desk accessory) in the Apple menu 

2. the icon is added to the list of icons that rotate with the Apple symbol in the menu 

bar 

3. the sound is played 

4 . the dialog box is presented, and the user dismisses it 

5. the response procedure is called 

At this point, the mark in the Apple menu and the icon rotating with the Apple symbol 
in the menu bar will remain until the notification request is removed from the queue. 
The sound and the dialog box are only presented once. 

Creating a  notification request 

To create a notification request, you must set up an NMRec with all the information 
about the notification you want: 

o  nmMark contains 0 for no mark in the Apple menu, 1 to mark the current 

application, or the refNum of a desk accessory to mark that desk accessory.  An 
application should pass 1, a desk accessory should pass is its own refNum, and a 
driver should pass 0. 

o  nmSicon contains nil for no icon in the menu bar, or a handle to a small icon to 
rotate with the Apple symbol.  (A small icon is a  16x16 bitmap, often stored in a 
SIGN resource.)  This handle does not need to be locked, but must be 
nonpurgeable. 

86 

Appendix D:  The  Notification Manager 


---
o  nmSound contains nil for no sound,-1 to use the system beep sound, or a handle to 
a sound record to be played with SndPlay.  This handle does not need to be locked, 
but it must be nonpurgeable. 

o  nmStr contains nil for no alert, or a pointer to the string to appear in the alert 

message. 

o  nmResp contains nil if you don't want to supply a response procedure, -1 to use a 

predefined procedure that removes the  request immediately after it is completed, 
or a pointer to a procedure that takes one parameter, a pointer to your queue 
element 

For example, this is how it would be declared if it were named My Response: 

PROCEDURE  MyResponse 

(nmReqPtr:  QElemPtr); 

+ No_te:  When this response procedure is called, A5 and low-memory globals are not 
set up for you.  If you need to acceM your application's globals, you should save 
your application's A5 in the nrnRefCon field as discussed below. 

Response procedures should never draw or do •user interface• things.  You should 
wait until the application or desk acceMory is brought to the front before responding to 
the user.  Some good ways to use the response procedure are to dequeue and 
deallocate your Notification Manager queue element or to set an application global 
(being careful about A5) so that the application knows when the user has been notified. 

You should probably use an nrnResp of-1 with audible and alert notifications to 
remove the notification as soon as the sound has played or the alert box has been 
dismissed.  You shouldn't use an nrnResp of-1 with an nmMark or an nmSicon, 
because the mark or icon would be removed before the user would see it  Note that an 
nmResp of -1 does not deallocate the memory block containing the queue element, it 
only removes it from the notification queue. 

. 

The nrnRefCon routine is available for your use.  One convenient way to use it is to put 
the application's A5 in this field so that the response procedure can aeceM application 
globals.  This is useful since the value of A5 is not guaranteed when the application 
calls the response procedure (see Chapter 2 for more information on the A5 world). 

Notification Manager routines 
The Notification Manager is automatically initiali7.ed each time the system starts up. 
To add a notification request to the notification queue, call NMinstall.  When your 
application no longer wants a notification to continue, it can remove the request by 
calling NMRemove.  NMinstall and NMRemove do not move or purge memory, and 
can be called from completion routines or interrupt handlers, as well as from the main 
body of an application and the response procedure of a notification request 

Notlflcatton  Manager routines 

87 

( 

(

'> 

/ 


---
NMlnstall 

NMinstall adds the notification request specified by nmReqPtr to the notification 
queue.  Here are the interface, glue, and result codes for NMinstall: 

•  FUNCTION  NMinstall 

(nmReqPtr:  QElemPtr) 

:  OSErr; 

INLINE  $205F,  $A05E,  $3E80; 

Trap macro  _NMinstall ($A05E) 

On entry 

AO:  theNMRec (pointer) 

On exit 

DO:  result code (word) 

NMinstall returns one of the result codes listed below. 

Result codes: noErr 

nmTypErr (-299) 

No error 
qType field isn't ORD(nmType) 

+ Note: qType must be set to ORD(nmType). 

NM Remove 

NMRemove removes the notification identified by nmReqPtr from  the notification 
queue.  Here are the interface, glue, and result codes for NMRemove: 

•  FUNCTION  NMRemove 

(nmReqPtr:  QElemPtr) 

:  OSErr; 

INLINE  $205F,  $A05F,  $3E80; 

Trap macro  _NMinstall ($A05F) 

On entry 

AO:  theNMRec (pointer) 

On exit 

DO:  result code (word) 

NMRemove returns one of the result codes listed below. 

Result codes: noErr 

qErr 

No error 
Not in queue 

nmTypErr (-299) 

qType field isn't ORD(nmType) 

+ Note: qType must be set to ORD(nmType). 

88 

Appendix 0: The  Notification Manager 


---
( 

Appendix  E 

A  Summary  of  the  MultiFinder 
Traps 

1bis appendix contains a summary listing of the new MultiFinder traps. 

( 

Temporary memory allocation calls 
Here are the new MultiFinder temporary memory allocation calls. 

•  FUNCTION  MFFreeMem 

:  LONGINT 

INLINE  $3F3C,  $0018,  $A88F 

MFFreeMem returns the total amount of free memory available for temporary 
allocation, in byres. 

•  FUNCTION  MFMaxMem(VAR  grow:Size) 

:  Size 

INLINE  $3F3C,  $0015,  $A88F 

MFMaxMem compacts the MultiFinder heap zone, purges all purgeable blocks, and 
returns the number ofbyteS of the largest contiguous free block for temporary 
allocation. 

•  FUNCTION  MFTempNewHandle (log icalSize: Size; VAR 

resultCode:OSErrl :Handle 

INLINE  $3F3C,  $0010,  $A88F 

MfTempNewHandle attempts to allocate a new relocatable block of logicalSiz.e 
byres for temporary usage and return a handle to it The new block will be unlocked 
and unpurgeable. If an error occurs, MFfempNewHandle will return nil 

Result codes: noErr 

memFullErr 

No error 
Not enough room 

89 


---
•  FUNCTION  MFTopMem:  Ptr 

INLINE  $3F3C,  $0016,  $A88F 

MFfopMem returns a pointer to the top of your application's memory partition. 

+ Note: Do not use this call to calculate the size of your application's memory 

partition. 

•  PROCEDURE  MFTempDisposHandle (h: Handle;  VAR  result Code: OSErr I 

INLINE  $3F3C,  $0020,  $A88F 

MFfempDisposHandle releases the memory occupied by the relocatable block 
whose handle is h. 

Result codes: noErr 

memWZErr 

No error 
Attempt to operate on a free block 

•  PROCEDURE  MFTempHLock (h: Handle;  VAR  resultCode: OS Err) 

INLINE  $3F3C,  $001E,  $A88F 

MFfempHLock locks the specified relocatable block, preventing it from being 
moved within the MultiFinder heap zone. 

Result codes: noErr 

nilHandleErr 
memWZErr 

No error 
Nil master pointer 
Attempt to operate on a free block 

•  PROCEDURE  MFTempHUnlock (h: Handle;  VAR  resultCode: OS Err) 

INLINE  $3F3C,  $001F,  $A88F 

MFfempHUnlock unlocks the specified relocatable block, allowing it to move. 

Result codes: noErr 

nilHandleErr 
memWZErr 

No error 
Nil master pointer 
Attempt to operate on a free block 

WaitNextEvent 
The interface for WaitNextEvent is: 

Function  WaitNextEvent 

(eventMask 

VAR  theEvent 
sleep 
mouseRqn 

INTEGER; 
EventRecord; 
Lonqint; 
RqnHandle  ) 

Tick  Units 

:  BOOLEAN; 

90 

Appendix E:  A  Summary of the MultlFinder Traps 


---
THE APPLE  PUBLISHING  SY5TEM 

Tilis Apple manual was written, 
eclited, and composed on a 
desktop publishi~ system using 
Apple  Macintosh  computers 
and Microsoft• Word.  Proof 
pages were created on the Apple 
LaserWriterGD Plus.  Final pages 
were created on the Varitype,e 
Vf600111 •  POSTSCRIPT•,  the 
LaserWriter  page-description 
language, was developed by 
Adobe Systems Incorporated. 

Text type is ITC Garamond• 
(a downloadable font clistributed 
by Adobe Systems).  Display type 
is ITC Avant Garde Gothid". 
Bullets are ITC Zapf Dingbats•. 
Some elements, such as  program 
listings,  are set in Apple Courier, 
a fixed-width font 

( 

(  .

. " \ 

/ 

C: 


---

---
