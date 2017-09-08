FROM debian:stretch

RUN echo "deb http://mirror.yandex.ru/debian sid main" > /etc/apt/sources.list.d/10-sid-backports.list && \
    echo "deb-src http://mirror.yandex.ru/debian sid main" >> /etc/apt/sources.list.d/10-sid-backports.list && \
    echo "deb http://mirror.yandex.ru/debian testing main" > /etc/apt/sources.list.d/20-testing-backports.list && \
    echo "deb-src http://mirror.yandex.ru/debian testing main" >> /etc/apt/sources.list.d/20-testing-backports.list && \
    echo "APT::Default-Release "stable";" > /etc/apt/apt.conf.d/10-default-release && \
    apt-get update && \
    apt-get -y --no-install-recommends install \
        clang-3.8 dpkg-dev subversion debhelper dh-exec llvm-3.8-tools locales-all build-essential sudo fakeroot \
        cmake wget && \
    useradd -m -g users builder && \
    cd /home/builder && \
    wget \
        http://mirror.yandex.ru/debian/pool/main/libc/libc%2B%2B/libc%2B%2B_3.9.1.orig.tar.bz2 \
        http://mirror.yandex.ru/debian/pool/main/libc/libc%2B%2B/libc%2B%2B_3.9.1-2.dsc \
        http://mirror.yandex.ru/debian/pool/main/libc/libc%2B%2B/libc%2B%2B_3.9.1-2.debian.tar.xz && \
    sudo -u builder dpkg-source -x libc++_3.9.1-2.dsc && \
    sudo -u builder mv libc++-3.9.1 libc++-3.8.1 && \
    cd libc++-3.8.1 && \
    sudo -u builder bash ./debian/libc++-get-orig-source.sh 3.8.1 RELEASE_381/final && \
    sudo -u builder rm -rf .pc libcxx libcxxabi && \
    sudo -u builder tar -xjf libc++_3.8.1.orig.tar.bz2 && \
    sudo -u builder mv libc++_3.8.1.orig.tar.bz2 .. && \
    sudo -u builder sed -i "s/3.9.1-2/3.8.1-1/" debian/changelog && \
    sudo -u builder sed -i "s/3.9-/3.8-/g;s/-3.9/-3.8/g" debian/control && \
    sudo -u builder sed -i 's/3.9/3.8/g' debian/rules && \
    sudo -u builder rm `find debian/patches/ -type f | grep -v libcxx-test-fix-russian-symbols.patch` && \
    sudo -u builder echo libcxx-test-fix-russian-symbols.patch > debian/patches/series && \
    sudo -u builder sed -i '/debian\/cxx\/usr\/lib\/\*\.a/d' debian/libc++-dev.install && \
    SUDO_FORCE_REMOVE=yes apt-get -y remove --purge sudo subversion && \
    apt-get -y autoremove && \
    apt-get -y clean && \
    rm -rf /var/lib/apt/lists

USER builder
WORKDIR /home/builder/libc++-3.8.1
VOLUME /home/builder/results
CMD DEB_BUILD_OPTIONS=nocheck dpkg-buildpackage && \
    cp ../*.deb /home/builder/results/
