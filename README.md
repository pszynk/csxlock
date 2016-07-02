csxlock - simple X screen locker with customized color
===============================

Simple screen locker utility for X, fork of sxlock which is based on sflock, which is based on slock.
Warning: suid needed to lock console! SUID can be disabled (tty locking will not be working).

Features
--------

 - provides basic user feedback
 - uses PAM
 - sets DPMS timeout to 10 seconds, before exit restores original settings
 - basic RandR support (drawing centered on the primary output)


Requirements
------------

 - libX11 (Xlib headers)
 - libXext (X11 extensions library, for DPMS)
 - libXrandr (RandR support)
 - PAM


Installation
------------

AUR package will be soon

For manual installation just install dependencies, checkout and make:

    git clone git://github.com/lahwaacz/sxlock.git
    cd ./sxlock
    make
    ./sxlock


Running csxlock
-------------

Simply invoking the csxlock command starts the display locker with default settings.

Custom settings:

    -f <font description>: modify the font.
    -p <password characters>: modify the characters displayed when the user enters his password. This can be a sequence of characters to create a fake password.
    -u <username>: a user name to be displayed at the lock screen.

Hooking into systemd events
---------------------------

When using [systemd](http://freedesktop.org/wiki/Software/systemd/), you can use the following service (create `/etc/systemd/system/sxlock.service`) to let the system lock your X session on hibernation or suspend:

```ini
[Unit]
Description=Lock X session using csxlock

[Service]
User=<username>
Environment=DISPLAY=:0
ExecStart=/usr/bin/sxlock

[Install]
WantedBy=sleep.target
```

However, this approach is useful only for single-user systems, because there is no way to know which user is currently logged in. Use [xss-lock](https://bitbucket.org/raymonad/xss-lock) as an alternative for multi-user systems.
