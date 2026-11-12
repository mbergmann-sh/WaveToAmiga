# Bindet die eingebundene SoX-Teilmenge (libsox: 8SVX, AIFF, MAUD + Erweiterungen) in ein Teilprojekt ein.
# Die statische Bibliothek 'w2asox' wird vom Teilprojekt soxlib/ gebaut (liegt in <build>/lib).

INCLUDEPATH += $$PWD/third_party/sox/src
LIBS += -L$$OUT_PWD/../lib -lw2asox
unix: LIBS += -lm
