# Dev/test environment: Wine (32-bit) + Xvfb for running the original game headlessly,
# clang/lld + MSVC CRT/SDK (via xwin) for building the i686 hook DLL with the MSVC C++ ABI.
FROM ubuntu:24.04
ENV DEBIAN_FRONTEND=noninteractive
RUN dpkg --add-architecture i386 && apt-get update && apt-get install -y --no-install-recommends \
      wine wine32:i386 wine64 xvfb xauth x11-utils xdotool imagemagick \
      clang lld llvm cmake ninja-build make python3 gdb-multiarch \
      ca-certificates curl xz-utils cabextract unzip procps \
    && rm -rf /var/lib/apt/lists/*
ARG XWIN_VERSION=0.10.0
RUN curl -fsSL "https://github.com/Jake-Shadle/xwin/releases/download/${XWIN_VERSION}/xwin-${XWIN_VERSION}-x86_64-unknown-linux-musl.tar.gz" \
      | tar -xz -C /usr/local/bin --strip-components=1 "xwin-${XWIN_VERSION}-x86_64-unknown-linux-musl/xwin" \
    && xwin --accept-license --arch x86 splat --output /opt/xwin 2>&1 | tail -3 \
    && rm -rf /root/.xwin-cache
RUN apt-get update && apt-get install -y --no-install-recommends \
      libgl1:i386 libgl1-mesa-dri:i386 libglx-mesa0:i386 libgl1 libgl1-mesa-dri libglx-mesa0 \
      libpulse0:i386 python3-pip python3-venv \
    && rm -rf /var/lib/apt/lists/*
RUN printf 'pcm.!default { type null }\nctl.!default { type hw card 0 }\n' > /etc/asound.conf \
    && printf 'pcm.!default { type plug slave.pcm "nullout" }\npcm.nullout { type null }\n' > /etc/asound.conf
ENV WINEDEBUG=-all WINEARCH=win32 WINEPREFIX=/wine
RUN wineboot --init 2>/dev/null; wineserver -w; true
WORKDIR /work
RUN apt-get update && apt-get install -y --no-install-recommends clang-19 lld-19 llvm-19 \
    && rm -rf /var/lib/apt/lists/* \
    && ln -sf /usr/bin/clang-19 /usr/local/bin/clang-cl && ln -sf /usr/bin/lld-link-19 /usr/local/bin/lld-link \
    && for t in llvm-rc llvm-lib llvm-mt llvm-readobj llvm-objdump; do ln -sf /usr/bin/$t-19 /usr/local/bin/$t; done
