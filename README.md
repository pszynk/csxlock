csxlock - simple X screen locker with customized color
===============================

Simple screen locker utility for X, fork of sxlock which is based on sflock, which is based on slock.
Warning: suid needed to lock console! SUID can be disabled (tty locking will not be working).

Thanks to Jakub Klinkovský for pure code!


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


Requirements
------------

 - libX11 (Xlib headers)
 - libXext (X11 extensions library, for DPMS)
 - libXrandr (RandR support)
 - PAM
 - droid sans (font)


Installation
------------

Arch Linux users can install this package from the [AUR](https://aur.archlinux.org/packages/csxlock-git/).

For manual installation just install dependencies, checkout and make:

    git clone https://github.com/HoskeOwl/csxlock
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

Custom settings:

    -f <font description>: modify the font.
    -p <password characters>: modify the characters displayed when the user enters his password. This can be a sequence of characters to create a fake password.
    -u <username>: a user name to be displayed at the lock screen.
    -b <color>: a background color for lockscreen in hex valie (i.e. "#FF00FF")
    -o <color>: a text color for lockscreen in hex valie (i.e. "#FF00FF")
    -w <color>: a text color for autentification error in hex valie (i.e. "#FF00FF")

Default values of csxlock
-------------------------

Custom colors:
 - background color: "#C3BfB0"
 - text color: "#423638"
 - error text color: "#F80009"

Custom font:
 - -\*-droid sans-\*-\*-\*-\*-20-\*-100-100-\*-\*-iso8859-1

Hooking into systemd events
---------------------------

When using [systemd](http://freedesktop.org/wiki/Software/systemd/), you can use the following service (create `/etc/systemd/system/csxlock.service`) to let the system lock your X session on hibernation or suspend:

```ini
[Unit]
Description=Lock X session using csxlock

[Service]
User=<username>
Environment=DISPLAY=:0
ExecStart=/usr/bin/csxlock

[Install]
WantedBy=sleep.target
```

However, this approach is useful only for single-user systems, because there is no way to know which user is currently logged in. Use [xss-lock](https://bitbucket.org/raymonad/xss-lock) as an alternative for multi-user systems.
To use xss add a line "exec /usr/bin/xss-lock /usr/bin/csxlock +resetsaver &" to ~/.xinitrc
