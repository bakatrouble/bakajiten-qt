# Installed layout for `cmake --install` and the AppImage
# (`usr/bin/bakajiten` + desktop/icon files, looked up via
# applicationDirPath — see DictionaryWidget::artifactPath and the
# recognizer candidates). Data artifacts (*.bin) are NEVER bundled:
# they are gitignored build products resolved at runtime from
# /usr/share/bakajiten or the dev-tree CWD fallbacks.
include(GNUInstallDirs)

install(
    TARGETS bakajiten
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(
    FILES ${CMAKE_CURRENT_SOURCE_DIR}/packaging/desktop/bakajiten.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
)
install(
    FILES ${CMAKE_CURRENT_SOURCE_DIR}/packaging/desktop/bakajiten.svg
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
)
install(
    FILES ${CMAKE_CURRENT_SOURCE_DIR}/packaging/desktop/bakajiten.png
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps
)
