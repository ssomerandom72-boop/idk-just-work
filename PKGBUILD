pkgname=chain-code
pkgver=0.1.0
pkgrel=1
pkgdesc="Arch Linux oriented coding assistant CLI"
arch=('any')
url="https://example.com/chain-code"
license=('MIT')
depends=('python' 'python-click' 'python-rich' 'python-requests' 'python-tomli-w')
makedepends=('python-build' 'python-installer' 'python-setuptools' 'python-wheel')
source=("$pkgname-$pkgver.tar.gz")
sha256sums=('SKIP')

build() {
  cd "$srcdir/$pkgname-$pkgver"
  python -m build --wheel --no-isolation
}

package() {
  cd "$srcdir/$pkgname-$pkgver"
  python -m installer --destdir="$pkgdir" dist/*.whl
}
