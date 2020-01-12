# Maintainer: Pawel Szynkiewicz (the_ham) <justhamstuff@gmail.com>

pkgname=csxlock-git
_pkgname=csxlock
pkgver=1.1.r13.gb28e92b
pkgrel=1
pkgdesc="Simple screen locker utility for X, fork of sxlock. Uses PAM authentication, suid needed if console locking enabled."
arch=('i686' 'x86_64')
url="https://github.com/pszynk/csxlock"
license=('MIT')
depends=('libxext' 'libxrandr' 'pam')
makedepends=('git')
source=("git://github.com/pszynk/csxlock.git")
md5sums=('SKIP')

pkgver() {
  cd "$_pkgname"
  ./version.sh
}

build() {
  cd "$_pkgname"
  make
}

package() {
  cd "$_pkgname"
  make DESTDIR="$pkgdir" install
  install -Dm644 LICENSE "$pkgdir/usr/share/licenses/$pkgname/LICENSE"
}
