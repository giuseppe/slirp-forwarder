slirp-forwarder
===============

A tool to create a network namespace that targets rootless containers.

SliRP emulates in userspace a TCP/IP stack.  It can be used to
circument the limitation of creating TAP/TUN devices in the host
namespace for an unprivileged user.

*slirp-forwarder* runs in the host network namespace without requiring
root privileges or a suid program to configure the network.  A TAP
device is created inside a new network namespace.  Data is shuttled
from the TAP device to the SLiRP stack running outside.

Notes
======
I've stopped working on this project as I've found that @AkihiroSuda
had already something similar so we joined our efforts in:
[slirp4netns](https://github.com/rootless-containers/slirp4netns)

Requirements
============
slirp-forwarder internally uses [libslirp](https://github.com/rd235/libslirp),
it is required for the build.

Usage
======
*slirp-forwarder* creates a new network namespace, configures a tap
device and keeps a reference to it in the specified.

```console
$ slirp-forwarder /path/to/net
```

For unprivileged users, before using *slirp-forwarder* it is first
necessary to run in a new user and mount namespace.

You can use the standard `unshare(1)` tool for doing it, or if you'd
like to get more users mapped into the namespace, you can use
[become-root](https://github.com/giuseppe/become-root).

```console
$ unshare -mr bash # start a new bash in a mount and user namespace
$ mount -t tmpfs tmpfs /var/run; mkdir -p /var/run/NetworkManager/
$ touch net; slirp-forwarder net & # keep a reference in the file net
$ nsenter --net=net dhclient -i tap0
$ nsenter --net=net route add default tap0
$ nsenter --net=net ifconfig -a
lo: flags=8<LOOPBACK>  mtu 65536
        loop  txqueuelen 1000  (Local Loopback)
        RX packets 0  bytes 0 (0.0 B)
        RX errors 0  dropped 0  overruns 0  frame 0
        TX packets 0  bytes 0 (0.0 B)
        TX errors 0  dropped 0 overruns 0  carrier 0  collisions 0

tap0: flags=67<UP,BROADCAST,RUNNING>  mtu 1500
        inet 10.0.2.15  netmask 255.255.255.0  broadcast 10.0.2.255
        inet6 fe80::e00f:e4ff:fe83:29cc  prefixlen 64  scopeid 0x20<link>
        ether e2:0f:e4:83:29:cc  txqueuelen 1000  (Ethernet)
        RX packets 2  bytes 724 (724.0 B)
        RX errors 0  dropped 0  overruns 0  frame 0
        TX packets 9  bytes 942 (942.0 B)
        TX errors 0  dropped 0 overruns 0  carrier 0  collisions 0
$ nsenter --net=net wget -O- www.gnu.org
....
```

Build
=====
After you have installed libslirp:
    
```console
$ ./autogen.sh && ./configure && make
```

TODO
====
Consider the slirp implementation in QEMU.
