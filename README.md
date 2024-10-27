# freebsd-daemond

## Why?

TrueNAS Core uses an outdated version of FreeBSD (13.1 at the time this program was written), which causes `/usr/sbin/daemon` to malfunction in FreeBSD jails (versions > 13.3 upon writing this). 

Just too lazy to try to fix it or migrate to another system.

For more details, refer to this [TrueNAS forum discussion](https://forums.truenas.com/t/any-idea-why-usr-sbin-daemon-is-not-starting-processes/137/).

## How?

### Compile

```sh
clang -o daemond daemond.c
cp daemond /usr/local/bin/
```

### Use

```sh
daemond <name> start|stop|restart -- command [args...]
```
