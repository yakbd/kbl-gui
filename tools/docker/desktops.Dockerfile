# KDE Plasma and Wayland compositors for tools/desktop-check.sh (the AppImage brings its own Qt).
FROM fedora:43
RUN dnf -y -q install mesa-libGL mesa-libEGL mesa-dri-drivers libxkbcommon libxkbcommon-x11 fontconfig dbus-libs dbus-daemon dbus-x11 \
      glib2 dejavu-sans-fonts xcb-util-cursor xcb-util-wm xcb-util-image xcb-util-keysyms xcb-util-renderutil \
      xorg-x11-server-Xvfb xorg-x11-xauth xwd ImageMagick weston kwin-wayland plasma-integration \
      xdg-desktop-portal xdg-desktop-portal-kde breeze-gtk breeze-icon-theme which procps-ng xwininfo >/dev/null && dnf clean all
