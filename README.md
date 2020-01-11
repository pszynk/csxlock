csxlock - simple X screen locker with customized color
===============================

Simple screen locker utility for X, fork of sxlock which is based on sflock, which is based on slock.
Warning: suid needed to lock console! SUID can be disabled (tty locking will not be working).

Thanks to Jakub Klinkovský and Safronov Kirll for pure code!


Features
--------

 - provides basic user feedback
 - uses PAM
 - sets DPMS timeout to 10 seconds, before exit restores original settings
 - basic RandR support (drawing centered on the primary output)
 - user colors for background and text
 - date and time (updated by keyboard press)
 - lock tty (SUID needed!)
 - display layout name of keyboard
 - will show warning about "Caps Lock" mode


Requirements
------------

 - libX11 (Xlib headers)
 - libXext (X11 extensions library, for DPMS)
 - libXrandr (RandR support)
 - PAM
 - terminus font (optional)


Installation
------------

Arch Linux users can install this package the with provided PKGBUILD file.
Package on [AUR](https://aur.archlinux.org/packages/csxlock-git/) is for
[HoskeOwl.csxlock][https://github.com/HoskeOwl/csxlock].

For manual installation just install dependencies, checkout and make:

    git clone https://github.com/pszynk/csxlock
    cd ./csxlock
    make install
    csxlock

To remove SUID make:

    chmod s-u /sbin/csxlock

Or run install as:

    make nosuidinstall
    csxlock

Running csxlock
-------------

Simply invoking the csxlock command starts the display locker with default settings.

Custom settings (`csxlock -h`):

csxlock: usage: csxlock [OPTS]...
Simple X screenlocker

    Mandatory arguments to long options are mandatory for short options too.
       -h, --help              show this help page and exit
       -v, --version           show version info and exit
       -d, --nodpms            do not handle DPMS
       -l, --hidelength        derange the password length indicator
       -u, --username=USER     user name to be displayed at the lockscreen
                                 (default: getenv(USER))
       -f, --font=XFONTDESC    use this font (expects X logical font description)
                                 (default: "-xos4-terminus-bold-r-normal--16-*")
       -p, --passchar=C        characters used to obfuscate the password
                                 (default: '*')
           --background-color=HEXCOLOR
                               background color for lockscreen in hex value
                                 (default: "#C3BfB0")
           --text-color=HEXCOLOR
                               text color for lockscreen in hex value
                                 (default: "#423638")
           --errmsg-color=HEXCOLOR
                               message color for autentification error in hex value
                             (default: "#F80009")
    
Default values of csxlock
-------------------------

Custom colors:
 - background color: `#C3BfB0`
 - text color: `#423638`
 - error message color: `#F80009`

Custom font:
 - `-xos4-terminus-bold-r-normal--16-*` (only bitmap fonts look presentable with X font protocol)

Hooking into systemd events
---------------------------

When using [systemd](http://freedesktop.org/wiki/Software/systemd/), you can use
the following service (create `/etc/systemd/system/csxlock.service`) to let the
system lock your X session on hibernation or suspend:

```ini
[Unit]
Description=Lock X session using csxlock
Before=sleep.target

[Service]
User=<username>
Environment=DISPLAY=:0
ExecStart=/usr/bin/csxlock

[Install]
WantedBy=sleep.target
```

However, this approach is useful only for single-user systems, because there is
no way to know which user is currently logged in.
Use [xss-lock](https://bitbucket.org/raymonad/xss-lock) as an alternative for
multi-user systems.
To use xss add a line "exec /usr/bin/xss-lock /usr/bin/csxlock +resetsaver &" to ~/.xinitrc
